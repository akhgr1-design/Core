/**
  ******************************************************************************
  * @file    hmi.h
  * @brief   DWIN DGUS HMI Communication Interface Header
  * @author  Your Name
  * @version V1.0
  * @date    2025
  ******************************************************************************
  */

#ifndef __HMI_H
#define __HMI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* GPIO Pin Definitions for HMI RS485 control --------------------------------*/
#ifndef HMI_DE_RE_GPIO_Port
#define HMI_DE_RE_GPIO_Port     GPIOE
#endif
#ifndef HMI_DE_RE_Pin
#define HMI_DE_RE_Pin           GPIO_PIN_8
#endif
#ifndef HMI_TX_GPIO_Port
#define HMI_TX_GPIO_Port        GPIOD
#endif
#ifndef HMI_TX_Pin
#define HMI_TX_Pin              GPIO_PIN_1
#endif
#ifndef HMI_RX_GPIO_Port
#define HMI_RX_GPIO_Port        GPIOD
#endif
#ifndef HMI_RX_Pin
#define HMI_RX_Pin              GPIO_PIN_0
#endif

/* Exported defines ----------------------------------------------------------*/
// DWIN DGUS Protocol Commands
#define DWIN_HEADER_SIZE        3
#define DWIN_HEADER_BYTE1       0x5A
#define DWIN_HEADER_BYTE2       0xA5
#define DWIN_MAX_DATA_LEN       252

// DWIN Commands
#define DWIN_CMD_WRITE_REG      0x82    // Write register
#define DWIN_CMD_READ_REG       0x83    // Read register
#define DWIN_CMD_WRITE_VAR      0x82    // Write variable
#define DWIN_CMD_READ_VAR       0x83    // Read variable

// Common DWIN Register Addresses
#define DWIN_REG_PIC_ID         0x03    // Picture ID register
#define DWIN_REG_TP_FLAG        0x4F    // Touch panel flag
#define DWIN_REG_TP_STATUS      0x4E    // Touch status
#define DWIN_REG_LED_NOW        0x31    // LED brightness
#define DWIN_REG_BUZZER_TIME    0xA0    // Buzzer time

// Variable Memory Addresses (VP addresses for your application)
#define HMI_VP_TEMP             0x1000  // Temperature display
#define HMI_VP_PRESSURE         0x1001  // Pressure display
#define HMI_VP_FLOW             0x1002  // Flow display
#define HMI_VP_LEVEL            0x1003  // Level display
#define HMI_VP_STATUS           0x1004  // System status
#define HMI_VP_NETWORK_STATUS   0x1005  // Network status
#define HMI_VP_MODBUS_STATUS    0x1006  // Modbus status
#define HMI_VP_RELAY_Q06        0x1007  // Relay Q0.6 status
#define HMI_VP_RELAY_Q07        0x1008  // Relay Q0.7 status
#define HMI_VP_SYSTEM_TIME      0x1010  // System uptime
#define HMI_VP_MESSAGE_COUNT    0x1011  // Message counter

// Touch Control VP Addresses (for buttons/controls from HMI)
#define HMI_VP_BTN_RELAY_Q06    0x2000  // Button to control Q0.6
#define HMI_VP_BTN_RELAY_Q07    0x2001  // Button to control Q0.7
#define HMI_VP_BTN_RESET        0x2002  // Reset button
#define HMI_VP_BTN_TEST         0x2003  // Test button

// System Status Values
#define HMI_STATUS_INIT         0x0000
#define HMI_STATUS_RUNNING      0x0001
#define HMI_STATUS_ERROR        0x0002
#define HMI_STATUS_STOP         0x0003

// Network Status Values
#define HMI_NET_DISCONNECTED    0x0000
#define HMI_NET_CONNECTING      0x0001
#define HMI_NET_CONNECTED       0x0002
#define HMI_NET_ERROR           0x0003

// Modbus Status Values
#define HMI_MODBUS_INIT         0x0000
#define HMI_MODBUS_OK           0x0001
#define HMI_MODBUS_ERROR        0x0002
#define HMI_MODBUS_TIMEOUT      0x0003

/* Exported types ------------------------------------------------------------*/
typedef enum {
    HMI_OK = 0,
    HMI_ERROR = 1,
    HMI_TIMEOUT = 2,
    HMI_BUSY = 3
} HMI_StatusTypeDef;

typedef struct {
    uint8_t connected;
    uint32_t last_activity;
    uint32_t connection_check_interval;
    uint32_t data_update_interval;
    uint32_t last_data_update;
    uint8_t tx_buffer[256];
    uint8_t rx_buffer[256];
    uint16_t rx_index;
    uint8_t waiting_response;
} HMI_HandleTypeDef;

typedef struct {
    uint16_t temperature;
    uint16_t pressure;
    uint16_t flow;
    uint16_t level;
    uint16_t system_status;
    uint16_t network_status;
    uint16_t modbus_status;
    uint8_t relay_q06;
    uint8_t relay_q07;
    uint32_t system_time;
    uint32_t message_count;
} HMI_DataTypeDef;

/* Exported variables --------------------------------------------------------*/
extern HMI_HandleTypeDef hmi_handle;
extern HMI_DataTypeDef hmi_data;

/* Exported function prototypes ----------------------------------------------*/
// System Functions
HMI_StatusTypeDef HMI_Init(void);
HMI_StatusTypeDef HMI_Process(void);

// Connection Management
uint8_t HMI_Detect_Connection(void);
uint8_t HMI_IsConnected(void);
void HMI_SetConnectionCheckInterval(uint32_t interval_ms);

// Utility Functions
void HMI_SendSystemInfo(void);

// Version check functions
void HMI_Send_Version_Check(void);
void HMI_Process_With_Version_Check(void);

// Enhanced robust functions
void HMI_Send_Robust_Version_Check(void);
void HMI_Capture_With_Stats(void);

// Test function with different terminations
void HMI_Test_Terminations(void);
void HMI_Process_Termination_Test(void);

void HMI_Test_Terminations_Slow(void);
void HMI_Capture_Show_All(void);
void HMI_Process_Termination_Test_Slow(void);

// Fixed version that captures complete responses
void HMI_Send_And_Capture_Complete(void);
void HMI_Process_Complete_Capture(void);

// Ultra-fast timing for DWIN HMI
void HMI_Send_Ultra_Fast_Timing(void);
void HMI_Send_With_TC_Flag(void);
void HMI_Process_Ultra_Fast(void);

void HMI_Send_Single_Command_Fixed(void);
void HMI_Process_Single_Command(void);

void HMI_Hardware_RX_Test(void);

void HMI_Send_Extended_Capture(void);
void HMI_Process_Extended_Capture(void);

void HMI_Capture_Real_DWIN_Response(void);
void HMI_Process_Real_DWIN_Capture(void);

void HMI_Capture_3_Byte_Response(void);
void HMI_Process_3_Byte_Capture(void);

void HMI_Fix_RS485_Direction(void);
void HMI_Process_RS485_Fix(void);

void HMI_Test_Packet_Terminations(void);
void HMI_Process_Termination_Test(void);

// CRC calculation for DWIN HMI commands
uint16_t Calculate_CRC16(uint8_t* data, uint8_t length);
uint8_t Calculate_CRC8(uint8_t* data, uint8_t length);
uint8_t Calculate_Simple_Checksum(uint8_t* data, uint8_t length);
uint8_t Calculate_XOR_Checksum(uint8_t* data, uint8_t length);
void HMI_Test_With_CRC(void);
void HMI_Process_CRC_Test(void);

// C51-specific HMI communication test
void HMI_Test_C51_Protocol(void);
void HMI_Process_C51_Test(void);

void HMI_Change_Page_0(void);
void HMI_Simple_Page_Change(void);
void HMI_Process_Page_Change_Test(void);

void HMI_Reset_Command(void);
void HMI_Test_Page_Changes(void);

// Add these new function declarations
void HMI_Correct_Page_Change(void);
void HMI_Correct_Reset(void);
void HMI_Test_Register_Write(void);

// Diagnostic: Send page change with detailed timing analysis
void HMI_Diagnostic_Page_Change(void);
// Test with slower byte-by-byte transmission
void HMI_Slow_Byte_Transmission(void);

void HMI_Test_PC_Working_Command(void);

// Test reading VP using the correct format you provided
void HMI_Test_VP_Read(void);

// Comprehensive UART4 RX diagnostic
void HMI_UART_RX_Diagnostic(void);
void HMI_Force_Enable_UART_RX(void);

// Improved HMI response capture with better timing
void HMI_Improved_Page_Test(void);

void HMI_Start_Interrupt_RX(void);

void HMI_System_Process(void);
void HMI_Process_Debug_Command(const char* command);
void HMI_Test_Page_Status(void);
void HMI_Set_Initialized(uint8_t status);
uint8_t HMI_Is_Initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* __HMI_H */
