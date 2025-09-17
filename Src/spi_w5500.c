#include "spi_w5500.h"
#include "spi.h"
#include "uart_comm.h"
#include <string.h>
#include <stdio.h>

// External SPI handle
extern SPI_HandleTypeDef hspi4;

// W5500 Chip Select Pin
#define W5500_CS_PORT    GPIOE
#define W5500_CS_PIN     GPIO_PIN_11

void SPI_W5500_Init(void) {
    // CS pin initialization
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = W5500_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(W5500_CS_PORT, &GPIO_InitStruct);
    SPI_W5500_CS_Disable(); // Start with CS high (inactive)

    Send_Debug_Data("SPI_W5500: Initialized\r\n");
}

void SPI_W5500_CS_Enable(void) {
    HAL_GPIO_WritePin(W5500_CS_PORT, W5500_CS_PIN, GPIO_PIN_RESET);
}

void SPI_W5500_CS_Disable(void) {
    HAL_GPIO_WritePin(W5500_CS_PORT, W5500_CS_PIN, GPIO_PIN_SET);
}

HAL_StatusTypeDef SPI_W5500_TransmitReceive(uint8_t *tx_data, uint8_t *rx_data, uint16_t size) {
    SPI_W5500_CS_Enable();
    HAL_Delay(1);
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi4, tx_data, rx_data, size, 1000);
    HAL_Delay(1);
    SPI_W5500_CS_Disable();
    return status;
}

HAL_StatusTypeDef SPI_W5500_Transmit(uint8_t *tx_data, uint16_t size) {
    SPI_W5500_CS_Enable();
    HAL_Delay(1);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi4, tx_data, size, 1000);
    HAL_Delay(1);
    SPI_W5500_CS_Disable();
    return status;
}

HAL_StatusTypeDef SPI_W5500_Receive(uint8_t *rx_data, uint16_t size) {
    SPI_W5500_CS_Enable();
    HAL_Delay(1);
    HAL_StatusTypeDef status = HAL_SPI_Receive(&hspi4, rx_data, size, 1000);
    HAL_Delay(1);
    SPI_W5500_CS_Disable();
    return status;
}

void SPI_W5500_Test_Communication(void) {
    uint8_t tx_data[4] = {0x55, 0xAA, 0x55, 0xAA};
    uint8_t rx_data[4] = {0x00, 0x00, 0x00, 0x00};

    Send_Debug_Data("=== SPI Communication Test ===\r\n");

    HAL_StatusTypeDef status = SPI_W5500_TransmitReceive(tx_data, rx_data, 4);

    char msg[100];
    snprintf(msg, sizeof(msg), "SPI Status: %d (0=OK)\r\n", status);
    Send_Debug_Data(msg);

    snprintf(msg, sizeof(msg), "TX: %02X %02X %02X %02X\r\n",
             tx_data[0], tx_data[1], tx_data[2], tx_data[3]);
    Send_Debug_Data(msg);

    snprintf(msg, sizeof(msg), "RX: %02X %02X %02X %02X\r\n",
             rx_data[0], rx_data[1], rx_data[2], rx_data[3]);
    Send_Debug_Data(msg);

    if (status == HAL_OK) {
        Send_Debug_Data("SPI Test: SUCCESS ✓\r\n");
    } else {
        Send_Debug_Data("SPI Test: FAILED ✗\r\n");
    }
}

// W5500 Register Operations
void SPI_W5500_WriteReg(uint16_t reg, uint8_t *data, uint16_t len) {
    uint8_t tx_buffer[len + 3];
    
    // W5500 Write Format: [Register High, Register Low, Control Byte]
    tx_buffer[0] = (reg >> 8) & 0xFF;    // Register high byte
    tx_buffer[1] = reg & 0xFF;           // Register low byte
    tx_buffer[2] = 0x04;                 // Control byte: Write operation, Block 0
    
    // Copy data
    if (len > 0 && data != NULL) {
        memcpy(&tx_buffer[3], data, len);
    }
    
    // Debug: Print what we're sending
    char msg[200];
    snprintf(msg, sizeof(msg), "Writing to reg 0x%04X: ", reg);
    for (int i = 0; i < len && i < 8; i++) {
        char temp[10];
        snprintf(temp, sizeof(temp), "%02X ", data[i]);
        strcat(msg, temp);
    }
    strcat(msg, "\r\n");
    Send_Debug_Data(msg);
    
    // Send the complete packet
    SPI_W5500_Transmit(tx_buffer, len + 3);
}

void SPI_W5500_ReadReg(uint16_t reg, uint8_t *data, uint16_t len) {
    uint8_t tx_buffer[len + 3];
    uint8_t rx_buffer[len + 3];
    
    // W5500 Read Format: [Register High, Register Low, Control Byte]
    tx_buffer[0] = (reg >> 8) & 0xFF;    // Register high byte
    tx_buffer[1] = reg & 0xFF;           // Register low byte
    tx_buffer[2] = 0x00;                 // Control byte: Read operation, Block 0
    
    // Clear rest of transmit buffer
    memset(&tx_buffer[3], 0x00, len);
    
    // Use the working SPI function
    SPI_W5500_TransmitReceive(tx_buffer, rx_buffer, len + 3);
    
    // Copy received data (skip first 3 bytes: register + control)
    if (len > 0 && data != NULL) {
        memcpy(data, &rx_buffer[3], len);
    }
}

uint8_t SPI_W5500_ReadRegByte(uint16_t reg) {
    uint8_t data;
    SPI_W5500_ReadReg(reg, &data, 1);
    return data;
}

void SPI_W5500_WriteRegByte(uint16_t reg, uint8_t data) {
    SPI_W5500_WriteReg(reg, &data, 1);
}
