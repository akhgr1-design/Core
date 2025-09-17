/* USER CODE BEGIN Header */
/**
  * @file    modbus_sensor.c
  * @brief   Modbus RTU driver for DS18B20 Temperature Sensor Board
  * @author  Industrial Chiller Control
  * @version 1.0
  * @date    2025
  */
/* USER CODE END Header */

#include "modbus_sensor.h"
#include "main.h"
#include <string.h>

// Add this line:
extern UART_HandleTypeDef huart8;

// Static buffer for Modbus communication
static uint8_t modbus_tx_buf[8];
static uint8_t modbus_rx_buf[25];  // Max: 1 byte ID + 1 byte func + 1 byte byte-count + 16*2 data + 2 CRC

// Global variables for Modbus state machine
static uint8_t modbus_state = 0;
static uint32_t modbus_last_request = 0;
static uint32_t modbus_request_interval = 30000; // 30 seconds between requests
static float last_temperatures[8] = {0};
static uint16_t last_status = 0;
static uint16_t last_error_count = 0;
static uint8_t last_sensor_count = 0;
static uint16_t last_uptime = 0;
static uint8_t modbus_initialized = 0;

// Global system control
static uint8_t modbus_system_enabled = 0;

/**
  * @brief  Calculate Modbus RTU CRC-16
  * @param  data: Pointer to data array
  * @param  length: Number of bytes
  * @retval CRC-16 value
  */
static uint16_t Modbus_CRC16(uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/**
  * @brief  Send Modbus request and receive response
  * @param  tx_buf: Transmit buffer
  * @param  tx_len: Length of data to send
  * @param  rx_buf: Receive buffer
  * @param  rx_len: Expected response length
  * @param  timeout_ms: Response timeout
  * @retval 1 if success, 0 if failure
  */
static uint8_t Modbus_Transceive(uint8_t *tx_buf, uint16_t tx_len, uint8_t *rx_buf, uint16_t rx_len, uint32_t timeout_ms) {
    // Clear UART receive buffer first
    uint8_t dummy;
    while (HAL_UART_Receive(MODBUS_UART, &dummy, 1, 1) == HAL_OK) {
        // Clear any pending data
    }
    
    // Add debug output
    char msg[100];
    snprintf(msg, sizeof(msg), "Modbus TX: %02X %02X %02X %02X %02X %02X %02X %02X\r\n", 
             tx_buf[0], tx_buf[1], tx_buf[2], tx_buf[3], tx_buf[4], tx_buf[5], tx_buf[6], tx_buf[7]);
    Send_Debug_Data(msg);
    
    // Enable DE (Transmit)
    HAL_GPIO_WritePin(MODBUS_DE_PORT, MODBUS_DE_PIN, GPIO_PIN_SET);
    HAL_UART_Transmit(MODBUS_UART, tx_buf, tx_len, timeout_ms);
    HAL_GPIO_WritePin(MODBUS_DE_PORT, MODBUS_DE_PIN, GPIO_PIN_RESET);

    // Wait before receiving - increase delay
    HAL_Delay(10);  // Increase from 2ms to 10ms

    // Receive response
    HAL_StatusTypeDef rx_status = HAL_UART_Receive(MODBUS_UART, rx_buf, rx_len, timeout_ms);
    if (rx_status != HAL_OK) {
        snprintf(msg, sizeof(msg), "Modbus RX FAILED: Status=%d\r\n", rx_status);
        Send_Debug_Data(msg);
        return 0;
    }

    // Show received data
    snprintf(msg, sizeof(msg), "Modbus RX: ");
    Send_Debug_Data(msg);
    for (int i = 0; i < rx_len; i++) {
        snprintf(msg, sizeof(msg), "%02X ", rx_buf[i]);
        Send_Debug_Data(msg);
    }
    Send_Debug_Data("\r\n");

    // Validate response length and CRC
    if (rx_len < 3) {
        Send_Debug_Data("Modbus: Response too short\r\n");
        return 0;
    }
    uint16_t crc = Modbus_CRC16(rx_buf, rx_len - 2);
    uint16_t received_crc = (rx_buf[rx_len - 1] << 8) | rx_buf[rx_len - 2];
    
    if (crc != received_crc) {
        snprintf(msg, sizeof(msg), "Modbus CRC FAILED: Calc=0x%04X, Recv=0x%04X\r\n", crc, received_crc);
        Send_Debug_Data(msg);
        return 0;
    }
    
    Send_Debug_Data("Modbus: SUCCESS\r\n");
    return 1;
}

/**
  * @brief  Decode temperature from register value
  * @param  reg_value: Raw register value
  * @retval Temperature in °C, or -999.0 if disconnected
  */
float Modbus_Decode_Temperature(uint16_t reg_value) {
    if (reg_value == 0) {
        return -999.0f;  // Sensor not connected
    }
    
    // Temperature format: (temp * 100) + 10000
    // So to decode: (reg_value - 10000) / 100
    float temp = (float)(reg_value - 10000) / 100.0f;
    
    // Sanity check: temperatures should be reasonable (-50°C to +150°C)
    if (temp < -50.0f || temp > 150.0f) {
        return -999.0f;  // Invalid reading
    }
    
    return temp;
}

/**
  * @brief  Initialize Modbus communication
  * @retval 1 if success, 0 if failure
  */
uint8_t Modbus_Init(void) {
    // Reset state machine
    modbus_state = 0;
    modbus_last_request = 0;
    modbus_initialized = 1;
    
    // Initialize temperature array
    for (int i = 0; i < 8; i++) {
        last_temperatures[i] = -999.0f;
    }
    
    return 1; // Always return success for now
}

/**
  * @brief  Process Modbus communication state machine
  */
void Modbus_ProcessCommunication(void) {
    if (!modbus_initialized) return;
    
    uint32_t current_time = HAL_GetTick();
    
    // Check if it's time for next request
    if (current_time - modbus_last_request >= modbus_request_interval) {
        modbus_last_request = current_time;
        
        // Read all registers in one command: 0-11 (12 registers total)
        if (Modbus_Read_AllData()) {
            // Show debug output after successful read
            Modbus_Debug_Status();
        }
    }
}

/**
  * @brief  Print clean Modbus debug status - only connected sensors
  */
void Modbus_Debug_Status(void) {
    Send_Debug_Data("DEBUG: Modbus_Debug_Status called\r\n");
    
    char msg[200];
    
    // Show system status
    snprintf(msg, sizeof(msg), "=== Modbus System Status ===\r\n");
    Send_Debug_Data(msg);
    snprintf(msg, sizeof(msg), "Slave ID: 0x%02X, Sensors: %d, Uptime: %ds, Errors: %d\r\n", 
             MODBUS_SLAVE_ID, last_sensor_count, last_uptime, last_error_count);
    Send_Debug_Data(msg);
    
    // Show only connected sensors
    snprintf(msg, sizeof(msg), "Connected Sensors:\r\n");
    Send_Debug_Data(msg);
    
    for (int i = 0; i < 8; i++) {
        if (last_temperatures[i] > -999.0f) {
            // Use integer math to avoid float formatting issues
            int temp_int = (int)(last_temperatures[i] * 100);
            int temp_whole = temp_int / 100;
            int temp_frac = (temp_int % 100) / 10;
            int temp_frac2 = temp_int % 10;
            snprintf(msg, sizeof(msg), "  A%d: %d.%d%dC\r\n", i, temp_whole, temp_frac, temp_frac2);
            Send_Debug_Data(msg);
        }
    }
    Send_Debug_Data("\r\n");
}

/**
  * @brief  Read sensor count (Register 0)
  * @param  count: Pointer to store sensor count
  * @retval 1 if success, 0 if failure
  */
uint8_t Modbus_Read_SensorCount(uint8_t *count) {
    modbus_tx_buf[0] = MODBUS_SLAVE_ID;
    modbus_tx_buf[1] = 0x03;  // Read Holding Registers
    modbus_tx_buf[2] = 0x00;  // High byte of address
    modbus_tx_buf[3] = 0x00;  // Low byte of address (REG_SENSOR_COUNT)
    modbus_tx_buf[4] = 0x00;  // High byte of count
    modbus_tx_buf[5] = 0x01;  // Low byte of count (1 register)
    uint16_t crc = Modbus_CRC16(modbus_tx_buf, 6);
    modbus_tx_buf[6] = crc & 0xFF;
    modbus_tx_buf[7] = (crc >> 8) & 0xFF;

    if (!Modbus_Transceive(modbus_tx_buf, 8, modbus_rx_buf, 7, 500)) {
        return 0;
    }

    *count = modbus_rx_buf[3];  // Sensor count is in the first data byte
    return 1;
}

/**
  * @brief  Read all 8 temperature sensors (Registers 1-8)
  * @param  temps: Array of 8 floats to store temperatures
  * @retval 1 if success, 0 if failure
  */
uint8_t Modbus_Read_Temperatures(float *temps) {
    // Build request: Read Holding Registers (0x03), Addr=0x01, Count=8
    modbus_tx_buf[0] = MODBUS_SLAVE_ID;
    modbus_tx_buf[1] = 0x03;
    modbus_tx_buf[2] = 0x00;  // High byte of address
    modbus_tx_buf[3] = 0x01;  // Low byte of address (REG_TEMP_START)
    modbus_tx_buf[4] = 0x00;  // High byte of count
    modbus_tx_buf[5] = 0x08;  // Low byte of count (8 registers)
    uint16_t crc = Modbus_CRC16(modbus_tx_buf, 6);
    modbus_tx_buf[6] = crc & 0xFF;
    modbus_tx_buf[7] = (crc >> 8) & 0xFF;

    // Expected response: 1 (ID) + 1 (func) + 1 (byte count) + 16 (8×2 bytes) + 2 (CRC) = 21 bytes
    if (!Modbus_Transceive(modbus_tx_buf, 8, modbus_rx_buf, 21, 500)) {
        return 0;
    }

    // Parse temperatures
    for (int i = 0; i < 8; i++) {
        uint16_t reg_val = (modbus_rx_buf[3 + i*2] << 8) | modbus_rx_buf[4 + i*2];
        temps[i] = Modbus_Decode_Temperature(reg_val);
    }

    return 1;
}

/**
  * @brief  Read system status (Registers 9-11)
  * @param  status: Pointer to store status bitmap
  * @param  error_count: Pointer to store error count
  * @retval 1 if success, 0 if failure
  */
uint8_t Modbus_Read_Status(uint16_t *status, uint16_t *error_count) {
    modbus_tx_buf[0] = MODBUS_SLAVE_ID;
    modbus_tx_buf[1] = 0x03;
    modbus_tx_buf[2] = 0x00;
    modbus_tx_buf[3] = 0x09;  // REG_STATUS_BITMAP
    modbus_tx_buf[4] = 0x00;
    modbus_tx_buf[5] = 0x03;  // Read 3 registers (status, uptime, error count)
    uint16_t crc = Modbus_CRC16(modbus_tx_buf, 6);
    modbus_tx_buf[6] = crc & 0xFF;
    modbus_tx_buf[7] = (crc >> 8) & 0xFF;

    if (!Modbus_Transceive(modbus_tx_buf, 8, modbus_rx_buf, 11, 500)) {
        return 0;
    }

    *status = (modbus_rx_buf[3] << 8) | modbus_rx_buf[4];        // Register 9
    last_uptime = (modbus_rx_buf[5] << 8) | modbus_rx_buf[6];    // Register 10
    *error_count = (modbus_rx_buf[7] << 8) | modbus_rx_buf[8];   // Register 11

    return 1;
}

/**
  * @brief  Read all sensor data with retry logic
  * @retval 1 if success, 0 if failure
  */
uint8_t Modbus_Read_AllData(void) {
    uint8_t retry_count = 0;
    uint8_t max_retries = 3;
    
    while (retry_count < max_retries) {
        // Prepare request: Read 12 registers starting from 0
        modbus_tx_buf[0] = 0x01;  // Slave ID
        modbus_tx_buf[1] = 0x03;  // Function code (Read Holding Registers)
        modbus_tx_buf[2] = 0x00;  // High byte of start address
        modbus_tx_buf[3] = 0x00;  // Low byte of start address
        modbus_tx_buf[4] = 0x00;  // High byte of register count
        modbus_tx_buf[5] = 0x0C;  // Low byte of register count (12 registers)

        uint16_t crc = Modbus_CRC16(modbus_tx_buf, 6);
        modbus_tx_buf[6] = crc & 0xFF;
        modbus_tx_buf[7] = (crc >> 8) & 0xFF;

        // Expected response: 1 (ID) + 1 (func) + 1 (byte count) + 24 (12×2 bytes) + 2 (CRC) = 29 bytes
        if (Modbus_Transceive(modbus_tx_buf, 8, modbus_rx_buf, 29, 1000)) {
            // Success - parse data
            last_sensor_count = modbus_rx_buf[3];
            
            // Registers 1-8: Temperatures
            for (int i = 0; i < 8; i++) {
                uint16_t reg_val = (modbus_rx_buf[5 + i*2] << 8) | modbus_rx_buf[6 + i*2];
                last_temperatures[i] = Modbus_Decode_Temperature(reg_val);
            }
            
            // Registers 9-11: Status, Uptime, Error count
            last_status = (modbus_rx_buf[21] << 8) | modbus_rx_buf[22];
            last_uptime = (modbus_rx_buf[23] << 8) | modbus_rx_buf[24];
            last_error_count = (modbus_rx_buf[25] << 8) | modbus_rx_buf[26];
            
            return 1;  // Success
        }
        
        // Failed - wait before retry
        retry_count++;
        if (retry_count < max_retries) {
            HAL_Delay(2000);  // Wait 2 seconds before retry
            Send_Debug_Data("Modbus: Retrying...\r\n");
        }
    }
    
    Send_Debug_Data("Modbus: All retries failed\r\n");
    return 0;
}

/**
  * @brief  Initialize the entire Modbus system
  * @retval 1 if success, 0 if failure
  */
uint8_t Modbus_System_Init(void) {
    // Initialize the Modbus system
    if (Modbus_Init()) {
        modbus_system_enabled = 1;
        Send_Debug_Data("Modbus RTU: System initialized successfully\r\n");
        return 1;
    } else {
        modbus_system_enabled = 0;
        Send_Debug_Data("Modbus RTU: System initialization failed\r\n");
        return 0;
    }
}

/**
  * @brief  Start Modbus system (perform first read)
  */
void Modbus_System_Start(void) {
    if (modbus_system_enabled) {
        Send_Debug_Data("--- Performing First Modbus Sensor Read ---\r\n");
        Modbus_Read_AllData(); // Perform first read
        Modbus_Debug_Status(); // Show first results
        Send_Debug_Data("--- First Modbus Read Complete ---\r\n");
    }
}

/**
  * @brief  Process Modbus system (main loop function)
  */
void Modbus_System_Process(void) {
    if (modbus_system_enabled) {
        Modbus_ProcessCommunication();
        // Remove the blocking delay - it was causing the system to freeze
        // HAL_Delay(2000);  // REMOVED - this was blocking the main loop
    }
}

/**
  * @brief  Check if Modbus system is enabled
  * @retval 1 if enabled, 0 if disabled
  */
uint8_t Modbus_System_IsEnabled(void) {
    return modbus_system_enabled;
}

/**
  * @brief  Enable Modbus system
  */
void Modbus_System_Enable(void) {
    modbus_system_enabled = 1;
    Send_Debug_Data("Modbus system: ENABLED\r\n");
}

/**
  * @brief  Disable Modbus system
  */
void Modbus_System_Disable(void) {
    modbus_system_enabled = 0;
    Send_Debug_Data("Modbus system: DISABLED\r\n");
}

/**
  * @brief  Set Modbus polling interval
  * @param  interval_ms: Interval in milliseconds
  */
void Modbus_System_SetInterval(uint32_t interval_ms) {
    modbus_request_interval = interval_ms;
    char msg[100];
    snprintf(msg, sizeof(msg), "Modbus polling interval set to: %lu ms\r\n", interval_ms);
    Send_Debug_Data(msg);
}
