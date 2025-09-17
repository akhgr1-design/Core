/**
  ******************************************************************************
  * @file    flash_25q16.c
  * @brief   25Q16BSIG SPI Flash Memory Driver (Read Only)
  ******************************************************************************
  */

#include "flash_25q16.h"

// External debug function
extern void Send_Debug_Data(const char *message);

// External SPI handle
extern SPI_HandleTypeDef hspi2;

/**
 * @brief Initialize Flash Memory (Clean version)
 */
uint8_t Flash_Init(void) {
    // Configure CS pin as GPIO output
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = FLASH_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(FLASH_CS_PORT, &GPIO_InitStruct);
    
    // Set CS high (inactive)
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
    
    // Wait a bit for flash to be ready
    HAL_Delay(10);
    
    // Test SPI communication first (silent)
    if (Flash_TestSPI() != 0) {
        return 1;
    }
    
    // Test communication using JEDEC ID (silent)
    uint8_t manufacturer, device_id;
    if (Flash_ReadID(&manufacturer, &device_id) == 0) {
        // Check for Winbond manufacturer (0xC8) and 16Mbit capacity (0x15)
        if (manufacturer == 0xC8 && device_id == 0x15) {
            Send_Debug_Data("25Q16BSIG Flash detected successfully!\r\n");
            return 0; // Success
        } else {
            return 1; // Error - wrong chip
        }
    } else {
        return 1; // Error
    }
}

/**
 * @brief Read Flash Memory ID (using JEDEC ID) - Silent version
 */
uint8_t Flash_ReadID(uint8_t *manufacturer, uint8_t *device_id) {
    uint8_t cmd = 0x9F; // JEDEC ID command
    uint8_t id_data[3];
    
    // Select flash
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_RESET);
    HAL_Delay(1);
    
    // Send JEDEC ID command
    if (HAL_SPI_Transmit(&hspi2, &cmd, 1, 1000) != HAL_OK) {
        HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
        return 1;
    }
    
    // Read JEDEC ID data (3 bytes)
    if (HAL_SPI_Receive(&hspi2, id_data, 3, 1000) != HAL_OK) {
        HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
        return 1;
    }
    
    // Deselect flash
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
    
    // JEDEC ID format: [Manufacturer ID, Memory Type, Capacity]
    *manufacturer = id_data[0];  // Manufacturer ID
    *device_id = id_data[2];     // Capacity ID (we'll use this as device ID)
    
    return 0; // Success
}

/**
 * @brief Read Data from Flash
 */
uint8_t Flash_ReadData(uint32_t address, uint8_t *data, uint16_t length) {
    uint8_t cmd = FLASH_CMD_READ_DATA;
    uint8_t addr_bytes[3];
    
    // Convert address to bytes
    addr_bytes[0] = (address >> 16) & 0xFF;
    addr_bytes[1] = (address >> 8) & 0xFF;
    addr_bytes[2] = address & 0xFF;
    
    // Select flash
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_RESET);
    
    // Send command
    if (HAL_SPI_Transmit(&hspi2, &cmd, 1, 1000) != HAL_OK) {
        HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
        return 1;
    }
    
    // Send address
    if (HAL_SPI_Transmit(&hspi2, addr_bytes, 3, 1000) != HAL_OK) {
        HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
        return 1;
    }
    
    // Read data
    if (HAL_SPI_Receive(&hspi2, data, length, 1000) != HAL_OK) {
        HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
        return 1;
    }
    
    // Deselect flash
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
    
    return 0; // Success
}

/**
 * @brief Test Flash Memory (Read Only) - Simple Status Check
 */
uint8_t Flash_Test(void) {
    Send_Debug_Data("=== FLASH MEMORY STATUS CHECK ===\r\n");
    
    // Test 1: Read ID
    uint8_t manufacturer, device_id;
    if (Flash_ReadID(&manufacturer, &device_id) == 0) {
        // Check for Winbond manufacturer (0xC8) and 16Mbit capacity (0x15)
        if (manufacturer == 0xC8 && device_id == 0x15) {
            Send_Debug_Data("Flash Memory: RESPONDING (25Q16BSIG detected)\r\n");
            return 0; // Success
        } else {
            Send_Debug_Data("Flash Memory: RESPONDING (Unknown chip)\r\n");
            return 1; // Error - wrong chip
        }
    } else {
        Send_Debug_Data("Flash Memory: NOT RESPONDING\r\n");
        return 1; // Error - no response
    }
}

/**
 * @brief Test SPI Communication (Silent version)
 */
uint8_t Flash_TestSPI(void) {
    // Test 1: Check if SPI2 is initialized
    if (hspi2.Instance != SPI2) {
        return 1;
    }
    
    // Test 2: Try to send a simple command
    uint8_t test_cmd = 0x9F; // JEDEC ID command
    uint8_t response[3];
    
    // Select flash
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_RESET);
    HAL_Delay(1);
    
    // Send JEDEC ID command
    if (HAL_SPI_Transmit(&hspi2, &test_cmd, 1, 1000) != HAL_OK) {
        HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
        return 1;
    }
    
    // Read response
    if (HAL_SPI_Receive(&hspi2, response, 3, 1000) != HAL_OK) {
        HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
        return 1;
    }
    
    // Deselect flash
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
    
    return 0; // Success
}

/**
 * @brief Analyze flash data patterns
 */
void Flash_Analyze_Data(void) {
    Send_Debug_Data("=== FLASH DATA ANALYSIS ===\r\n");
    
    uint8_t data[64];
    
    // Read from different addresses to see patterns
    uint32_t addresses[] = {0x000000, 0x000100, 0x001000, 0x010000};
    
    for (int i = 0; i < 4; i++) {
        char msg[50];
        snprintf(msg, sizeof(msg), "Address 0x%06lX: ", addresses[i]);
        Send_Debug_Data(msg);
        
        if (Flash_ReadData(addresses[i], data, 16) == 0) {
            for (int j = 0; j < 16; j++) {
                char byte_msg[10];
                snprintf(byte_msg, sizeof(byte_msg), "%02X ", data[j]);
                Send_Debug_Data(byte_msg);
            }
            Send_Debug_Data("\r\n");
        } else {
            Send_Debug_Data("Read failed\r\n");
        }
    }
    
    // Try to interpret the first 32 bytes as different data types
    Send_Debug_Data("Data interpretation:\r\n");
    Send_Debug_Data("- As temperatures (if 0x0259 = 601 = 60.1°C): ");
    Send_Debug_Data("60.1°C, 60.1°C\r\n");
    Send_Debug_Data("- As pressure (if 0x0259 = 601 = 6.01 bar): ");
    Send_Debug_Data("6.01 bar, 6.01 bar\r\n");
    Send_Debug_Data("- As timestamps: Possible date/time data\r\n");
}

/**
 * @brief Write Enable
 */
uint8_t Flash_WriteEnable(void) {
    uint8_t cmd = 0x06; // Write Enable command
    
    // Select flash
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_RESET);
    
    // Send command
    if (HAL_SPI_Transmit(&hspi2, &cmd, 1, 1000) != HAL_OK) {
        HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
        return 1;
    }
    
    // Deselect flash
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
    
    return 0; // Success
}

/**
 * @brief Wait for Flash to be Ready
 */
uint8_t Flash_WaitReady(void) {
    uint32_t timeout = HAL_GetTick() + 5000; // 5 second timeout
    
    while (HAL_GetTick() < timeout) {
        uint8_t status = Flash_ReadStatusReg(); // Use renamed function
        if ((status & 0x01) == 0) { // Check BUSY bit
            return 0; // Ready
        }
        HAL_Delay(1);
    }
    
    return 1; // Timeout
}

/**
 * @brief Read Status Register (renamed to avoid conflict)
 */
uint8_t Flash_ReadStatusReg(void) {
    uint8_t cmd = 0x05; // Read Status Register
    uint8_t status;
    
    // Select flash
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_RESET);
    
    // Send command
    if (HAL_SPI_Transmit(&hspi2, &cmd, 1, 1000) != HAL_OK) {
        HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
        return 0xFF;
    }
    
    // Read status
    if (HAL_SPI_Receive(&hspi2, &status, 1, 1000) != HAL_OK) {
        HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
        return 0xFF;
    }
    
    // Deselect flash
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
    
    return status;
}

/**
 * @brief Write Page to Flash
 */
uint8_t Flash_WritePage(uint32_t address, uint8_t *data, uint16_t length) {
    uint8_t cmd = 0x02; // Page Program command
    uint8_t addr_bytes[3];
    
    // Convert address to bytes
    addr_bytes[0] = (address >> 16) & 0xFF;
    addr_bytes[1] = (address >> 8) & 0xFF;
    addr_bytes[2] = address & 0xFF;
    
    // Enable write
    if (Flash_WriteEnable() != 0) {
        return 1;
    }
    
    // Select flash
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_RESET);
    
    // Send command
    if (HAL_SPI_Transmit(&hspi2, &cmd, 1, 1000) != HAL_OK) {
        HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
        return 1;
    }
    
    // Send address
    if (HAL_SPI_Transmit(&hspi2, addr_bytes, 3, 1000) != HAL_OK) {
        HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
        return 1;
    }
    
    // Send data
    if (HAL_SPI_Transmit(&hspi2, data, length, 1000) != HAL_OK) {
        HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
        return 1;
    }
    
    // Deselect flash
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
    
    // Wait for write to complete
    if (Flash_WaitReady() != 0) {
        return 1;
    }
    
    return 0; // Success
}

/**
 * @brief Write Modbus Data to Flash
 */
void Flash_Write_Modbus_Data(void) {
    Send_Debug_Data("=== WRITING MODBUS DATA TO FLASH ===\r\n");
    
    // Get Modbus sensor data (you'll need to access your Modbus data)
    // For now, let's create some test data
    uint8_t modbus_data[32];
    uint32_t timestamp = HAL_GetTick() / 1000; // Current time in seconds
    
    // Fill with test data (replace with actual Modbus data)
    modbus_data[0] = 0xAA; // Header
    modbus_data[1] = 0x55; // Header
    modbus_data[2] = (timestamp >> 24) & 0xFF; // Timestamp
    modbus_data[3] = (timestamp >> 16) & 0xFF;
    modbus_data[4] = (timestamp >> 8) & 0xFF;
    modbus_data[5] = timestamp & 0xFF;
    modbus_data[6] = 0x25; // Temperature sensor 1 (37°C)
    modbus_data[7] = 0x1E; // Temperature sensor 2 (30°C)
    modbus_data[8] = 0x28; // Temperature sensor 3 (40°C)
    modbus_data[9] = 0x23; // Temperature sensor 4 (35°C)
    // Fill rest with zeros
    for (int i = 10; i < 32; i++) {
        modbus_data[i] = 0x00;
    }
    
    // Write to flash at address 0x1000
    if (Flash_WritePage(0x1000, modbus_data, 32) == 0) {
        Send_Debug_Data("Modbus data written to flash successfully\r\n");
        
        // Show what we wrote
        Send_Debug_Data("Data written: ");
        for (int i = 0; i < 32; i++) {
            char byte_msg[10];
            snprintf(byte_msg, sizeof(byte_msg), "%02X ", modbus_data[i]);
            Send_Debug_Data(byte_msg);
            if ((i + 1) % 16 == 0) Send_Debug_Data("\r\n");
        }
        Send_Debug_Data("\r\n");
    } else {
        Send_Debug_Data("Failed to write Modbus data to flash\r\n");
    }
}

/**
 * @brief Read Modbus Data from Flash
 */
void Flash_Read_Modbus_Data(void) {
    Send_Debug_Data("=== READING MODBUS DATA FROM FLASH ===\r\n");
    
    uint8_t read_data[32];
    
    // Read from flash at address 0x1000
    if (Flash_ReadData(0x1000, read_data, 32) == 0) {
        Send_Debug_Data("Modbus data read from flash successfully\r\n");
        
        // Show what we read
        Send_Debug_Data("Data read: ");
        for (int i = 0; i < 32; i++) {
            char byte_msg[10];
            snprintf(byte_msg, sizeof(byte_msg), "%02X ", read_data[i]);
            Send_Debug_Data(byte_msg);
            if ((i + 1) % 16 == 0) Send_Debug_Data("\r\n");
        }
        Send_Debug_Data("\r\n");
        
        // Interpret the data
        if (read_data[0] == 0xAA && read_data[1] == 0x55) {
            uint32_t timestamp = (read_data[2] << 24) | (read_data[3] << 16) | 
                                (read_data[4] << 8) | read_data[5];
            char msg[100];
            snprintf(msg, sizeof(msg), "Timestamp: %lu seconds\r\n", timestamp);
            Send_Debug_Data(msg);
            snprintf(msg, sizeof(msg), "T1: %d°C, T2: %d°C, T3: %d°C, T4: %d°C\r\n", 
                    read_data[6], read_data[7], read_data[8], read_data[9]);
            Send_Debug_Data(msg);
        }
    } else {
        Send_Debug_Data("Failed to read Modbus data from flash\r\n");
    }
}
