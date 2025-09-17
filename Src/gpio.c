/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */
#define RS485_DE_PORT    GPIOE
#define RS485_DE_PIN     GPIO_PIN_8
/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, Q0_2_Pin|Q0_4_Pin|Q0_5_Pin|Q0_0_Pin
                          |Q0_6_Pin|HMI_DE_RE_Pin|W5500_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, RUN_LED_Pin|Q1_2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOH, Q1_0_Pin|Q1_1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, ERR_LED_Pin|W5500_RST_Pin|F25Q_CS_Pin|Q1_4_Pin
                          |STOP_LED_Pin|Q0_1_Pin|Q0_3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, Q1_6_Pin|Q1_7_Pin|U485_DE_RE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, Q0_7_Pin|Q1_5_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : Q0_2_Pin Q0_4_Pin Q0_5_Pin Q0_0_Pin
                           Q0_6_Pin HMI_DE_RE_Pin W5500_CS_Pin */
  GPIO_InitStruct.Pin = Q0_2_Pin|Q0_4_Pin|Q0_5_Pin|Q0_0_Pin
                          |Q0_6_Pin|HMI_DE_RE_Pin|W5500_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : RUN_LED_Pin Q1_2_Pin */
  GPIO_InitStruct.Pin = RUN_LED_Pin|Q1_2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : Q1_0_Pin Q1_1_Pin */
  GPIO_InitStruct.Pin = Q1_0_Pin|Q1_1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pins : I1_6_Pin I1_7_Pin I0_4_Pin I0_5_Pin
                           I0_2_Pin I0_3_Pin */
  GPIO_InitStruct.Pin = I1_6_Pin|I1_7_Pin|I0_4_Pin|I0_5_Pin
                          |I0_2_Pin|I0_3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : I0_0_Pin I0_1_Pin I0_6_Pin I0_7_Pin */
  GPIO_InitStruct.Pin = I0_0_Pin|I0_1_Pin|I0_6_Pin|I0_7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : ERR_LED_Pin W5500_RST_Pin F25Q_CS_Pin Q1_4_Pin
                           STOP_LED_Pin Q0_1_Pin Q0_3_Pin */
  GPIO_InitStruct.Pin = ERR_LED_Pin|W5500_RST_Pin|F25Q_CS_Pin|Q1_4_Pin
                          |STOP_LED_Pin|Q0_1_Pin|Q0_3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : I1_2_Pin */
  GPIO_InitStruct.Pin = I1_2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(I1_2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : I1_4_Pin */
  GPIO_InitStruct.Pin = I1_4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(I1_4_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : I1_5_Pin I1_0_Pin I1_1_Pin I1_3_Pin */
  GPIO_InitStruct.Pin = I1_5_Pin|I1_0_Pin|I1_1_Pin|I1_3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : Q1_6_Pin Q1_7_Pin U485_DE_RE_Pin */
  GPIO_InitStruct.Pin = Q1_6_Pin|Q1_7_Pin|U485_DE_RE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : Q0_7_Pin Q1_5_Pin */
  GPIO_InitStruct.Pin = Q0_7_Pin|Q1_5_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
