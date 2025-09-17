/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.h
  * @brief   This file contains all the function prototypes for
  *          the gpio.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#ifndef __GPIO_H__
#define __GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */
#define HMI_TX_Pin GPIO_PIN_0
#define HMI_TX_GPIO_Port GPIOA
#define HMI_RX_Pin GPIO_PIN_1
#define HMI_RX_GPIO_Port GPIOA
#define HMI_DE_RE_Pin GPIO_PIN_8
#define HMI_DE_RE_GPIO_Port GPIOE

#define DEBUG_TX_Pin GPIO_PIN_4
#define DEBUG_TX_GPIO_Port GPIOB
#define DEBUG_RX_Pin GPIO_PIN_3  // or whatever pin you assign for RX
#define DEBUG_RX_GPIO_Port GPIOB
/* USER CODE END Private defines */

void MX_GPIO_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif
#endif /*__ GPIO_H__ */

