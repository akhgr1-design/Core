#include "uart_comm.h"
#include "main.h"
#include "usart.h"
#include "gpio.h"
#include <string.h>

// External UART handles
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart8;
extern UART_HandleTypeDef huart7;  // Add UART7

// Initialize UART peripherals
void Init_UARTs(void) {
    // Enable UART clocks
    __HAL_RCC_UART4_CLK_ENABLE();
    __HAL_RCC_UART8_CLK_ENABLE();
    __HAL_RCC_UART7_CLK_ENABLE();  // Add UART7 clock enable
    
    // Configure UART4 (HMI) - 9600 baud
    huart4.Instance = UART4;
    huart4.Init.BaudRate = 9600;
    huart4.Init.WordLength = UART_WORDLENGTH_8B;
    huart4.Init.StopBits = UART_STOPBITS_1;
    huart4.Init.Parity = UART_PARITY_NONE;
    huart4.Init.Mode = UART_MODE_TX_RX;
    huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart4.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart4);
    
    // Configure UART8 (RS485) - 9600 baud for Modbus
    huart8.Instance = UART8;
    huart8.Init.BaudRate = 9600;  // FIX: Set to 9600 for Modbus
    huart8.Init.WordLength = UART_WORDLENGTH_8B;
    huart8.Init.StopBits = UART_STOPBITS_1;
    huart8.Init.Parity = UART_PARITY_NONE;
    huart8.Init.Mode = UART_MODE_TX_RX;
    huart8.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart8.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart8);
    
    // Configure UART7 (DEBUG) - 115200 baud
    huart7.Instance = UART7;
    huart7.Init.BaudRate = 115200;
    huart7.Init.WordLength = UART_WORDLENGTH_8B;
    huart7.Init.StopBits = UART_STOPBITS_1;
    huart7.Init.Parity = UART_PARITY_NONE;
    huart7.Init.Mode = UART_MODE_TX_RX;
    huart7.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart7.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart7);
}

// Send data to HMI UART (UART4) - for future DWIN HMI
void Send_HMI_Data(const char *message) {
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET); // DE HIGH
    HAL_Delay(1);
    HAL_UART_Transmit(&huart4, (uint8_t*)message, strlen(message), 1000);
    HAL_Delay(1);
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET); // DE LOW
}

// Send data to Debug UART (UART7) - for debug output
void Send_Debug_Data(const char *message) {
    HAL_UART_Transmit(&huart7, (uint8_t*)message, strlen(message), 1000);
}

// Send data to Modbus UART (UART8) - for RS485 Modbus
void Send_RS485_Data(const char *message) {
    HAL_GPIO_WritePin(U485_DE_RE_GPIO_Port, U485_DE_RE_Pin, GPIO_PIN_SET); // DE HIGH
    HAL_Delay(1);
    HAL_UART_Transmit(&huart8, (uint8_t*)message, strlen(message), 1000);
    HAL_Delay(1);
    HAL_GPIO_WritePin(U485_DE_RE_GPIO_Port, U485_DE_RE_Pin, GPIO_PIN_RESET); // DE LOW
}