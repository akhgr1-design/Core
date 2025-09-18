#ifndef __W5500_PLATFORM_H
#define __W5500_PLATFORM_H

#include "w5500_conf.h"
//#include "wizchip_conf.h"

// Platform Initialization Functions
void W5500_Platform_Init(void);
void W5500_CS_Select(void);
void W5500_CS_Deselect(void);
void W5500_Reset(void);
uint8_t W5500_SPI_SendRecv(uint8_t data);

// SPI Transaction Handlers
void W5500_SPI_TransmitReceive(uint8_t* txData, uint8_t* rxData, uint16_t size);
void W5500_SPI_Transmit(uint8_t* txData, uint16_t size);
void W5500_SPI_Receive(uint8_t* rxData, uint16_t size);

// Add these prototypes
uint8_t W5500_SPI_ReadByte(void);
void W5500_SPI_WriteByte(uint8_t wb);

#endif /* __W5500_PLATFORM_H */
