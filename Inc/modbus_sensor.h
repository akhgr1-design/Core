/* USER CODE BEGIN Header */
/**
  * @file    modbus_sensor.h
  * @brief   Modbus RTU driver for DS18B20 Temperature Sensor Board
  * @author  Industrial Chiller Control
  * @version 1.0
  * @date    2025
  */
/* USER CODE END Header */

#ifndef MODBUS_SENSOR_H
#define MODBUS_SENSOR_H

#include "main.h"
#include <stdint.h>

// Modbus Slave Configuration
#define MODBUS_SLAVE_ID           0x01        // Slave ID of DS18B20 board
#define MODBUS_UART               &huart8     // Already initialized in uart_comm.c
#define MODBUS_DE_PORT            U485_DE_RE_GPIO_Port  // PD4 controls DE/RE
#define MODBUS_DE_PIN             U485_DE_RE_Pin        // PD4

// Register Addresses (updated to match specification)
#define REG_SENSOR_COUNT          0x00        // Register 0: Active sensor count
#define REG_TEMP_START            0x01        // Register 1: Start of temperature registers (A0-A7)
#define REG_STATUS_BITMAP         0x09        // Register 9: Sensor connection status
#define REG_UPTIME                0x0A        // Register 10: System uptime
#define REG_ERROR_COUNT           0x0B        // Register 11: Error counter

// Sensor Count Limits
#define MAX_SENSORS               8

// Function Prototypes
uint8_t Modbus_Init(void);
void Modbus_ProcessCommunication(void);
void Modbus_Debug_Status(void);
uint8_t Modbus_Read_AllData(void);  // Add this line
uint8_t Modbus_Read_Temperatures(float *temps);
uint8_t Modbus_Read_Status(uint16_t *status, uint16_t *error_count);
uint8_t Modbus_Read_SensorCount(uint8_t *count);
uint8_t Modbus_Read_Uptime(uint16_t *uptime);

// Utility: Convert register value to temperature
float Modbus_Decode_Temperature(uint16_t reg_value);

// High-level system functions (for main.c)
uint8_t Modbus_System_Init(void);
void Modbus_System_Start(void);
void Modbus_System_Process(void);
uint8_t Modbus_System_IsEnabled(void);
void Modbus_System_Enable(void);
void Modbus_System_Disable(void);
void Modbus_System_SetInterval(uint32_t interval_ms);  // Add this line

#endif /* MODBUS_SENSOR_H */
