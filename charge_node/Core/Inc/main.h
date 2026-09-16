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
#include "stm32f0xx_hal.h"

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
#define BQ_nQON_Pin GPIO_PIN_6
#define BQ_nQON_GPIO_Port GPIOA
#define BQ_nCE_Pin GPIO_PIN_1
#define BQ_nCE_GPIO_Port GPIOB
#define BQ_nINT_Pin GPIO_PIN_2
#define BQ_nINT_GPIO_Port GPIOB
#define BQ_nINT_EXTI_IRQn EXTI2_3_IRQn
#define BQ_STAT_Pin GPIO_PIN_12
#define BQ_STAT_GPIO_Port GPIOB
#define ADDR0_Pin GPIO_PIN_3
#define ADDR0_GPIO_Port GPIOB
#define ADDR1_Pin GPIO_PIN_4
#define ADDR1_GPIO_Port GPIOB
#define ADDR2_Pin GPIO_PIN_5
#define ADDR2_GPIO_Port GPIOB
#define ADDR3_Pin GPIO_PIN_8
#define ADDR3_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
