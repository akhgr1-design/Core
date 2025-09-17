#ifndef SPI_W5500_H
#define SPI_W5500_H

#include "main.h"
#include "spi.h"

// Function prototypes
void SPI_W5500_Init(void);
void SPI_W5500_CS_Enable(void);
void SPI_W5500_CS_Disable(void);

HAL_StatusTypeDef SPI_W5500_TransmitReceive(uint8_t *tx_data, uint8_t *rx_data, uint16_t size);
HAL_StatusTypeDef SPI_W5500_Transmit(uint8_t *tx_data, uint16_t size);
HAL_StatusTypeDef SPI_W5500_Receive(uint8_t *rx_data, uint16_t size);

void SPI_W5500_Test_Communication(void);

// W5500 Register Operations
void SPI_W5500_WriteReg(uint16_t reg, uint8_t *data, uint16_t len);
void SPI_W5500_ReadReg(uint16_t reg, uint8_t *data, uint16_t len);
uint8_t SPI_W5500_ReadRegByte(uint16_t reg);
void SPI_W5500_WriteRegByte(uint16_t reg, uint8_t data);

#endif /* SPI_W5500_H */
