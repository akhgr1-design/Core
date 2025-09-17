#ifndef SPI_CONTROLLER_H
#define SPI_CONTROLLER_H

#include "stm32h7xx_hal.h"

// SPI Configuration
#define SPI_TIMEOUT 1000

// Function prototypes
void spi_init(SPI_HandleTypeDef *hspi);
uint8_t spi_transfer(uint8_t data);
void spi_write(uint8_t *data, uint16_t size);
void spi_read(uint8_t *data, uint16_t size);

// W5500 Specific functions
void SPI_WriteRegByte(uint32_t reg, uint8_t value);
uint8_t SPI_ReadRegByte(uint32_t reg);
void SPI_WriteRegBlock(uint32_t reg, uint8_t *data, uint16_t size);
void SPI_ReadRegBlock(uint32_t reg, uint8_t *data, uint16_t size);

// CS Control
void spi_cs_select(void);
void spi_cs_deselect(void);

// Error handling
uint8_t spi_check_errors(void);
void spi_reset(void);

#endif /* SPI_CONTROLLER_H */
