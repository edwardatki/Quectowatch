/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32wbxx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LCD_CS_Pin GPIO_PIN_15
#define LCD_CS_GPIO_Port GPIOA
#define LCD_SCK_Pin GPIO_PIN_3
#define LCD_SCK_GPIO_Port GPIOB
#define LCD_MOSI_Pin GPIO_PIN_5
#define LCD_MOSI_GPIO_Port GPIOB
#define IMU_SDA_Pin GPIO_PIN_7
#define IMU_SDA_GPIO_Port GPIOB
#define SW_UP_Pin GPIO_PIN_14
#define SW_UP_GPIO_Port GPIOB
#define SW_UP_EXTI_IRQn EXTI15_10_IRQn
#define SW_LIGHT_Pin GPIO_PIN_3
#define SW_LIGHT_GPIO_Port GPIOH
#define SW_LIGHT_EXTI_IRQn EXTI3_IRQn
#define SW_MENU_Pin GPIO_PIN_15
#define SW_MENU_GPIO_Port GPIOB
#define SW_MENU_EXTI_IRQn EXTI15_10_IRQn
#define SW_DOWN_Pin GPIO_PIN_6
#define SW_DOWN_GPIO_Port GPIOB
#define SW_DOWN_EXTI_IRQn EXTI9_5_IRQn
#define IMU_INT1_Pin GPIO_PIN_8
#define IMU_INT1_GPIO_Port GPIOB
#define IMU_INT2_Pin GPIO_PIN_0
#define IMU_INT2_GPIO_Port GPIOC
#define SW_RESET_Pin GPIO_PIN_9
#define SW_RESET_GPIO_Port GPIOB
#define LCD_REFRESH_Pin GPIO_PIN_13
#define LCD_REFRESH_GPIO_Port GPIOC
#define FG_SDA_Pin GPIO_PIN_1
#define FG_SDA_GPIO_Port GPIOC
#define FG_INT_Pin GPIO_PIN_2
#define FG_INT_GPIO_Port GPIOC
#define FG_SCL_Pin GPIO_PIN_7
#define FG_SCL_GPIO_Port GPIOA
#define AUX_EN_Pin GPIO_PIN_3
#define AUX_EN_GPIO_Port GPIOA
#define LIGHT_EN_Pin GPIO_PIN_2
#define LIGHT_EN_GPIO_Port GPIOB
#define IMU_SCL_Pin GPIO_PIN_9
#define IMU_SCL_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
