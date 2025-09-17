/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : sd_card.h
  * @brief          : Header for SD Card functionality using SDMMC1
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __SD_CARD_H
#define __SD_CARD_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32h7xx_hal.h"

/* Exported types ------------------------------------------------------------*/
typedef enum {
    SD_CARD_OK = 0,
    SD_CARD_ERROR,
    SD_CARD_NOT_PRESENT,
    SD_CARD_INIT_FAILED,
    SD_CARD_WRITE_FAILED,
    SD_CARD_READ_FAILED
} SD_Card_Status_t;

/* Exported constants --------------------------------------------------------*/
#define SD_CARD_TIMEOUT_MS        5000
#define SD_CARD_BLOCK_SIZE        512
#define SD_CARD_MAX_FILENAME      64

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
SD_Card_Status_t SD_Card_Init(void);
SD_Card_Status_t SD_Card_DeInit(void);
SD_Card_Status_t SD_Card_IsPresent(void);
SD_Card_Status_t SD_Card_GetInfo(HAL_SD_CardInfoTypeDef *CardInfo);
SD_Card_Status_t SD_Card_WriteBlocks(uint8_t *pData, uint32_t BlockAdd, uint32_t NumberOfBlocks);
SD_Card_Status_t SD_Card_ReadBlocks(uint8_t *pData, uint32_t BlockAdd, uint32_t NumberOfBlocks);
SD_Card_Status_t SD_Card_WriteSingleBlock(uint8_t *pData, uint32_t BlockAdd);
SD_Card_Status_t SD_Card_ReadSingleBlock(uint8_t *pData, uint32_t BlockAdd);
SD_Card_Status_t SD_Card_EraseBlocks(uint32_t StartBlock, uint32_t EndBlock);

// Utility functions
void SD_Card_Process(void);
void SD_Card_Test(void);
void SD_Card_Status_Display(void);
uint8_t SD_Card_Is_Initialized(void);
void SD_Card_Capacity_Test(void);
void SD_Card_Diagnostic(void);
void SD_Card_Manual_Check(void);

void SD_Card_Text_Test(void);
void SD_Card_Simple_Test(void);

void SD_Card_Config_Check(void);
void SD_Card_Config_Test(void);

void SD_Card_Fix_Bus_Width(void);

void SD_Card_Simple_Test_Safe(void);

void SD_Card_Write_Diagnostic(void);
void SD_Card_Block_Size_Test(void);

void SD_Card_Simple_Diagnostic(void);
void SD_Card_Minimal_Test(void);

void SD_Card_Write_Protection_Test(void);
void SD_Card_Erase_Test(void);

void SD_Card_Post_Erase_Test(void);
void SD_Card_Write_Approaches_Test(void);

void SD_Card_Simple_Test_No_State_Change(void);

void SD_Card_Communication_Diagnostic(void);
void SD_Card_Reset_And_Reinit(void);

/* NEW ENHANCED FUNCTIONS */
SD_Card_Status_t SD_Card_Advanced_Init(void);
void SD_Card_Display_Info(void);
void SD_Card_Optimize_Bus_Width(void);
void SD_Card_Performance_Test(void);
void SD_Card_Multi_Block_Test(void);
SD_Card_Status_t SD_Card_Complete_Setup(void);
void SD_Card_Quick_Health_Check(void);

// Automatic testing functions
void SD_Card_Set_Auto_Test(uint8_t enable);
void SD_Card_Set_Auto_Test_Interval(uint32_t interval_ms);
void SD_Card_Auto_Test_Process(void);
void SD_Card_Auto_Basic_Test(void);
void SD_Card_Auto_Performance_Test(void);
void SD_Card_Auto_Text_Test(void);
void SD_Card_Auto_Multi_Block_Test(void);
void SD_Card_Auto_Status_Report(void);
void SD_Card_Run_Full_Auto_Test(void);
void SD_Card_Display_Auto_Test_Config(void);

void SD_Card_Emergency_Recovery(void);

// External variables for main.c access
extern SD_HandleTypeDef hsd1;
extern uint8_t sd_card_initialized;
extern uint8_t sd_card_present;

void SD_Card_Run_Automatic_Tests(void);

void SD_Card_Simple_Alphabet_Test(void);

#ifdef __cplusplus
}
#endif

#endif /* __SD_CARD_H */
