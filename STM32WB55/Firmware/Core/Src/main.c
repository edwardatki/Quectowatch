/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

// dfu-util -d 0483:df11 -a 0 --download Firmware.bin -s 0x8000000 -R

/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "lptim.h"
#include "memorymap.h"
#include "rtc.h"
#include "spi.h"
#include "usb.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lcd.h"
#include "bq27441.h"
#include "sleep.h"
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

/* USER CODE BEGIN PV */
uint8_t up_just_pressed = 0;
uint8_t menu_just_pressed = 0;
uint8_t down_just_pressed = 0;

uint32_t up_press_start_timestamp;
uint32_t menu_press_start_timestamp;
uint32_t down_press_start_timestamp;

uint8_t should_sleep = 1;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	uint32_t current_timestamp = HAL_GetTick();

	if (GPIO_Pin == GPIO_PIN_14) { // UP
		if ((current_timestamp - up_press_start_timestamp) > 200) {
			up_just_pressed = 1;
			up_press_start_timestamp = current_timestamp;
		}
	} else if(GPIO_Pin == GPIO_PIN_15) { // MENU
		if ((current_timestamp - menu_press_start_timestamp) > 200) {
			menu_just_pressed = 1;
			menu_press_start_timestamp = current_timestamp;
		}
	} else if (GPIO_Pin == GPIO_PIN_6) { // DOWN
		if ((current_timestamp - down_press_start_timestamp) > 200) {
			down_just_pressed = 1;
			down_press_start_timestamp = current_timestamp;
		}
	} else if (GPIO_Pin == GPIO_PIN_3) { // LIGHT
		if (HAL_GPIO_ReadPin(SW_LIGHT_GPIO_Port, SW_LIGHT_Pin)) {
			HAL_LPTIM_PWM_Start(&hlptim1, 100, 50);
		} else {
			HAL_LPTIM_PWM_Stop(&hlptim1);
		}
	}
}

static int16_t BQ27441_i2cWriteBytes(uint8_t DevAddress, uint8_t subAddress, uint8_t * src, uint8_t count)
{
    if (HAL_I2C_Mem_Write(&hi2c3, (uint16_t)(DevAddress << 1), subAddress, 1, src, count, 50) == HAL_OK)
        return true;
    else
        return false;
}

static int16_t BQ27441_i2cReadBytes(uint8_t DevAddress, uint8_t subAddress, uint8_t * dest, uint8_t count)
{
    if (HAL_I2C_Mem_Read(&hi2c3, (uint16_t)(DevAddress << 1), subAddress, 1, dest, count, 50) == HAL_OK)
        return true;
    else
        return false;
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

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */
  sleep_init();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_LPTIM1_Init();
  MX_RTC_Init();
  MX_I2C1_Init();
  MX_I2C3_Init();
  MX_SPI1_Init();
  MX_USB_PCD_Init();
  /* USER CODE BEGIN 2 */
  HAL_RTCEx_SetSmoothCalib(&hrtc, RTC_SMOOTHCALIB_PERIOD_32SEC, RTC_SMOOTHCALIB_PLUSPULSES_RESET, 160);

  static BQ27441_ctx_t BQ27441 = {
		  .BQ27441_i2c_address = BQ72441_I2C_ADDRESS, // i2c device address, if you have another address change this
		  .read_reg = BQ27441_i2cReadBytes,           // i2c read callback
		  .write_reg = BQ27441_i2cWriteBytes,         // i2c write callback
  };
  BQ27441_init(&BQ27441);
//  BQ27441_Full_Reset();
  BQ27441_enterConfig(1);
  BQ27441_setCapacity(500);
  BQ27441_setTerminateVoltageMin(3000);
  BQ27441_exitConfig(1);

  lcd_init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	uint32_t current_timestamp = HAL_GetTick();

	static uint8_t editing = 0;

	static bool just_switched = false;
	if (!HAL_GPIO_ReadPin(SW_MENU_GPIO_Port, SW_MENU_Pin) && ((current_timestamp - menu_press_start_timestamp) > 3000)) {
		if (!just_switched) {
			should_sleep = !should_sleep;
			editing = 0;
			just_switched = true;
		}
	} else {
		just_switched = false;
	}

	if (should_sleep) {
		up_just_pressed = false;
		menu_just_pressed = false;
		down_just_pressed = false;

		lcd_update(-1);
		enter_stop(200);
	} else {

		RTC_TimeTypeDef new_time = {0};
		RTC_DateTypeDef new_date = {0};
		HAL_RTC_GetTime(&hrtc, &new_time, RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc, &new_date, RTC_FORMAT_BIN);

		 if (up_just_pressed) {
			 if (editing == 0) new_time.Hours = (uint16_t)((uint16_t)new_time.Hours + 1) % 24;
			 else if (editing == 1) new_time.Minutes = (uint16_t)((uint16_t)new_time.Minutes + 1) % 60;
			 else if (editing == 2) new_time.Seconds = (uint16_t)((uint16_t)new_time.Seconds + 1) % 60;
			 else if (editing == 3) new_date.WeekDay = (uint16_t)((uint16_t)new_date.WeekDay + 1)% 6;
			 else if (editing == 4) new_date.Date = (uint16_t)((uint16_t)new_date.Date % 31) + 1;
			 else if (editing == 5) new_date.Month = (uint16_t)((uint16_t)new_date.Month % 12) + 1;
			 else if (editing == 6) new_date.Year = (uint16_t)((uint16_t)new_date.Year + 1) % 100;

			up_just_pressed = false;
		 }

		 if (down_just_pressed) {
			 if (editing == 0) new_time.Hours = (uint16_t)((uint16_t)new_time.Hours - 1) % 24;
			 else if (editing == 1) new_time.Minutes = (uint16_t)((uint16_t)new_time.Minutes - 1) % 60;
			 else if (editing == 2) new_time.Seconds = (uint16_t)((uint16_t)new_time.Seconds - 1) % 60;
			 else if (editing == 3) new_date.WeekDay = (uint16_t)((uint16_t)new_date.WeekDay - 1) % 6;
			 else if (editing == 4) new_date.Date = (uint16_t)((uint16_t)new_date.Date - 1) % 32;
			 else if (editing == 5) new_date.Month = (uint16_t)((uint16_t)new_date.Month - 1) % 13;
			 else if (editing == 6) new_date.Year = (uint16_t)((uint16_t)new_date.Year - 1) % 100;

			 down_just_pressed = false;
		 }

		 if (menu_just_pressed) {
			 editing = (editing + 1) % 7;
			 menu_just_pressed = false;
		 }
		 HAL_RTC_SetTime(&hrtc, &new_time, RTC_FORMAT_BIN);
		 HAL_RTC_SetDate(&hrtc, &new_date, RTC_FORMAT_BIN);

		 lcd_update(editing);
	}
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

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_MEDIUMHIGH);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI1
                              |RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV8;
  RCC_OscInitStruct.PLL.PLLN = 32;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK4|RCC_CLOCKTYPE_HCLK2
                              |RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK2Divider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.AHBCLK4Divider = RCC_SYSCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SMPS;
  PeriphClkInitStruct.SmpsClockSelection = RCC_SMPSCLKSOURCE_HSE;
  PeriphClkInitStruct.SmpsDivSelection = RCC_SMPSCLKDIV_RANGE1;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN Smps */

  /* USER CODE END Smps */
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
  __disable_irq();
  while (1)
  {
  }
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
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
