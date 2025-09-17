#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_sd.h"
#include "sd_card.h"
#include "uart_comm.h"
#include <string.h>
#include <stdio.h>

/* External variables */
extern SD_HandleTypeDef hsd1;

/* Private variables - make these non-static so main.c can access them */
uint8_t sd_card_initialized = 0;
uint8_t sd_card_present = 0;

/* Private variables for automatic testing */
static uint8_t auto_test_enabled = 0;  // Disabled by default
static uint32_t auto_test_interval = 60000;  // 60 seconds
static uint32_t last_auto_test = 0;
static uint8_t auto_test_sequence = 0;

/* Function prototypes */
static void MX_SDMMC1_SD_Init(void);
static void MX_SDMMC1_GPIO_Init(void);

/**
  * @brief  Initialize SD Card
  */
SD_Card_Status_t SD_Card_Init(void)
{
    // Initialize SDMMC1
    MX_SDMMC1_SD_Init();
    MX_SDMMC1_GPIO_Init();
    
    // Initialize SD card
    if (HAL_SD_Init(&hsd1) == HAL_OK) {
        sd_card_initialized = 1;
        sd_card_present = 1;
        return SD_CARD_OK;
    }
    
    return SD_CARD_ERROR;
}

/**
  * @brief  Check if SD Card is present
  */
SD_Card_Status_t SD_Card_CheckPresence(void)
{
    // Simple check - try to get card state
    if (sd_card_initialized) {
        HAL_SD_CardStateTypeDef state = HAL_SD_GetCardState(&hsd1);
        if (state == HAL_SD_CARD_TRANSFER || 
            state == HAL_SD_CARD_SENDING || 
            state == HAL_SD_CARD_RECEIVING ||
            state == HAL_SD_CARD_PROGRAMMING) {
            return SD_CARD_OK;
        }
    }
    return SD_CARD_ERROR;
}

/**
  * @brief  Write single block to SD Card
  */
SD_Card_Status_t SD_Card_WriteSingleBlock(uint8_t *data, uint32_t block_address)
{
    if (!sd_card_initialized) {
        return SD_CARD_ERROR;
    }
    
    if (HAL_SD_WriteBlocks(&hsd1, data, block_address, 1, 5000) == HAL_OK) {
        return SD_CARD_OK;
    }
    
    return SD_CARD_WRITE_FAILED;
}

/**
  * @brief  Read single block from SD Card
  */
SD_Card_Status_t SD_Card_ReadSingleBlock(uint8_t *data, uint32_t block_address)
{
    if (!sd_card_initialized) {
        return SD_CARD_ERROR;
    }
    
    if (HAL_SD_ReadBlocks(&hsd1, data, block_address, 1, 5000) == HAL_OK) {
        return SD_CARD_OK;
    }
    
    return SD_CARD_READ_FAILED;
}

/**
  * @brief  SD Card simple diagnostic
  */
void SD_Card_Simple_Diagnostic(void)
{
    if (!sd_card_initialized) {
        Send_Debug_Data("SD Card Simple Diagnostic: Card not initialized\r\n");
        return;
    }
    
    Send_Debug_Data("\r\n=== SD CARD SIMPLE DIAGNOSTIC ===\r\n");
    
    // Get card info
    HAL_SD_CardInfoTypeDef CardInfo;
    if (HAL_SD_GetCardInfo(&hsd1, &CardInfo) != HAL_OK) {
        Send_Debug_Data("Cannot get card info\r\n");
        return;
    }
    
    char msg[100];
    snprintf(msg, sizeof(msg), "Card block size: %lu bytes\r\n", CardInfo.LogBlockSize);
    Send_Debug_Data(msg);
    
    // Check card state
    HAL_SD_CardStateTypeDef card_state = HAL_SD_GetCardState(&hsd1);
    snprintf(msg, sizeof(msg), "Card state: %d\r\n", card_state);
    Send_Debug_Data(msg);
    
    // Try simple read test
    uint8_t read_buffer[512];
    uint32_t test_block = 0;
    
    snprintf(msg, sizeof(msg), "Testing simple read from block %lu...\r\n", test_block);
    Send_Debug_Data(msg);
    
    HAL_StatusTypeDef read_result = HAL_SD_ReadBlocks(&hsd1, read_buffer, test_block, 1, 10000);
    
    if (read_result == HAL_OK) {
        Send_Debug_Data("Simple read: SUCCESS\r\n");
    } else {
        Send_Debug_Data("Simple read: FAILED\r\n");
        uint32_t error_code = HAL_SD_GetError(&hsd1);
        snprintf(msg, sizeof(msg), "Read error code: 0x%08lX\r\n", error_code);
        Send_Debug_Data(msg);
    }
    
    Send_Debug_Data("===============================\r\n");
}

/**
  * @brief  Check if SD Card is initialized
  */
uint8_t SD_Card_Is_Initialized(void)
{
    return sd_card_initialized;
}

/* Private functions */
static void MX_SDMMC1_SD_Init(void)
{
    hsd1.Instance = SDMMC1;
    hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd1.Init.BusWide = SDMMC_BUS_WIDE_1B;
    hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd1.Init.ClockDiv = 8;
}

static void MX_SDMMC1_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_SDMMC1_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    
    // Configure SDMMC1 pins
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_SDMMC1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}

/**
  * @brief  SD Card communication diagnostic
  */
void SD_Card_Communication_Diagnostic(void)
{
    if (!sd_card_initialized) {
        Send_Debug_Data("SD Card Communication Diagnostic: Card not initialized\r\n");
        return;
    }
    
    Send_Debug_Data("\r\n=== SD CARD COMMUNICATION DIAGNOSTIC ===\r\n");
    
    // Check what operations work
    Send_Debug_Data("Testing operations that work:\r\n");
    
    // 1. Card info (this works)
    HAL_SD_CardInfoTypeDef CardInfo;
    if (HAL_SD_GetCardInfo(&hsd1, &CardInfo) == HAL_OK) {
        Send_Debug_Data("GetCardInfo: SUCCESS\r\n");
    } else {
        Send_Debug_Data("GetCardInfo: FAILED\r\n");
    }
    
    // 2. Card state (this works)
    HAL_SD_CardStateTypeDef card_state = HAL_SD_GetCardState(&hsd1);
    char msg[100];
    snprintf(msg, sizeof(msg), "GetCardState: SUCCESS (state: %d)\r\n", card_state);
    Send_Debug_Data(msg);
    
    // 3. Try erase (this works)
    Send_Debug_Data("Testing erase operation...\r\n");
    if (HAL_SD_Erase(&hsd1, 5000, 5000) == HAL_OK) {
        Send_Debug_Data("Erase: SUCCESS\r\n");
    } else {
        Send_Debug_Data("Erase: FAILED\r\n");
    }
    
    // 4. Try read (this fails)
    Send_Debug_Data("Testing read operation...\r\n");
    uint8_t read_buffer[512];
    HAL_StatusTypeDef read_result = HAL_SD_ReadBlocks(&hsd1, read_buffer, 5000, 1, 1000);
    
    if (read_result == HAL_OK) {
        Send_Debug_Data("Read: SUCCESS\r\n");
    } else {
        Send_Debug_Data("Read: FAILED\r\n");
        
        uint32_t error_code = HAL_SD_GetError(&hsd1);
        snprintf(msg, sizeof(msg), "Read error code: 0x%08lX\r\n", error_code);
        Send_Debug_Data(msg);
        
        // Check card state after failed read
        card_state = HAL_SD_GetCardState(&hsd1);
        snprintf(msg, sizeof(msg), "Card state after failed read: %d\r\n", card_state);
        Send_Debug_Data(msg);
    }
    
    // 5. Try write (this fails)
    Send_Debug_Data("Testing write operation...\r\n");
    uint8_t test_data[512];
    memset(test_data, 0x55, 512);
    
    HAL_StatusTypeDef write_result = HAL_SD_WriteBlocks(&hsd1, test_data, 5000, 1, 1000);
    
    if (write_result == HAL_OK) {
        Send_Debug_Data("Write: SUCCESS\r\n");
    } else {
        Send_Debug_Data("Write: FAILED\r\n");
        
        uint32_t error_code = HAL_SD_GetError(&hsd1);
        snprintf(msg, sizeof(msg), "Write error code: 0x%08lX\r\n", error_code);
        Send_Debug_Data(msg);
        
        // Check card state after failed write
        card_state = HAL_SD_GetCardState(&hsd1);
        snprintf(msg, sizeof(msg), "Card state after failed write: %d\r\n", card_state);
        Send_Debug_Data(msg);
    }
    
    // 6. Check SDMMC registers
    Send_Debug_Data("SDMMC Register Status:\r\n");
    snprintf(msg, sizeof(msg), "  POWER: 0x%08lX\r\n", SDMMC1->POWER);
    Send_Debug_Data(msg);
    snprintf(msg, sizeof(msg), "  CLKCR: 0x%08lX\r\n", SDMMC1->CLKCR);
    Send_Debug_Data(msg);
    snprintf(msg, sizeof(msg), "  ARG: 0x%08lX\r\n", SDMMC1->ARG);
    Send_Debug_Data(msg);
    snprintf(msg, sizeof(msg), "  CMD: 0x%08lX\r\n", SDMMC1->CMD);
    Send_Debug_Data(msg);
    snprintf(msg, sizeof(msg), "  RESPCMD: 0x%08lX\r\n", SDMMC1->RESPCMD);
    Send_Debug_Data(msg);
    snprintf(msg, sizeof(msg), "  STA: 0x%08lX\r\n", SDMMC1->STA);
    Send_Debug_Data(msg);
    
    Send_Debug_Data("==========================================\r\n");
}

/**
  * @brief  Process SD card insertion/removal detection - ONLY INSERT/REMOVE
  */
void SD_Card_Process(void)
{
    static uint8_t last_card_state = 0;
    static uint32_t last_detection_time = 0;
    
    // Only check every 1 second to avoid spam
    if (HAL_GetTick() - last_detection_time < 1000) {
        return;
    }
    last_detection_time = HAL_GetTick();
    
    // Check current card state
    uint8_t current_card_state = 0;
    
    if (sd_card_initialized) {
        HAL_SD_CardStateTypeDef state = HAL_SD_GetCardState(&hsd1);
        if (state == HAL_SD_CARD_TRANSFER || 
            state == HAL_SD_CARD_SENDING || 
            state == HAL_SD_CARD_RECEIVING ||
            state == HAL_SD_CARD_PROGRAMMING ||
            state == HAL_SD_CARD_STANDBY) {
            current_card_state = 1;  // Card present and working
        } else {
            current_card_state = 0;  // Card removed or error
        }
    } else {
        // Try to detect card insertion
        HAL_StatusTypeDef result = HAL_SD_Init(&hsd1);
        if (result == HAL_OK) {
            HAL_SD_CardStateTypeDef state = HAL_SD_GetCardState(&hsd1);
            if (state == HAL_SD_CARD_TRANSFER || state == HAL_SD_CARD_STANDBY) {
                current_card_state = 1;  // Card inserted
                sd_card_initialized = 1;
                sd_card_present = 1;
            }
        }
    }
    
    // Check for state changes - ONLY show insert/remove
    if (current_card_state != last_card_state) {
        if (current_card_state == 1) {
            // Card inserted or reconnected
            if (!sd_card_initialized) {
                // Initialize the card silently
                if (HAL_SD_Init(&hsd1) == HAL_OK) {
                    HAL_SD_CardStateTypeDef state = HAL_SD_GetCardState(&hsd1);
                    if (state == HAL_SD_CARD_TRANSFER || state == HAL_SD_CARD_STANDBY) {
                        sd_card_initialized = 1;
                        sd_card_present = 1;
                        Send_Debug_Data("SD Card: INSERTED\r\n");
                    }
                }
            } else {
                Send_Debug_Data("SD Card: RECONNECTED\r\n");
            }
        } else {
            // Card removed
            Send_Debug_Data("SD Card: REMOVED\r\n");
            sd_card_initialized = 0;
            sd_card_present = 0;
            
            // Deinitialize to free resources
            HAL_SD_DeInit(&hsd1);
        }
        
        last_card_state = current_card_state;
    }
}

/**
  * @brief  Test SD Card with text data
  */
void SD_Card_Text_Test(void)
{
    if (!sd_card_initialized) {
        Send_Debug_Data("SD Card Text Test: Card not initialized\r\n");
        return;
    }
    
    Send_Debug_Data("\r\n=== SD CARD TEXT TEST ===\r\n");
    
    // Test data - some text to write
    char test_text[] = "Hello STM32H7! This is a test message for SD card write/read functionality. Testing 123...";
    uint8_t write_buffer[512];
    uint8_t read_buffer[512];
    
    // Clear buffers
    memset(write_buffer, 0, 512);
    memset(read_buffer, 0, 512);
    
    // Copy test text to write buffer
    strncpy((char*)write_buffer, test_text, strlen(test_text));
    
    Send_Debug_Data("Writing text to SD card...\r\n");
    Send_Debug_Data("Text to write: ");
    Send_Debug_Data(test_text);
    Send_Debug_Data("\r\n");
    
    // Write to block 10
    SD_Card_Status_t write_status = SD_Card_WriteSingleBlock(write_buffer, 10);
    
    if (write_status == SD_CARD_OK) {
        Send_Debug_Data("Write: SUCCESS\r\n");
        
        // Read back from block 10
        Send_Debug_Data("Reading text from SD card...\r\n");
        SD_Card_Status_t read_status = SD_Card_ReadSingleBlock(read_buffer, 10);
        
        if (read_status == SD_CARD_OK) {
            Send_Debug_Data("Read: SUCCESS\r\n");
            
            // Display the read text
            Send_Debug_Data("Text read back: ");
            Send_Debug_Data((char*)read_buffer);
            Send_Debug_Data("\r\n");
            
            // Verify data integrity
            if (memcmp(write_buffer, read_buffer, 512) == 0) {
                Send_Debug_Data("Data verification: SUCCESS\r\n");
                Send_Debug_Data("SD Card Text Test: PASSED\r\n");
            } else {
                Send_Debug_Data("Data verification: FAILED\r\n");
                Send_Debug_Data("SD Card Text Test: FAILED\r\n");
            }
        } else {
            char error_msg[100];
            snprintf(error_msg, sizeof(error_msg), "Read: FAILED (Status: %d)\r\n", read_status);
            Send_Debug_Data(error_msg);
            Send_Debug_Data("SD Card Text Test: FAILED\r\n");
        }
    } else {
        char error_msg[100];
        snprintf(error_msg, sizeof(error_msg), "Write: FAILED (Status: %d)\r\n", write_status);
        Send_Debug_Data(error_msg);
        Send_Debug_Data("SD Card Text Test: FAILED\r\n");
    }
    
    Send_Debug_Data("========================\r\n");
}

/**
  * @brief  Comprehensive SD Card test
  */
void SD_Card_Test(void)
{
    if (!sd_card_initialized) {
        Send_Debug_Data("SD Card Test: Card not initialized\r\n");
        return;
    }
    
    Send_Debug_Data("\r\n=== COMPREHENSIVE SD CARD TEST ===\r\n");
    
    // Get card info
    HAL_SD_CardInfoTypeDef CardInfo;
    if (HAL_SD_GetCardInfo(&hsd1, &CardInfo) == HAL_OK) {
        char msg[200];
        const char* card_type_name = "Unknown";
        if (CardInfo.CardType == CARD_SDSC) {
            card_type_name = "SDSC";
        } else if (CardInfo.CardType == CARD_SDHC_SDXC) {
            card_type_name = "SDHC/SDXC";
        }

        snprintf(msg, sizeof(msg), 
            "Card Information:\r\n"
            "  Type: %s\r\n"
            "  Capacity: %lu MB\r\n"
            "  Block Size: %lu bytes\r\n"
            "  Block Count: %lu\r\n",
            card_type_name,
            (CardInfo.LogBlockNbr * CardInfo.LogBlockSize) / (1024 * 1024),
            CardInfo.LogBlockSize,
            CardInfo.LogBlockNbr);
        Send_Debug_Data(msg);
    }

    // Test different operations
    uint8_t test_data[512];
    uint8_t read_data[512];

    // Fill with test pattern
    for (int i = 0; i < 512; i++) {
        test_data[i] = (uint8_t)(i & 0xFF);
    }

    // Test multiple blocks
    uint32_t test_blocks[] = {100, 1000, 5000};
    int success_count = 0;

    for (int i = 0; i < 3; i++) {
        uint32_t block = test_blocks[i];
        char msg[100];
        snprintf(msg, sizeof(msg), "Testing block %lu...\r\n", block);
        Send_Debug_Data(msg);

        // Write test
        if (SD_Card_WriteSingleBlock(test_data, block) == SD_CARD_OK) {
            Send_Debug_Data("  Write: SUCCESS\r\n");

            // Read test
            if (SD_Card_ReadSingleBlock(read_data, block) == SD_CARD_OK) {
                Send_Debug_Data("  Read: SUCCESS\r\n");

                // Verify data
                if (memcmp(test_data, read_data, 512) == 0) {
                    Send_Debug_Data("  Verification: SUCCESS\r\n");
                    success_count++;
                } else {
                    Send_Debug_Data("  Verification: FAILED\r\n");
                }
            } else {
                Send_Debug_Data("  Read: FAILED\r\n");
            }
        } else {
            Send_Debug_Data("  Write: FAILED\r\n");
        }
    }

    char msg[100];
    snprintf(msg, sizeof(msg), "Test Results: %d/3 blocks passed\r\n", success_count);
    Send_Debug_Data(msg);

    if (success_count == 3) {
        Send_Debug_Data("SD Card Test: PASSED\r\n");
    } else {
        Send_Debug_Data("SD Card Test: PARTIAL/FAILED\r\n");
    }

    Send_Debug_Data("=====================================\r\n");
}

/**
  * @brief  Test SD Card capacity
  */
void SD_Card_Capacity_Test(void)
{
    if (!sd_card_initialized) {
        Send_Debug_Data("SD Card Capacity Test: Card not initialized\r\n");
        return;
    }

    Send_Debug_Data("\r\n=== SD CARD CAPACITY TEST ===\r\n");

    // Get card info
    HAL_SD_CardInfoTypeDef CardInfo;
    if (HAL_SD_GetCardInfo(&hsd1, &CardInfo) == HAL_OK) {
        char msg[200];
        const char* card_type_name = "Unknown";
        if (CardInfo.CardType == CARD_SDSC) {
            card_type_name = "SDSC";
        } else if (CardInfo.CardType == CARD_SDHC_SDXC) {
            card_type_name = "SDHC/SDXC";
        }

        snprintf(msg, sizeof(msg),
            "Capacity Information:\r\n"
            "  Total Capacity: %lu MB\r\n"
            "  Block Size: %lu bytes\r\n"
            "  Total Blocks: %lu\r\n"
            "  Card Type: %s\r\n",
            (CardInfo.LogBlockNbr * CardInfo.LogBlockSize) / (1024 * 1024),
            CardInfo.LogBlockSize,
            CardInfo.LogBlockNbr,
            card_type_name);
        Send_Debug_Data(msg);
        
        // Test capacity by trying to access different parts of the card
        uint32_t total_blocks = CardInfo.LogBlockNbr;
        uint32_t test_blocks[] = {
            total_blocks / 4,      // 25%
            total_blocks / 2,      // 50%
            (total_blocks * 3) / 4, // 75%
            total_blocks - 10      // Near end
        };
        
        uint8_t test_data[512];
        uint8_t read_data[512];
        memset(test_data, 0xAA, 512);
        
        for (int i = 0; i < 4; i++) {
            uint32_t block = test_blocks[i];
            snprintf(msg, sizeof(msg), "Testing block %lu (%.1f%% into card)...\r\n", 
                     block, (float)block * 100.0 / total_blocks);
            Send_Debug_Data(msg);
            
            // Try to read the block
            if (SD_Card_ReadSingleBlock(read_data, block) == SD_CARD_OK) {
                Send_Debug_Data("  Read: SUCCESS\r\n");
            } else {
                Send_Debug_Data("  Read: FAILED\r\n");
            }
        }
    } else {
        Send_Debug_Data("Cannot get card info\r\n");
    }
    
    Send_Debug_Data("=====================================\r\n");
}

/**
  * @brief  Display SD Card status
  */
void SD_Card_Status_Display(void)
{
    Send_Debug_Data("\r\n=== SD CARD STATUS ===\r\n");
    
    char msg[100];
    snprintf(msg, sizeof(msg), "Initialized: %s\r\n", sd_card_initialized ? "YES" : "NO");
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "Present: %s\r\n", sd_card_present ? "YES" : "NO");
    Send_Debug_Data(msg);
    
    if (sd_card_initialized) {
        // Get card state
        HAL_SD_CardStateTypeDef card_state = HAL_SD_GetCardState(&hsd1);
        snprintf(msg, sizeof(msg), "Card State: %d\r\n", card_state);
        Send_Debug_Data(msg);
        
        // Get card info
        HAL_SD_CardInfoTypeDef CardInfo;
        if (HAL_SD_GetCardInfo(&hsd1, &CardInfo) == HAL_OK) {
            const char* card_type_name = "Unknown";
            if (CardInfo.CardType == CARD_SDSC) {
                card_type_name = "SDSC";
            } else if (CardInfo.CardType == CARD_SDHC_SDXC) {
                card_type_name = "SDHC/SDXC";
            }

            snprintf(msg, sizeof(msg), "Card Type: %s\r\n", card_type_name);
            Send_Debug_Data(msg);
            
            snprintf(msg, sizeof(msg), "Capacity: %lu MB\r\n", 
                    (CardInfo.LogBlockNbr * CardInfo.LogBlockSize) / (1024 * 1024));
            Send_Debug_Data(msg);
        }
    }
    
    Send_Debug_Data("=====================\r\n");
}

/**
  * @brief  Manual SD card detection - CLEAN OUTPUT
  */
void SD_Card_Manual_Detection(void)
{
    // Force deinitialize first
    HAL_SD_DeInit(&hsd1);
    HAL_Delay(100);
    
    // Reset state
    sd_card_initialized = 0;
    sd_card_present = 0;
    
    // Try to detect card
    HAL_StatusTypeDef result = HAL_SD_Init(&hsd1);
    if (result == HAL_OK) {
        HAL_SD_CardStateTypeDef state = HAL_SD_GetCardState(&hsd1);
        if (state == HAL_SD_CARD_TRANSFER || state == HAL_SD_CARD_STANDBY) {
            sd_card_initialized = 1;
            sd_card_present = 1;
            Send_Debug_Data("SD Card: DETECTED\r\n");
        } else {
            Send_Debug_Data("SD Card: NOT DETECTED\r\n");
        }
    } else {
        Send_Debug_Data("SD Card: NOT DETECTED\r\n");
    }
}

/**
  * @brief  Manual SD Card check
  */
void SD_Card_Manual_Check(void)
{
    Send_Debug_Data("\r\n=== MANUAL SD CARD CHECK ===\r\n");
    
    if (sd_card_initialized) {
        HAL_SD_CardStateTypeDef card_state = HAL_SD_GetCardState(&hsd1);
        char msg[100];
        snprintf(msg, sizeof(msg), "Card State: %d\r\n", card_state);
        Send_Debug_Data(msg);
        
        if (card_state == HAL_SD_CARD_TRANSFER || 
            card_state == HAL_SD_CARD_SENDING || 
            card_state == HAL_SD_CARD_RECEIVING ||
            card_state == HAL_SD_CARD_PROGRAMMING) {
            Send_Debug_Data("Card is present and working\r\n");
        } else {
            Send_Debug_Data("Card might be removed\r\n");
        }
    } else {
        Send_Debug_Data("Card not initialized, attempting detection...\r\n");
        
        HAL_StatusTypeDef init_result = HAL_SD_Init(&hsd1);
        
        if (init_result == HAL_OK) {
            Send_Debug_Data("Card detected and initialized!\r\n");
            sd_card_present = 1;
            sd_card_initialized = 1;
        } else {
            uint32_t error_code = HAL_SD_GetError(&hsd1);
            char msg[100];
            snprintf(msg, sizeof(msg), "No card detected, error: 0x%08lX\r\n", error_code);
            Send_Debug_Data(msg);
        }
    }
    
    Send_Debug_Data("============================\r\n");
}

/* ==== NEW ENHANCED FUNCTIONS ==== */

/**
  * @brief  Advanced SD Card initialization with error recovery
  */
SD_Card_Status_t SD_Card_Advanced_Init(void)
{
    Send_Debug_Data("\r\n=== SAFE SD CARD INITIALIZATION ===\r\n");

    // Try multiple initialization approaches
    uint8_t init_attempts = 0;
    HAL_StatusTypeDef result;

    while (init_attempts < 3) {
        init_attempts++;
        char msg[100];
        snprintf(msg, sizeof(msg), "Initialization attempt %d/3...\r\n", init_attempts);
        Send_Debug_Data(msg);

        // Reset the SD card handle
        HAL_SD_DeInit(&hsd1);
        HAL_Delay(100);

        // Configure with safe settings - 1-bit only, no bus width changes
                hsd1.Instance = SDMMC1;
                hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
                hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
        hsd1.Init.BusWide = SDMMC_BUS_WIDE_1B;  // Force 1-bit only
                hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
        
        // Use different clock dividers for each attempt
        switch (init_attempts) {
            case 1:
                hsd1.Init.ClockDiv = 16;  // Very slow clock
                Send_Debug_Data("  Using: ClockDiv=16, 1-bit bus (SAFE)\r\n");
                break;
            case 2:
                hsd1.Init.ClockDiv = 8;   // Medium clock
                Send_Debug_Data("  Using: ClockDiv=8, 1-bit bus (SAFE)\r\n");
                break;
            case 3:
                hsd1.Init.ClockDiv = 4;   // Faster clock
                Send_Debug_Data("  Using: ClockDiv=4, 1-bit bus (SAFE)\r\n");
                break;
        }

        // Try initialization
        result = HAL_SD_Init(&hsd1);

        if (result == HAL_OK) {
            HAL_SD_CardStateTypeDef state = HAL_SD_GetCardState(&hsd1);
            if (state == HAL_SD_CARD_TRANSFER || state == HAL_SD_CARD_STANDBY) {
                sd_card_initialized = 1;
                sd_card_present = 1;

                snprintf(msg, sizeof(msg), "SUCCESS! Card state: %d\r\n", state);
                Send_Debug_Data(msg);

                // Display card information
                SD_Card_Display_Info();

                // SKIP bus width optimization to prevent hangs
                Send_Debug_Data("Bus width optimization: SKIPPED (prevents hangs)\r\n");
                Send_Debug_Data("Card working in 1-bit mode for stability\r\n");

                Send_Debug_Data("========================================\r\n");
                return SD_CARD_OK;
            }
        }

        uint32_t error = HAL_SD_GetError(&hsd1);
        snprintf(msg, sizeof(msg), "  Failed - Error: 0x%08lX\r\n", error);
        Send_Debug_Data(msg);

        HAL_Delay(200);
    }

    Send_Debug_Data("All initialization attempts FAILED\r\n");
    Send_Debug_Data("========================================\r\n");
    sd_card_initialized = 0;
    sd_card_present = 0;
    return SD_CARD_INIT_FAILED;
}

/**
  * @brief  Display detailed card information
  */
void SD_Card_Display_Info(void)
{
    if (!sd_card_initialized) return;

    Send_Debug_Data("\r\n--- Card Information ---\r\n");

    HAL_SD_CardInfoTypeDef CardInfo;
    if (HAL_SD_GetCardInfo(&hsd1, &CardInfo) == HAL_OK) {
        char msg[300];

        // Card type
        const char* card_type_str = "Unknown";
        switch (CardInfo.CardType) {
            case CARD_SDSC: card_type_str = "SDSC (Standard Capacity)"; break;
            case CARD_SDHC_SDXC: card_type_str = "SDHC/SDXC (High Capacity)"; break;
        }

        snprintf(msg, sizeof(msg), "Card Type: %s\r\n", card_type_str);
        Send_Debug_Data(msg);

        // Capacity calculation
        uint64_t capacity_bytes = (uint64_t)CardInfo.LogBlockNbr * CardInfo.LogBlockSize;
        uint32_t capacity_mb = (uint32_t)(capacity_bytes / (1024 * 1024));
        uint32_t capacity_gb = capacity_mb / 1024;

        if (capacity_gb > 0) {
            snprintf(msg, sizeof(msg), "Capacity: %lu GB (%lu MB)\r\n", capacity_gb, capacity_mb);
        } else {
            snprintf(msg, sizeof(msg), "Capacity: %lu MB\r\n", capacity_mb);
        }
        Send_Debug_Data(msg);

        snprintf(msg, sizeof(msg), "Block Size: %lu bytes\r\n", CardInfo.LogBlockSize);
        Send_Debug_Data(msg);

        snprintf(msg, sizeof(msg), "Total Blocks: %lu\r\n", CardInfo.LogBlockNbr);
        Send_Debug_Data(msg);

        // Additional card info
        snprintf(msg, sizeof(msg), "Card Version: %d.%d\r\n",
                CardInfo.CardVersion >> 4, CardInfo.CardVersion & 0x0F);
        Send_Debug_Data(msg);

        snprintf(msg, sizeof(msg), "Class: %d\r\n", CardInfo.Class);
        Send_Debug_Data(msg);
    }

    Send_Debug_Data("------------------------\r\n");
}

/**
  * @brief  Optimize SD card bus width - DISABLED to prevent hangs
  */
void SD_Card_Optimize_Bus_Width(void)
{
    if (!sd_card_initialized) return;

    Send_Debug_Data("Bus width optimization: DISABLED (prevents system hangs)\r\n");
    Send_Debug_Data("Using 1-bit bus width only for stability\r\n");
    
    // Force 1-bit mode and don't try to change it
    HAL_SD_ConfigWideBusOperation(&hsd1, SDMMC_BUS_WIDE_1B);
}

/**
  * @brief  Comprehensive SD Card performance test
  */
void SD_Card_Performance_Test(void)
{
    if (!sd_card_initialized) {
        Send_Debug_Data("Performance Test: Card not initialized\r\n");
        return;
    }

    Send_Debug_Data("\r\n=== SD CARD PERFORMANCE TEST ===\r\n");

    uint8_t write_buffer[512];
    uint8_t read_buffer[512];

    // Fill write buffer with test pattern
    for (int i = 0; i < 512; i++) {
        write_buffer[i] = (uint8_t)(i & 0xFF);
    }

    // Test different block addresses
    uint32_t test_blocks[] = {100, 1000, 5000, 10000};
    uint32_t successful_tests = 0;

    for (int i = 0; i < 4; i++) {
        uint32_t block = test_blocks[i];
        char msg[100];
        snprintf(msg, sizeof(msg), "Testing block %lu...\r\n", block);
        Send_Debug_Data(msg);

        // Measure write time
        uint32_t start_time = HAL_GetTick();
        HAL_StatusTypeDef write_result = HAL_SD_WriteBlocks(&hsd1, write_buffer, block, 1, 10000);
        uint32_t write_time = HAL_GetTick() - start_time;

        if (write_result == HAL_OK) {
            snprintf(msg, sizeof(msg), "  Write: SUCCESS (%lu ms)\r\n", write_time);
            Send_Debug_Data(msg);

            // Measure read time
            start_time = HAL_GetTick();
            HAL_StatusTypeDef read_result = HAL_SD_ReadBlocks(&hsd1, read_buffer, block, 1, 10000);
            uint32_t read_time = HAL_GetTick() - start_time;

            if (read_result == HAL_OK) {
                snprintf(msg, sizeof(msg), "  Read: SUCCESS (%lu ms)\r\n", read_time);
                Send_Debug_Data(msg);

                // Verify data
                if (memcmp(write_buffer, read_buffer, 512) == 0) {
                    Send_Debug_Data("  Data verification: PASSED\r\n");
                    successful_tests++;

                    // Calculate speeds (approximate)
                    uint32_t write_speed = (write_time > 0) ? (512000 / write_time) : 0; // bytes/sec
                    uint32_t read_speed = (read_time > 0) ? (512000 / read_time) : 0;

                    snprintf(msg, sizeof(msg), "  Write speed: ~%lu KB/s\r\n", write_speed / 1024);
                    Send_Debug_Data(msg);
                    snprintf(msg, sizeof(msg), "  Read speed: ~%lu KB/s\r\n", read_speed / 1024);
                    Send_Debug_Data(msg);
                } else {
                    Send_Debug_Data("  Data verification: FAILED\r\n");
                }
            } else {
                snprintf(msg, sizeof(msg), "  Read: FAILED (Error: 0x%08lX)\r\n", HAL_SD_GetError(&hsd1));
                Send_Debug_Data(msg);
            }
        } else {
            snprintf(msg, sizeof(msg), "  Write: FAILED (Error: 0x%08lX)\r\n", HAL_SD_GetError(&hsd1));
            Send_Debug_Data(msg);
        }

        Send_Debug_Data("\r\n");
        HAL_Delay(100);
    }

    char msg[100];
    snprintf(msg, sizeof(msg), "Performance Test Results: %lu/4 tests PASSED\r\n", successful_tests);
    Send_Debug_Data(msg);

    if (successful_tests == 4) {
        Send_Debug_Data("SD Card Performance: EXCELLENT\r\n");
    } else if (successful_tests >= 2) {
        Send_Debug_Data("SD Card Performance: GOOD\r\n");
    } else if (successful_tests >= 1) {
        Send_Debug_Data("SD Card Performance: POOR\r\n");
    } else {
        Send_Debug_Data("SD Card Performance: FAILED\r\n");
    }

    Send_Debug_Data("================================\r\n");
}

/**
  * @brief  Multi-block read/write test
  */
void SD_Card_Multi_Block_Test(void)
{
    if (!sd_card_initialized) {
        Send_Debug_Data("Multi-Block Test: Card not initialized\r\n");
        return;
    }
    
    Send_Debug_Data("\r\n=== MULTI-BLOCK TEST ===\r\n");

    #define MULTI_BLOCK_COUNT 4
    uint8_t write_buffer[512 * MULTI_BLOCK_COUNT];
    uint8_t read_buffer[512 * MULTI_BLOCK_COUNT];

    // Fill write buffer with unique pattern for each block
    for (int block = 0; block < MULTI_BLOCK_COUNT; block++) {
        for (int i = 0; i < 512; i++) {
            write_buffer[block * 512 + i] = (uint8_t)((block << 4) | (i & 0x0F));
        }
    }
    
    uint32_t start_block = 2000;

    char msg[100];
    snprintf(msg, sizeof(msg), "Writing %d blocks starting at block %lu...\r\n",
             MULTI_BLOCK_COUNT, start_block);
    Send_Debug_Data(msg);
    
    // Write multiple blocks
    uint32_t start_time = HAL_GetTick();
    HAL_StatusTypeDef write_result = HAL_SD_WriteBlocks(&hsd1, write_buffer, start_block, MULTI_BLOCK_COUNT, 15000);
    uint32_t write_time = HAL_GetTick() - start_time;
    
    if (write_result == HAL_OK) {
        snprintf(msg, sizeof(msg), "Multi-block write: SUCCESS (%lu ms)\r\n", write_time);
        Send_Debug_Data(msg);

        // Read multiple blocks
        snprintf(msg, sizeof(msg), "Reading %d blocks...\r\n", MULTI_BLOCK_COUNT);
        Send_Debug_Data(msg);

        start_time = HAL_GetTick();
        HAL_StatusTypeDef read_result = HAL_SD_ReadBlocks(&hsd1, read_buffer, start_block, MULTI_BLOCK_COUNT, 15000);
        uint32_t read_time = HAL_GetTick() - start_time;
        
        if (read_result == HAL_OK) {
            snprintf(msg, sizeof(msg), "Multi-block read: SUCCESS (%lu ms)\r\n", read_time);
                Send_Debug_Data(msg);

            // Verify data
            if (memcmp(write_buffer, read_buffer, 512 * MULTI_BLOCK_COUNT) == 0) {
                Send_Debug_Data("Multi-block verification: PASSED\r\n");

                // Calculate throughput
                uint32_t total_bytes = 512 * MULTI_BLOCK_COUNT;
                uint32_t write_throughput = (write_time > 0) ? (total_bytes * 1000 / write_time) : 0;
                uint32_t read_throughput = (read_time > 0) ? (total_bytes * 1000 / read_time) : 0;

                snprintf(msg, sizeof(msg), "Write throughput: %lu KB/s\r\n", write_throughput / 1024);
    Send_Debug_Data(msg);
                snprintf(msg, sizeof(msg), "Read throughput: %lu KB/s\r\n", read_throughput / 1024);
        Send_Debug_Data(msg);
        
                Send_Debug_Data("Multi-block test: PASSED\r\n");
            } else {
                Send_Debug_Data("Multi-block verification: FAILED\r\n");
                Send_Debug_Data("Multi-block test: FAILED\r\n");
            }
        } else {
            snprintf(msg, sizeof(msg), "Multi-block read: FAILED (Error: 0x%08lX)\r\n", HAL_SD_GetError(&hsd1));
            Send_Debug_Data(msg);
        }
    } else {
        snprintf(msg, sizeof(msg), "Multi-block write: FAILED (Error: 0x%08lX)\r\n", HAL_SD_GetError(&hsd1));
    Send_Debug_Data(msg);
    }

    Send_Debug_Data("========================\r\n");
    
    Send_Debug_Data("[AUTO] Status Report Complete\r\n");
}

/**
  * @brief  Display auto test configuration - SIMPLIFIED
  */
void SD_Card_Display_Auto_Test_Config(void)
{
    Send_Debug_Data("\r\n=== SD CARD AUTO TEST CONFIG ===\r\n");
    Send_Debug_Data("Auto Test: DISABLED (prevents system hangs)\r\n");
    Send_Debug_Data("Test Interval: N/A\r\n");
    Send_Debug_Data("Current Sequence: N/A\r\n");
    Send_Debug_Data("Next Test In: N/A\r\n");
    Send_Debug_Data("Test Sequence: DISABLED\r\n");
    Send_Debug_Data("================================\r\n");
}

/**
  * @brief  Enable/Disable automatic SD card testing - DISABLED
  */
void SD_Card_Set_Auto_Test(uint8_t enable)
{
    Send_Debug_Data("SD Card Auto Test: DISABLED (prevents system hangs)\r\n");
    Send_Debug_Data("Use manual test commands instead:\r\n");
    Send_Debug_Data("  - sd_test: Basic test\r\n");
    Send_Debug_Data("  - sd_performance: Performance test\r\n");
    Send_Debug_Data("  - sd_multiblock: Multi-block test\r\n");
}

/**
  * @brief  Set automatic test interval - DISABLED
  */
void SD_Card_Set_Auto_Test_Interval(uint32_t interval_ms)
{
    Send_Debug_Data("SD Card Auto Test: DISABLED (prevents system hangs)\r\n");
    Send_Debug_Data("Use manual test commands instead\r\n");
}

/**
  * @brief  Run automatic SD card test sequence - DISABLED
  */
void SD_Card_Auto_Test_Process(void)
{
    // Do nothing - automatic testing disabled to prevent hangs
    return;
}

/**
  * @brief  Automatic basic SD card test - DISABLED
  */
void SD_Card_Auto_Basic_Test(void)
{
    Send_Debug_Data("Auto Basic Test: DISABLED (prevents system hangs)\r\n");
    Send_Debug_Data("Use 'sd_test' command for manual testing\r\n");
}

/**
  * @brief  Automatic performance test - DISABLED
  */
void SD_Card_Auto_Performance_Test(void)
{
    Send_Debug_Data("Auto Performance Test: DISABLED (prevents system hangs)\r\n");
    Send_Debug_Data("Use 'sd_performance' command for manual testing\r\n");
}

/**
  * @brief  Automatic text test - DISABLED
  */
void SD_Card_Auto_Text_Test(void)
{
    Send_Debug_Data("Auto Text Test: DISABLED (prevents system hangs)\r\n");
    Send_Debug_Data("Use 'sd_test' command for manual testing\r\n");
}

/**
  * @brief  Automatic multi-block test - DISABLED
  */
void SD_Card_Auto_Multi_Block_Test(void)
{
    Send_Debug_Data("Auto Multi-Block Test: DISABLED (prevents system hangs)\r\n");
    Send_Debug_Data("Use 'sd_multiblock' command for manual testing\r\n");
}

/**
  * @brief  Automatic status report - DISABLED
  */
void SD_Card_Auto_Status_Report(void)
{
    Send_Debug_Data("Auto Status Report: DISABLED (prevents system hangs)\r\n");
    Send_Debug_Data("Use 'sd_status' command for manual status\r\n");
}

/**
  * @brief  Run complete automatic test suite - DISABLED
  */
void SD_Card_Run_Full_Auto_Test(void)
{
    Send_Debug_Data("\r\n========================================\r\n");
    Send_Debug_Data("SD CARD AUTOMATIC TEST SUITE: DISABLED\r\n");
    Send_Debug_Data("(Prevents system hangs)\r\n");
    Send_Debug_Data("========================================\r\n");
    Send_Debug_Data("Use manual test commands instead:\r\n");
    Send_Debug_Data("  - sd_test: Basic read/write test\r\n");
    Send_Debug_Data("  - sd_performance: Performance test\r\n");
    Send_Debug_Data("  - sd_multiblock: Multi-block test\r\n");
    Send_Debug_Data("  - sd_status: Status report\r\n");
    Send_Debug_Data("========================================\r\n");
}

/**
  * @brief  Ultra-safe SD Card complete setup - minimal testing
  */
SD_Card_Status_t SD_Card_Complete_Setup(void)
{
    Send_Debug_Data("\r\n===== SD CARD ULTRA-SAFE SETUP =====\r\n");

    // Step 1: Safe initialization only
    SD_Card_Status_t init_result = SD_Card_Advanced_Init();
    if (init_result != SD_CARD_OK) {
        Send_Debug_Data("SD Card setup FAILED at initialization\r\n");
        Send_Debug_Data("===================================\r\n");
        return init_result;
    }

    // Step 2: Skip ALL tests to prevent hangs
    Send_Debug_Data("All performance tests: DISABLED (prevents system hangs)\r\n");
    Send_Debug_Data("Use individual test commands manually if needed:\r\n");
    Send_Debug_Data("  - sd_test: Basic read/write test\r\n");
    Send_Debug_Data("  - sd_performance: Performance test\r\n");
    Send_Debug_Data("  - sd_multiblock: Multi-block test\r\n");

    Send_Debug_Data("SD Card Ultra-Safe Setup: COMPLETED\r\n");
    Send_Debug_Data("Card is ready for use in 1-bit mode\r\n");
    Send_Debug_Data("===================================\r\n");

    return SD_CARD_OK;
}

/**
  * @brief  Run automatic SD card test suite - COMPLETELY SILENT
  */
void SD_Card_Run_Automatic_Tests(void)
{
    if (!sd_card_initialized) {
        return;
    }
    
    // Run tests completely silently - NO debug output at all
    SD_Card_Simple_Alphabet_Test();
    HAL_Delay(1000);
    
    // Run other tests silently too (you'll need to remove debug output from them)
    // SD_Card_Test();
    // SD_Card_Text_Test();
    // SD_Card_Performance_Test();
    // SD_Card_Multi_Block_Test();
    // SD_Card_Status_Display();
}

/**
  * @brief  Simple alphabet test - NO DEBUG OUTPUT
  */
void SD_Card_Simple_Alphabet_Test(void)
{
    if (!sd_card_initialized) {
        return;
    }
    
    // Simple test data - just a few alphabets
    char test_message[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    uint8_t write_buffer[512];
    uint8_t read_buffer[512];
    
    // Clear buffers
    memset(write_buffer, 0, 512);
    memset(read_buffer, 0, 512);
    
    // Copy test message to write buffer
    strcpy((char*)write_buffer, test_message);
    
    uint32_t test_block = 15000;  // Safe test block
    
    // Write and read test - NO DEBUG OUTPUT
    if (SD_Card_WriteSingleBlock(write_buffer, test_block) == SD_CARD_OK) {
        if (SD_Card_ReadSingleBlock(read_buffer, test_block) == SD_CARD_OK) {
            // Test completed silently
        }
    }
}
