#include "w5500_driver.h"
#include "main.h"
#include "spi.h"
#include "gpio.h"
#include <string.h>
#include <stdio.h>

// External handles
extern SPI_HandleTypeDef hspi4;
extern UART_HandleTypeDef huart4;

//void W5500_Debug_Message(const char *message) {
   // extern void Send_Debug_Data(const char *message);
   // Send_Debug_Data(message);
//}

void W5500_Basic_SPI_Test(void) {
    W5500_Debug_Message("=== BASIC SPI LOOPBACK TEST ===\r\n");

    // Test 1: Check if SPI can do loopback (short MISO-MOSI)
    uint8_t tx_loopback[4] = {0xAA, 0x55, 0xAA, 0x55};
    uint8_t rx_loopback[4] = {0x00, 0x00, 0x00, 0x00};

    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET); // CS LOW
    HAL_Delay(2);
    HAL_SPI_TransmitReceive(&hspi4, tx_loopback, rx_loopback, 4, 1000);
    HAL_Delay(2);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET); // CS HIGH

    char msg[100];
    snprintf(msg, sizeof(msg), "Loopback TX: %02X %02X %02X %02X\r\n",
             tx_loopback[0], tx_loopback[1], tx_loopback[2], tx_loopback[3]);
    W5500_Debug_Message(msg);

    snprintf(msg, sizeof(msg), "Loopback RX: %02X %02X %02X %02X\r\n",
             rx_loopback[0], rx_loopback[1], rx_loopback[2], rx_loopback[3]);
    W5500_Debug_Message(msg);

    // Test 2: Manual slow SPI communication
    W5500_Debug_Message("=== MANUAL SLOW SPI TEST ===\r\n");

    for (int i = 0; i < 4; i++) {
        uint8_t tx_byte = 0x80 + i;
        uint8_t rx_byte = 0x00;

        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET); // CS LOW
        HAL_Delay(5);
        HAL_SPI_TransmitReceive(&hspi4, &tx_byte, &rx_byte, 1, 1000);
        HAL_Delay(5);
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET); // CS HIGH
        HAL_Delay(10);

        snprintf(msg, sizeof(msg), "Byte %d: TX=0x%02X, RX=0x%02X\r\n", i, tx_byte, rx_byte);
        W5500_Debug_Message(msg);
    }
}

void W5500_Hardware_Check(void) {
    W5500_Debug_Message("=== HARDWARE CONNECTION CHECK ===\r\n");

    // Check physical connections
    W5500_Debug_Message("1. Check 3.3V power to W5500\r\n");
    W5500_Debug_Message("2. Check GND connection\r\n");
    W5500_Debug_Message("3. Check MISO line (PE13 to W5500 pin 34)\r\n");
    W5500_Debug_Message("4. Check MOSI line (PE14 to W5500 pin 35)\r\n");
    W5500_Debug_Message("5. Check SCK line (PE12 to W5500 pin 33)\r\n");
    W5500_Debug_Message("6. Check CS line (PE11 to W5500 pin 32)\r\n");
    W5500_Debug_Message("7. Check RST line (PB10 to W5500 pin 37)\r\n");
}

uint8_t W5500_Simple_Test(void) {
    W5500_Debug_Message("=== SIMPLE W5500 TEST ===\r\n");

    // Try to read version register with very slow timing
    uint8_t tx_data[4] = {0x00, 0x39, 0x00, 0x00};
    uint8_t rx_data[4] = {0x00, 0x00, 0x00, 0x00};

    // Very slow communication
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET); // CS LOW
    HAL_Delay(10);
    HAL_SPI_TransmitReceive(&hspi4, tx_data, rx_data, 4, 1000);
    HAL_Delay(10);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET); // CS HIGH
    HAL_Delay(50);

    char msg[100];
    snprintf(msg, sizeof(msg), "Slow Test TX: %02X %02X %02X %02X\r\n",
             tx_data[0], tx_data[1], tx_data[2], tx_data[3]);
    W5500_Debug_Message(msg);

    snprintf(msg, sizeof(msg), "Slow Test RX: %02X %02X %02X %02X\r\n",
             rx_data[0], rx_data[1], rx_data[2], rx_data[3]);
    W5500_Debug_Message(msg);

    if (rx_data[3] == 0x04) {
        W5500_Debug_Message("W5500 responded correctly! ✓\r\n");
        return 1;
    }

    W5500_Debug_Message("W5500 not responding correctly ✗\r\n");
    return 0;
}
