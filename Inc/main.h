/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
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
// Remove or comment out this line to avoid redefinition
// #define U485_DE_RE_Pin GPIO_PIN_5
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define Q0_2_Pin GPIO_PIN_2
#define Q0_2_GPIO_Port GPIOE
#define Q0_4_Pin GPIO_PIN_3
#define Q0_4_GPIO_Port GPIOE
#define Q0_5_Pin GPIO_PIN_4
#define Q0_5_GPIO_Port GPIOE
#define Q0_0_Pin GPIO_PIN_5
#define Q0_0_GPIO_Port GPIOE
#define Q0_6_Pin GPIO_PIN_6
#define Q0_6_GPIO_Port GPIOE
#define RUN_LED_Pin GPIO_PIN_13
#define RUN_LED_GPIO_Port GPIOC
#define Q1_0_Pin GPIO_PIN_0
#define Q1_0_GPIO_Port GPIOH
#define Q1_1_Pin GPIO_PIN_1
#define Q1_1_GPIO_Port GPIOH
#define Q1_2_Pin GPIO_PIN_0
#define Q1_2_GPIO_Port GPIOC
#define I1_6_Pin GPIO_PIN_2
#define I1_6_GPIO_Port GPIOC
#define I1_7_Pin GPIO_PIN_3
#define I1_7_GPIO_Port GPIOC
#define I0_0_Pin GPIO_PIN_0
#define I0_0_GPIO_Port GPIOA
#define I0_1_Pin GPIO_PIN_1
#define I0_1_GPIO_Port GPIOA
#define I0_4_Pin GPIO_PIN_4
#define I0_4_GPIO_Port GPIOC
#define I0_5_Pin GPIO_PIN_5
#define I0_5_GPIO_Port GPIOC
#define ERR_LED_Pin GPIO_PIN_2
#define ERR_LED_GPIO_Port GPIOB
#define HMI_DE_RE_Pin GPIO_PIN_8
#define HMI_DE_RE_GPIO_Port GPIOE
#define W5500_CS_Pin GPIO_PIN_11
#define W5500_CS_GPIO_Port GPIOE
#define I1_2_Pin GPIO_PIN_15
#define I1_2_GPIO_Port GPIOE
#define W5500_RST_Pin GPIO_PIN_10
#define W5500_RST_GPIO_Port GPIOB
#define I1_4_Pin GPIO_PIN_11
#define I1_4_GPIO_Port GPIOB
#define F25Q_CS_Pin GPIO_PIN_12
#define F25Q_CS_GPIO_Port GPIOB
#define F25Q_SCK_Pin GPIO_PIN_13
#define F25Q_SCK_GPIO_Port GPIOB
#define F25Q_MISO_Pin GPIO_PIN_14
#define F25Q_MISO_GPIO_Port GPIOB
#define F25Q_MOSI_Pin GPIO_PIN_15
#define F25Q_MOSI_GPIO_Port GPIOB
#define I1_5_Pin GPIO_PIN_10
#define I1_5_GPIO_Port GPIOD
#define I1_0_Pin GPIO_PIN_12
#define I1_0_GPIO_Port GPIOD
#define I1_1_Pin GPIO_PIN_13
#define I1_1_GPIO_Port GPIOD
#define Q1_6_Pin GPIO_PIN_14
#define Q1_6_GPIO_Port GPIOD
#define Q1_7_Pin GPIO_PIN_15
#define Q1_7_GPIO_Port GPIOD
#define I0_2_Pin GPIO_PIN_6
#define I0_2_GPIO_Port GPIOC
#define I0_3_Pin GPIO_PIN_7
#define I0_3_GPIO_Port GPIOC
#define I0_6_Pin GPIO_PIN_8
#define I0_6_GPIO_Port GPIOA
#define I0_7_Pin GPIO_PIN_9
#define I0_7_GPIO_Port GPIOA
#define Q0_7_Pin GPIO_PIN_10
#define Q0_7_GPIO_Port GPIOA
#define Q1_5_Pin GPIO_PIN_11
#define Q1_5_GPIO_Port GPIOA
#define HMI_RX_Pin GPIO_PIN_0
#define HMI_RX_GPIO_Port GPIOD
#define HMI_TX_Pin GPIO_PIN_1
#define HMI_TX_GPIO_Port GPIOD
#define I1_3_Pin GPIO_PIN_3
#define I1_3_GPIO_Port GPIOD
#define U485_DE_RE_Pin GPIO_PIN_5
#define U485_DE_RE_GPIO_Port GPIOD
#define DEBUG_RX_Pin GPIO_PIN_3
#define DEBUG_RX_GPIO_Port GPIOB
#define DEBUG_TX_Pin GPIO_PIN_4
#define DEBUG_TX_GPIO_Port GPIOB
#define Q1_4_Pin GPIO_PIN_6
#define Q1_4_GPIO_Port GPIOB
#define STOP_LED_Pin GPIO_PIN_7
#define STOP_LED_GPIO_Port GPIOB
#define Q0_1_Pin GPIO_PIN_8
#define Q0_1_GPIO_Port GPIOB
#define Q0_3_Pin GPIO_PIN_9
#define Q0_3_GPIO_Port GPIOB
#define U485_RX_Pin GPIO_PIN_0
#define U485_RX_GPIO_Port GPIOE
#define U485_TX_Pin GPIO_PIN_1
#define U485_TX_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
