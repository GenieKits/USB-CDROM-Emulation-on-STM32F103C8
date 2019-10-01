#include "usbd_conf.h"
#include "spiflash.h"

/***************************************
 * SPI functions
 **************************************/
extern SPI_HandleTypeDef  hspi2;
#define HSPI_SDCARD       &hspi2
#define	SD_CS_PORT        GPIOB
#define SD_CS_PIN         GPIO_PIN_12
#define SPI_TIMEOUT       100

/* slave select */
static void SELECT(void)
{
	HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET);
	//HAL_Delay(1);
}

/* slave deselect */
static void DESELECT(void)
{
	HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
	//HAL_Delay(1);
}

/* SPI transmit a byte */
static void SPI_TxByte(uint8_t data)
{
	while(!__HAL_SPI_GET_FLAG(HSPI_SDCARD, SPI_FLAG_TXE));
	HAL_SPI_Transmit(HSPI_SDCARD, &data, 1, SPI_TIMEOUT);
}

/* SPI transmit buffer */
static void SPI_TxBuffer(uint8_t *buffer, uint16_t len)
{
	while(!__HAL_SPI_GET_FLAG(HSPI_SDCARD, SPI_FLAG_TXE));
	HAL_SPI_Transmit(HSPI_SDCARD, buffer, len, SPI_TIMEOUT);
}

/* SPI receive a byte */
static uint8_t SPI_RxByte(void)
{
	uint8_t data = 0xFF;

	while(!__HAL_SPI_GET_FLAG(HSPI_SDCARD, SPI_FLAG_TXE));
	HAL_SPI_TransmitReceive(HSPI_SDCARD, &data, &data, 1, SPI_TIMEOUT);

	return data;
}

/* SPI receive buffer */
static void SPI_RxBuffer(uint8_t *buffer, uint16_t len)
{
  while(!__HAL_SPI_GET_FLAG(HSPI_SDCARD, SPI_FLAG_TXE));
  HAL_SPI_TransmitReceive(HSPI_SDCARD, buffer, buffer, len, SPI_TIMEOUT);
}

NOR_HandleTypeDef hnor;

uint32_t NOR_GetChipID(void)
{
	uint8_t cmd;

  if (hnor.id != 0)
  {
    return hnor.id;
  }

	/* check standard flash chip installed */
	cmd = 0x9F;
	SELECT();
	SPI_TxByte(cmd);
	SPI_RxBuffer((uint8_t *)&hnor.id, 3);
	DESELECT();
  if (hnor.id == 0 || hnor.id == 0x00FFFFFF)
  {
    /* check AT45DB041B chip installed */
    cmd = 0xD7;
    SELECT();
    SPI_TxByte(cmd);
    hnor.id = (uint32_t)SPI_RxByte();
    DESELECT();
  }

  return hnor.id;
}

uint8_t NOR_Init(void)
{
  NOR_GetChipID();
  hnor.BLOCK = 4095;
  hnor.BUSY = 0x01;
  hnor.CMD_BlkErase = 0x20;
  hnor.CMD_GetStatus = 0x05;

  switch(hnor.id)
  {
  case AT45DB041B:
    hnor.CMD_GetStatus = 0xD7;
    hnor.BUSY = 0x80;
    break;
  case S25FL004A:
  case S25FL008A:
  case S25FL064A:
  case W25X40:
    hnor.CMD_BlkErase = 0xD8;
    hnor.BLOCK = 65535;
    break;
  }
  hnor.state = NOR_STATE_INITIAL;

  return 1;
}

uint8_t NOR_DeInit(void)
{
  return 1;
}

static uint8_t NOR_GetChipStatus(void)
{
  uint8_t status;

  SELECT();
  SPI_TxByte(hnor.CMD_GetStatus);
  status = SPI_RxByte();
  DESELECT();

	return status;
}
static void NOR_ChkStatusBusy(void)
{
  switch(hnor.id)
  {
  case AT45DB041B:
    while((NOR_GetChipStatus()&0x80) == 0){;}
    break;
  default:
    while((NOR_GetChipStatus()&0x01) == 1){;}
    break;
  }
}

static void NOR_WriteEnable(uint8_t *cmd)
{
  NOR_ChkStatusBusy();

  if (hnor.id == AT45DB041B)
  {
    SELECT();
    SPI_TxBuffer(cmd,4+256);
    DESELECT();
		NOR_ChkStatusBusy();
  }
  else
  {
    // Write Enable Command
    SELECT();
    SPI_TxByte(0x06);
    DESELECT();
    while(!(NOR_GetChipStatus()&0x02)){;}
  }
}
static void NOR_WriteDisable(uint8_t *cmd)
{
  SELECT();
  SPI_TxByte(0x04);
  DESELECT();
  return;
}

static void NOR_Unprotect(uint8_t *cmd)
{
  NOR_WriteEnable(cmd);

  if (hnor.id == SST25VF016B)
  {
		// Unprotect all memory sectors
		SELECT();
		SPI_TxByte(0x01);
		SPI_TxByte(0x00);
		DESELECT();
		while(NOR_GetChipStatus()&0xFF){;}
		NOR_WriteEnable(NULL);
    return;
  }

  if (hnor.id == AT26F004)
  {
		// Unprotect sector
		cmd[0] = 0x39;
		SELECT();
		SPI_TxBuffer(cmd, 4);
		DESELECT();
		NOR_ChkStatusBusy();
		NOR_WriteEnable(NULL);
    return;
  }

  return;
}
static void NOR_Protect(uint8_t *cmd)
{
  NOR_ChkStatusBusy();

  if (hnor.id != AT45DB041B)
  {
    NOR_WriteDisable(cmd);
  }
}

static void NOR_BlockErase(uint8_t *cmd)
{
  if (hnor.id != AT45DB041B)
  {
    cmd[0] = hnor.CMD_BlkErase;
    SELECT();
    SPI_TxBuffer(cmd, 4);
    DESELECT();
    NOR_WriteEnable(cmd);
  }
  return;
}

static void NOR_PageProgram(uint32_t PAGE, uint8_t * DATA, uint32_t len)
{
  uint8_t cmd[4+256];
  uint32_t a;

  if (hnor.id == AT45DB041B)
  {
    cmd[0] = 0x84;  // cmd[0] = 0x87;
    cmd[1] = 0x00;
    cmd[2] = 0x00;
    cmd[3] = 0x00;
    memcpy(&cmd[4], DATA, len);

    NOR_Unprotect(cmd);

		cmd[0] = 0x83;  // cmd[0] = 0x86;
		cmd[1] = (uint8_t)((PAGE>>7)&0xFF);
		cmd[2] = (uint8_t)((PAGE<<1)&0xFF);
		cmd[3] = 0;
		SELECT();
		SPI_TxBuffer(cmd, 4);
		DESELECT();

    NOR_Protect(cmd);
    return;
  }

  a = PAGE * 256;
  cmd[1] = (uint8_t)((a>>16)&0xFF);
  cmd[2] = (uint8_t)((a>> 8)&0xFF);
  cmd[3] = (uint8_t)((a    )&0xFF);

  if (hnor.id == AT26F004)
  {
    NOR_Unprotect(cmd);

    if ((a&hnor.BLOCK) == 0)
    {
      /* Erase Block */
      NOR_BlockErase(cmd);
    }

		/* Sequential byte program */
		cmd[0] = 0xAF; cmd[5] = DATA[0];
		SELECT();
		SPI_TxBuffer(cmd, 4+1);
		DESELECT();

		for(a=1; a<len; a++)
		{
			NOR_ChkStatusBusy();
			cmd[1] = DATA[a];
			SELECT();
			SPI_TxBuffer(cmd, 2);
			DESELECT();
		}

    NOR_Protect(cmd);
    return;
  }

  if (hnor.id == SST25VF016B)
  {
    NOR_Unprotect(NULL);

    if ((a&hnor.BLOCK) == 0)
    {
      /* Erase Block */
      NOR_BlockErase(cmd);
    }

		/* Sequential bytes program */
		cmd[0] = 0xAD; cmd[4] = DATA[0]; cmd[5] = DATA[1];
		SELECT();
		SPI_TxBuffer(cmd, 4+2);
		DESELECT();

		for(a=2; a<len; a+=2)
		{
			NOR_ChkStatusBusy();
			cmd[1] = DATA[a]; cmd[2] = DATA[a+1];
			SELECT();
			SPI_TxBuffer(cmd, 3);
			DESELECT();
		}

    NOR_Protect(cmd);
    return;
  }

  if (hnor.id == EN25B64)
  {
    NOR_Unprotect(NULL);

    if (a==4096 || a==8192 || a==16384 || a==32768 || (a&65535)==0)
    {
      /* Erase Block */
      NOR_BlockErase(cmd);
    }

    /* One page program */
		cmd[0] = 0x02; memcpy(&cmd[4], DATA, len);
		SELECT();
		SPI_TxBuffer(cmd, 4+len);
		DESELECT();

    NOR_Protect(cmd);
    return;
  }

  {
    /* Standard flash operation */
    NOR_Unprotect(cmd);

    if ((a&hnor.BLOCK) == 0)
    {
      /* Block Erase */
      NOR_BlockErase(cmd);
    }

    /* One page program */
    cmd[0] = 0x02; memcpy(&cmd[4], DATA, len);
    SELECT();
    SPI_TxBuffer(cmd, 4+len);
    DESELECT();

    NOR_Protect(cmd);
  }

  return;
}
uint8_t NOR_SectorBurn(uint32_t LBA, uint8_t *Buffer, uint32_t Sectors)
{
  uint32_t PAGE, Length = Sectors * MSC_MEDIA_PACKET;

  /**
    * If LBA is less than "32768/MSC_MEDIA_PACKET",
    * We discard all data because the data will be all zero.
    */
  if (LBA < 32768 / MSC_MEDIA_PACKET)
  {
    return 1;
  }

  PAGE = (LBA-32768/MSC_MEDIA_PACKET) * MSC_MEDIA_PACKET / 256;
  while(Length)
  {
    NOR_PageProgram(PAGE, Buffer, 256);
    Length -= 256; Buffer += 256; PAGE++;
  }
  return 1;
}

static void NOR_PageRead(uint32_t PAGE, uint8_t *Buffer, int Length)
{
	uint8_t cmd[8];

	NOR_ChkStatusBusy();

	if (hnor.id == AT45DB041B)
	{
		cmd[0] = 0xE8;
		cmd[1] = (uint8_t)((PAGE>>7)&0xFF);
		cmd[2] = (uint8_t)((PAGE<<1)&0xFF);
		cmd[3] = cmd[4] = cmd[5] = cmd[6] = cmd[7] = 0;
		SELECT();
		SPI_TxBuffer(cmd, 8);
		SPI_RxBuffer(Buffer, Length);
		DESELECT();
	}
	else
	{
		uint32_t a;

		a = PAGE * 256;
		cmd[0] = 0x03;
		cmd[1] = (uint8_t)((a>>16)&0xFF);
		cmd[2] = (uint8_t)((a>> 8)&0xFF);
		cmd[3] = (uint8_t)((a    )&0xFF);
		SELECT();
		SPI_TxBuffer(cmd, 4);
		SPI_RxBuffer(Buffer, Length);
		DESELECT();
	}
}
uint8_t NOR_SectorRead(uint32_t LBA, uint8_t *Buffer, uint32_t Sectors)
{
	uint32_t PAGE, Length = Sectors * MSC_MEDIA_PACKET;

	/**
	  * If lba_start is less than "32768/MSC_MEDIA_PACKET",
	  * We just return all zero to the host.
	  */
	if (LBA < 32768 / MSC_MEDIA_PACKET)
	{
	  /* Fill Buffer with 0 */
		memset(Buffer, 0, Length);
		return 1;
	}

  PAGE = (LBA-32768/MSC_MEDIA_PACKET) * MSC_MEDIA_PACKET / 256;
  while(Length)
  {
    NOR_PageRead(PAGE, Buffer, 256);
    Length -= 256; Buffer += 256; PAGE++;
  }
  return 1;
}

uint32_t NOR_GetMaxBlocks(void)
{
  uint32_t MaxCap;

  switch(hnor.id)
  {
  case AT45DB041B:
  case S25FL004A:
  case AT26F004:
  case W25X40:
    MaxCap = 512 * 1024 / MSC_MEDIA_PACKET;
    break;
  case S25FL008A:
  case W25X80:
  case W25Q80:
  case EN25F80:
  case MX25L80:
  case GD25Q80:
    MaxCap = 1024 * 1024 / MSC_MEDIA_PACKET;
    break;
  case W25X16:
  case W25Q16:
  case EN25F16:
  case MX25L16:
  case GD25Q16:
  case SST25VF016B:
    MaxCap = 2048 * 1024 / MSC_MEDIA_PACKET;
    break;
  case W25X32:
  case W25Q32:
  case EN25F32:
  case MX25L32:
    MaxCap = 4096 * 1024 / MSC_MEDIA_PACKET;
    break;
  case W25X64:
  case W25Q64:
  case S25FL064A:
  case MX25L64:
  case EN25B64:
    MaxCap = 8192 * 1024 / MSC_MEDIA_PACKET;
    break;
  default:
    /**
      * We need a tiny DB to store the parameters
      * of new flash chips.
      */
    return 0;
  }

  /**
    * The first 32KB of a CDROM disc image is all zero.
    * We can discard this 32KB. That means we increase
    * the capacity of the flash chip.
    */
  MaxCap += 32768 / MSC_MEDIA_PACKET;
  return MaxCap;
}
