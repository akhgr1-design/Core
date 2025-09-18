#pragma once

#include "stm32h7xx_hal.h"

// Network Configuration
#define W5500_IP_ADDR      {192, 168, 8, 70}    // Your static IP
#define W5500_SUBNET       {255, 255, 255, 0}   // Subnet mask
#define W5500_GATEWAY      {192, 168, 8, 1}     // Gateway (usually router IP)
#define W5500_MAC_ADDR     {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}  // Unique MAC

// SPI Configuration
#define W5500_SPI_HANDLE   hspi4        // Match your SPI handle
#define W5500_SPI_TIMEOUT  1000         // Timeout in ms

// GPIO Definitions - REPLACE WITH YOUR ACTUAL PINS
#define W5500_CS_PIN       GPIO_PIN_4
#define W5500_CS_PORT      GPIOA
#define W5500_RST_PIN      GPIO_PIN_5
#define W5500_RST_PORT     GPIOA

// Wiznet Constants
#define WIZ_OK            1
#define CW_GET_LINK_STATUS 0x13
