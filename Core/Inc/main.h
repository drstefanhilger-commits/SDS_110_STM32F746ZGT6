/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"

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
#define EN_ADA_Pin GPIO_PIN_3
#define EN_ADA_GPIO_Port GPIOE
#define EN_ESP_CTRL_Pin GPIO_PIN_13
#define EN_ESP_CTRL_GPIO_Port GPIOC
#define USB2_VBUS_SENSE_Pin GPIO_PIN_14
#define USB2_VBUS_SENSE_GPIO_Port GPIOC
#define LED_RUN_Pin GPIO_PIN_2
#define LED_RUN_GPIO_Port GPIOG
#define LED_COMM_Pin GPIO_PIN_3
#define LED_COMM_GPIO_Port GPIOG
#define LED_ERROR_Pin GPIO_PIN_4
#define LED_ERROR_GPIO_Port GPIOG
#define MAG_INT_Pin GPIO_PIN_8
#define MAG_INT_GPIO_Port GPIOG

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
