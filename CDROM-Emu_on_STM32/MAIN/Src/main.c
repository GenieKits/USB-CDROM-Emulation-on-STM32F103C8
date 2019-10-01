/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_msc.h"
#include "spiflash.h"			/* for the structure "hnor". Zach Lee */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USB Mass storage Page 0x80 Inquiry Data. Zach Lee */
const uint8_t  MSC_Page80_Inquiry_Data[12] =
{
  0x05, /* DEVICE TYPE = 05 */
  0x80, /* PAGE CODE */
  0x00,
  0x08,
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '
};
/* USB Mass storage Page 0x83 Inquiry Data. Zach Lee */
const uint8_t  MSC_Page83_Inquiry_Data[4] =
{
  0x05,
  0x83,
  0x00,
  0x00
};
/* USB Mass storage Event Status Notification Response. Zach Lee */
const uint8_t MSC_Event_Status_Data[4] =
{
  0x00,
  0x00,
  0x80, /* NEA bit is 1 */
  0x00
};
/* USB Mass storage Get Configuration Response. Zach Lee */
const uint8_t MSC_Get_Configuration_Data[8] =
{
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00
};
/* USB Mass storage Read TOC Response. Zach Lee */
const uint8_t MSC_Read_TOC_Data[20] =
{
	0x00,
	0x12,
	0x01,
	0x01,
	0x00,0x14,0x01,0x00,0x00,0x00,0x02,0x00,
	0x00,0x14,0xaa,0x00,0x00,0x00,0x00,0x00
};
/* USB Mass storage Read Disc Information Response. Zach Lee */
const uint8_t MSC_Read_Disc_Info_Data[2] =
{
  0x00,
  0x00
};

/**
  * "MAIN_ProcessCmd" is a SCSI Command Filter.
  *
  * If the command can be proccessed here, the filter will return a zero
  * when it finishes the command. It will return a -1 if the command needs
  * to be processed by the func "SCSI_ProcessCmd".
  */
int8_t MAIN_ProcessCmd(USBD_HandleTypeDef *pdev, uint8_t lun, uint8_t *cmd)
{
  USBD_MSC_BOT_HandleTypeDef  *hmsc = (USBD_MSC_BOT_HandleTypeDef *)pdev->pClassData;
  uint32_t expectLength, realLength;

  switch (cmd[0])
  {
  case 0x12:  /* INQUIRY */
    if (cmd[1] & 0x01U) /* Evpd is set */
    {
      if (cmd[2] == 0x00)
      {
        return -1;  /* Pass this request to the func "SCSI_ProcessCmd" */
      }
      else
      if (cmd[2] == 0x80)
      {
        expectLength = hmsc->cbw.dDataLength; realLength = sizeof(MSC_Page80_Inquiry_Data);
        hmsc->bot_data_length = expectLength < realLength? expectLength:realLength;

        while(realLength)
        {
          realLength--;
          hmsc->bot_data[realLength] = MSC_Page80_Inquiry_Data[realLength];
        }
        return 0;
      }
      else
      if (cmd[2] == 0x83)
      {
        expectLength = hmsc->cbw.dDataLength; realLength = sizeof(MSC_Page83_Inquiry_Data);
        hmsc->bot_data_length = expectLength < realLength? expectLength:realLength;

        while(realLength)
        {
          realLength--;
          hmsc->bot_data[realLength] = MSC_Page83_Inquiry_Data[realLength];
        }
        return 0;
      }
    }
    return -1;  /* Let the func "SCSI_ProcessCmd" to return the standard inquiry response. */
  case 0x43:  /* READ TOC/PMA/ATIP */
    if (((cmd[1] & 0x02) == 2) || /* MSF == 1 */
        ((cmd[1] & 0x02) == 0))   /* MSF == 0 */
    {
      expectLength = hmsc->cbw.dDataLength; realLength = sizeof(MSC_Read_TOC_Data);
      hmsc->bot_data_length = expectLength < realLength? expectLength:realLength;

      while(realLength)
      {
        realLength--;
        hmsc->bot_data[realLength] = MSC_Read_TOC_Data[realLength];
      }
    }
    return 0;
  case 0x46:  /* GET CONFIGURATION */
    if ((cmd[1] & 0x03) == 0) /* RT == 00b */
    {
      expectLength = hmsc->cbw.dDataLength; realLength = sizeof(MSC_Get_Configuration_Data);
      hmsc->bot_data_length = expectLength < realLength? expectLength:realLength;

      while(realLength)
      {
        realLength--;
        hmsc->bot_data[realLength] = MSC_Get_Configuration_Data[realLength];
      }
    }
    else
    {
      return -1;
    }
    return 0;
  case 0x4A:  /* GET EVENT/STATUS NOTIFICATION */
    if (cmd[1] & 0x01)  /* IMMED bit is set */
    {
      expectLength = hmsc->cbw.dDataLength; realLength = sizeof(MSC_Event_Status_Data);
      hmsc->bot_data_length = expectLength < realLength? expectLength:realLength;

      while(realLength)
      {
        realLength--;
        hmsc->bot_data[realLength] = MSC_Event_Status_Data[realLength];
      }
    }
    else
    {
      return -1;
    }
    return 0;
  case 0x51:  /* READ DISC INFORMATION */
    {
      expectLength = hmsc->cbw.dDataLength; realLength = sizeof(MSC_Read_Disc_Info_Data);
      hmsc->bot_data_length = expectLength < realLength? expectLength:realLength;

      while(realLength)
      {
        realLength--;
        hmsc->bot_data[realLength] = MSC_Read_Disc_Info_Data[realLength];
      }
    }
    return 0;
  /* Start to process Custom SCSI requests */
  case 0xFF:
    if (hmsc->cbw.bmFlags&0x80)
    {
      /* Get the info of the NOR flash chip */
      expectLength = hmsc->cbw.dDataLength; realLength = sizeof(hnor);
      hmsc->bot_data_length = expectLength < realLength? expectLength:realLength;

      while(realLength--)
      {
        hmsc->bot_data[realLength] = ((uint8_t *)&hnor)[realLength];
      }
    }
    else
    {
      /* Write some data to the device */
    }
    return 0;
  case 0xFE:  /* Read/Write a sector of the USB disc */
    if (hmsc->cbw.bmFlags&0x80)
    {
      /**
        * Read a sector and send the data to the host.
        * We make a fake READ(10) command and
        * pass it to the func "SCSI_ProcessCmd"
        */
      cmd[0] = 0x28;  /* READ(10) */
    }
    else
    {
      /**
        * Write a sector
        * We make a fake WRITE(10) command and
        * pass it to the func "SCSI_ProcessCmd"
        */
      cmd[0] = 0x2A;  /* WRITE(10) */
    }
    /* return -1, the fack cbw.CB we made will continue to be processed. */
    return -1;
  default:
    /**
      * These commands needs to be processed by the func "SCSI_ProcessCmd".
      * Just return a -1.
      */
    return -1;
  }

  return -1;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */


  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */
}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);

  /*Configure GPIO pin : PB12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
