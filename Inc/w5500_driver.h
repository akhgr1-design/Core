#ifndef W5500_DRIVER_H
#define W5500_DRIVER_H

#include "main.h"
#include <stdint.h>

// W5500 Register Addresses (CORRECTED)
#define W5500_MR            0x0000  // Mode Register
#define W5500_GAR           0x0001  // Gateway Address Register  
#define W5500_SUBR          0x0005  // Subnet Mask Register
#define W5500_SHAR          0x0009  // Source Hardware Address (MAC)
#define W5500_SIPR          0x000F  // Source IP Address
#define W5500_IR            0x0015  // Interrupt Register
#define W5500_IMR           0x0016  // Interrupt Mask Register
#define W5500_RTR           0x0017  // Retry Timeout Register
#define W5500_RCR           0x0019  // Retry Count Register
#define W5500_PTIMER        0x0028  // PPP LCP Request Timer
#define W5500_PMAGIC        0x0029  // PPP LCP Magic Number
#define W5500_PHYCFGR       0x002E  // PHY Configuration Register
#define W5500_VERSION       0x0039  // Chip Version Register

// Socket Registers (ADD THESE)
#define W5500_SOCK0_MR      0x0400  // Socket 0 Mode Register
#define W5500_SOCK0_CR      0x0401  // Socket 0 Command Register
#define W5500_SOCK0_IR      0x0402  // Socket 0 Interrupt Register
#define W5500_SOCK0_SR      0x0403  // Socket 0 Status Register
#define W5500_SOCK0_PORT    0x0404  // Socket 0 Source Port
#define W5500_SOCK0_DHAR    0x0406  // Socket 0 Destination Hardware Address
#define W5500_SOCK0_DIPR    0x040C  // Socket 0 Destination IP Address
#define W5500_SOCK0_DPORT   0x0410  // Socket 0 Destination Port

// Default MAC Address
#define W5500_MAC_ADDR      {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}

// GPIO Definitions
#define W5500_RST_PORT      GPIOB
#define W5500_RST_PIN       GPIO_PIN_10

// Function Declarations
void W5500_Reset(void);
void W5500_Test_CS(void);
void W5500_Test_Reset(void);
uint8_t W5500_Test_SPI_Modes(void);
uint8_t W5500_SelfTest(void);
uint8_t W5500_Init(void);
uint8_t W5500_GetVersion(void);
void W5500_ReadMAC(uint8_t *mac);
void W5500_ConfigureNetwork(uint8_t *mac, uint8_t *ip, uint8_t *subnet, uint8_t *gateway);
void W5500_CheckMode(void);
uint8_t W5500_GetDHCPStatus(void);
void W5500_GetIPConfig(uint8_t *ip, uint8_t *subnet, uint8_t *gateway, uint8_t *dns);
uint8_t W5500_CheckLinkStatus(void);
void W5500_Debug_Message(const char *message);

// Core W5500 I/O (now properly defined)
void W5500_WriteByte(uint16_t addr, uint8_t data);
uint8_t W5500_ReadByte(uint16_t addr);

#endif /* W5500_DRIVER_H */
