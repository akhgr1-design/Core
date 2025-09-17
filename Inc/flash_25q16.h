/**
  ******************************************************************************
  * @file    flash_25q16.h
  * @brief   25Q16BSIG SPI Flash Memory Driver Header (Read Only)
  ******************************************************************************
  */

#ifndef __FLASH_25Q16_H
#define __FLASH_25Q16_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spi.h"
#include <stdint.h>

/* Flash Memory Commands */
#define FLASH_CMD_READ_DATA         0x03
#define FLASH_CMD_DEVICE_ID         0x90
#define FLASH_CMD_READ_STATUS_REG   0x05

/* GPIO Pin Definitions */
#define FLASH_CS_PORT               GPIOB
#define FLASH_CS_PIN                GPIO_PIN_12

/* Function Prototypes */
uint8_t Flash_Init(void);
uint8_t Flash_ReadID(uint8_t *manufacturer, uint8_t *device_id);
uint8_t Flash_ReadData(uint32_t address, uint8_t *data, uint16_t length);
uint8_t Flash_WritePage(uint32_t address, uint8_t *data, uint16_t length);
uint8_t Flash_ReadStatusReg(void); // Renamed function
uint8_t Flash_WaitReady(void);
uint8_t Flash_WriteEnable(void);
uint8_t Flash_Test(void);
uint8_t Flash_TestSPI(void);
void Flash_Write_Modbus_Data(void);
void Flash_Read_Modbus_Data(void);
#ifdef __cplusplus
}
#endif

#endif /* __FLASH_25Q16_H */
