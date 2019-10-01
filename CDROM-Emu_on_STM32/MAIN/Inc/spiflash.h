#ifndef _NAND_H_
#define _NAND_H_

#include "stm32f1xx_hal.h"

typedef struct __NOR_HandleTypeDef
{
  uint32_t id;
  uint16_t BLOCK;
  uint8_t CMD_GetStatus;
  uint8_t CMD_BlkErase;
  uint8_t BUSY;
  uint8_t state;
} NOR_HandleTypeDef;
extern NOR_HandleTypeDef hnor;

#define NOR_STATE_INVALID 0
#define NOR_STATE_INITIAL 1

#define	AT45DB041B        0x0000009C  // 0x9C000000
#define S25FL004A         0x00010212  // 0x12020100
#define S25FL008A         0x00010213  // 0x13020100
#define S25FL064A         0x00010216  // 0x16020100
#define AT26F004          0x001F0400  // 0x00041F00
#define SST25VF016B       0x00BF2541  // 0x4125BF00
#define W25X40            0x00EF3013  // 0x1330EF00
#define W25X80            0x00EF3014  // 0x1430EF00
#define W25X16            0x00EF3015  // 0x1530EF00
#define W25X32            0x00EF3016  // 0x1630EF00
#define W25X64            0x00EF3017  // 0x1730EF00
#define W25Q80            0x00EF4014  // 0x1440EF00
#define W25Q16            0x00EF4015  // 0x1540EF00
#define W25Q32            0x00EF4016  // 0x1640EF00
#define W25Q64            0x00EF4017  // 0x1740EF00
#define EN25F80           0x001C3114  // 0x14311C00
#define EN25F16           0x001C3115  // 0x15311C00
#define EN25F32           0x001C3116  // 0x16311C00
#define EN25B64           0x001C2017  // 0x17201C00
#define MX25L80           0x00C22014  // 0x1420C200
#define MX25L16           0x00C22015  // 0x1520C200
#define MX25L32           0x00C22016  // 0x1620C200
#define MX25L64           0x00C22017  // 0x1720C200
#define GD25Q80           0x00C84014  // 0x1440C800
#define GD25Q16           0x00C84015  // 0x1540C800

uint32_t NOR_GetChipID(void);
uint32_t NOR_GetMaxBlocks(void);
uint8_t NOR_Init(void);
uint8_t NOR_DeInit(void);
uint8_t NOR_SectorRead(uint32_t LBA, uint8_t *Buffer, uint32_t Sectors);
uint8_t NOR_SectorBurn(uint32_t LBA, uint8_t *Buffer, uint32_t Sectors);

#endif
