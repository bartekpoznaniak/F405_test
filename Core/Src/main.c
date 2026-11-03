/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <math.h>
#include <stdio.h>

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
DAC_HandleTypeDef hdac;
DMA_HandleTypeDef hdma_dac1;
SD_HandleTypeDef hsd;
TIM_HandleTypeDef htim6;
UART_HandleTypeDef huart2;
/* USER CODE BEGIN PV */

static uint16_t dac_samples[48];
volatile FRESULT sd_mount_result = FR_NOT_READY;
volatile uint8_t sd_mount_done = 0;

#define AUDIO_BUFFER_SAMPLES 1024
#define AUDIO_HALF_SAMPLES   (AUDIO_BUFFER_SAMPLES / 2)

static uint16_t audio_buffer[AUDIO_BUFFER_SAMPLES];
static FIL wav_file;
static volatile uint8_t audio_half_ready = 0;
static volatile uint8_t audio_running = 0;
static uint32_t wav_data_bytes_left = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_DAC_Init(void);
static void MX_TIM6_Init(void);
static void MX_SDIO_SD_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */



int __io_putchar(int ch)
{
  uint8_t byte = (uint8_t)ch;
  if (HAL_UART_Transmit(&huart2, &byte, 1, 100) != HAL_OK)
  {
    return -1;
  }
  return ch;
}

static FRESULT audio_open_wav(const char *path)
{
    FRESULT res;
    UINT br;
    uint8_t header[44];

    res = f_open(&wav_file, path, FA_READ);
    if (res != FR_OK)
    {
        return res;
    }

    res = f_read(&wav_file, header, sizeof(header), &br);
    if (res != FR_OK || br != sizeof(header))
    {
        f_close(&wav_file);
        return FR_INVALID_OBJECT;
    }

    if (header[0] != 'R' || header[1] != 'I' ||
        header[2] != 'F' || header[3] != 'F' ||
        header[8] != 'W' || header[9] != 'A' ||
        header[10] != 'V' || header[11] != 'E')
    {
        f_close(&wav_file);
        return FR_INVALID_OBJECT;
    }

    uint16_t channels = (uint16_t)(header[22] | (header[23] << 8));
    uint32_t sample_rate = (uint32_t)header[24] |
                           ((uint32_t)header[25] << 8) |
                           ((uint32_t)header[26] << 16) |
                           ((uint32_t)header[27] << 24);
    uint16_t bits = (uint16_t)(header[34] | (header[35] << 8));

    printf("WAV: ch=%u rate=%lu bits=%u\r\n",
           (unsigned int)channels,
           (unsigned long)sample_rate,
           (unsigned int)bits);
    fflush(stdout);

    if (channels != 1 || bits != 16 || sample_rate != 44100)
    {
        f_close(&wav_file);
        return FR_INVALID_PARAMETER;
    }

    wav_data_bytes_left = (uint32_t)header[40] |
                          ((uint32_t)header[41] << 8) |
                          ((uint32_t)header[42] << 16) |
                          ((uint32_t)header[43] << 24);

    return FR_OK;
}

static FRESULT audio_start(const char *path)
{
    FRESULT res;
    UINT br;

    res = audio_open_wav(path);
    if (res != FR_OK)
    {
        return res;
    }

    res = f_read(&wav_file, audio_buffer,
                 AUDIO_HALF_SAMPLES * 2, &br);
    if (res != FR_OK || br != AUDIO_HALF_SAMPLES * 2)
    {
        f_close(&wav_file);
        return FR_INVALID_OBJECT;
    }

    for (uint32_t i = 0; i < br / 2U; i++)
    {
        int32_t sample = (int16_t)audio_buffer[i];
        audio_buffer[i] = (uint16_t)((sample + 32768) >> 4);
    }

    wav_data_bytes_left -= br;




    if (HAL_DAC_Start_DMA(&hdac,
                          DAC_CHANNEL_1,
                          (uint32_t *)audio_buffer,
                          AUDIO_BUFFER_SAMPLES,
                          DAC_ALIGN_12B_R) != HAL_OK)
    {
        f_close(&wav_file);
        return FR_INVALID_NAME;   /* DAC Start_DMA failed */
    }

    if (HAL_TIM_Base_Start(&htim6) != HAL_OK)
    {
        HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        f_close(&wav_file);
        return FR_INVALID_DRIVE;  /* TIM6 start failed */
    }

    audio_running = 1;
    return FR_OK;
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
  MX_DMA_Init();
  MX_DAC_Init();
  MX_TIM6_Init();
  MX_SDIO_SD_Init();
  MX_FATFS_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  printf("STM32F405 printf OK\r\n");
  fflush(stdout);

  printf("Przed f_mount\r\n");
  fflush(stdout);
  sd_mount_result = f_mount(&SDFatFS, SDPath, 1);
  sd_mount_done = 1;
  printf("SD mount: %d\r\n", (int)sd_mount_result);
  fflush(stdout);

  if (sd_mount_result == FR_OK)
  {
    FIL test_file;
    char text[128];
    UINT bytes_read = 0;
    FRESULT result;

    result = f_open(&test_file, "TEST.TXT", FA_READ);
    printf("TEST.TXT open: %d\r\n", (int)result);
    fflush(stdout);

    if (result == FR_OK)
    {
      result = f_read(&test_file, text, sizeof(text) - 1, &bytes_read);
      printf("TEST.TXT read: %d, bytes: %u\r\n",
             (int)result, (unsigned int)bytes_read);

      if (result == FR_OK)
      {
        text[bytes_read] = '\0';
        printf("Tresc: %s\r\n", text);
      }

      result = f_close(&test_file);
      printf("TEST.TXT close: %d\r\n", (int)result);
      fflush(stdout);
    }
  }

  FRESULT wav_result = audio_start("SHOT.WAV");
  htim6.Instance->ARR = 1904;
  printf("WAV start: %d\r\n", (int)wav_result);
  fflush(stdout);


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */


	  if (audio_running)
	  {
	      if (audio_half_ready != 0)
	      {
	          uint16_t *target;
	          uint32_t bytes_to_read;
	          UINT br = 0;

	          if (audio_half_ready == 1)
	          {
	              target = &audio_buffer[0];
	          }
	          else
	          {
	              target = &audio_buffer[AUDIO_HALF_SAMPLES];
	          }

	          audio_half_ready = 0;

	          bytes_to_read = AUDIO_HALF_SAMPLES * 2;

	          if (wav_data_bytes_left < bytes_to_read)
	          {
	              bytes_to_read = wav_data_bytes_left;
	          }

	          if (bytes_to_read > 0)
	          {

	        	  if (f_read(&wav_file, target,
	        	             bytes_to_read, &br) == FR_OK)
	        	  {
	        	      for (uint32_t i = 0; i < br / 2U; i++)
	        	      {
	        	          int32_t sample = (int16_t)target[i];
	        	          target[i] = (uint16_t)((sample + 32768) >> 4);
	        	      }

	        	      wav_data_bytes_left -= br;
	        	  }

//	              if (f_read(&wav_file, target,
//	                         bytes_to_read, &br) == FR_OK)
//	              {
//	                  wav_data_bytes_left -= br;
//	              }
	          }

	          if (wav_data_bytes_left == 0)
	          {
	              HAL_TIM_Base_Stop(&htim6);
	              HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
	              f_close(&wav_file);
	              audio_running = 0;

	              printf("WAV done\r\n");
	              fflush(stdout);
	          }
	      }
	  }



	  if (sd_mount_done && sd_mount_result == FR_OK)
	  {
	    /* Montowanie udane: dioda świeci stale */
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);

//		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);
//	    HAL_Delay(20);
	  }
	  else
	  {
	    /* Montowanie nieudane: dioda miga */
	    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_2);
	    HAL_Delay(200);
	  }

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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief DAC Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC_Init(void)
{

  /* USER CODE BEGIN DAC_Init 0 */

  /* USER CODE END DAC_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC_Init 1 */

  /* USER CODE END DAC_Init 1 */

  /** DAC Initialization
  */
  hdac.Instance = DAC;
  if (HAL_DAC_Init(&hdac) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC_Init 2 */

  /* USER CODE END DAC_Init 2 */

}

/**
  * @brief SDIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_SDIO_SD_Init(void)
{

  /* USER CODE BEGIN SDIO_Init 0 */

  /* USER CODE END SDIO_Init 0 */

  /* USER CODE BEGIN SDIO_Init 1 */

  /* USER CODE END SDIO_Init 1 */
  hsd.Instance = SDIO;
  hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
  hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
  hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
  hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
  hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd.Init.ClockDiv = 4;
  /* USER CODE BEGIN SDIO_Init 2 */

  /* USER CODE END SDIO_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 0;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 1749;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);

  /*Configure GPIO pin : PB2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : SD_DETECT_Pin */
  GPIO_InitStruct.Pin = SD_DETECT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SD_DETECT_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
    if (hdac->Instance == DAC)
    {
        audio_half_ready = 1;
    }
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
    if (hdac->Instance == DAC)
    {
        audio_half_ready = 2;
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
