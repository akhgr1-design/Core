/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body with Modbus RTU Integration
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"
#include "uart_comm.h"
#include "w5500_driver.h"
#include "spi_w5500.h"
#include "w5500_socket.h"
#include "w5500_tcp_server.h"
#include "modbus_sensor.h"          // Keep this include
#include "gpio_manager.h"           // Add GPIO Manager
#include <string.h>
#include <stdio.h>
#include "flash_25q16.h"
#include "hmi.h"
#include "sd_card.h"
#include "equipment_config.h"

/* Private define ------------------------------------------------------------*/
#define RUN_LED_PORT        RUN_LED_GPIO_Port
#define RUN_LED_PIN         RUN_LED_Pin
#define ERR_LED_PORT        ERR_LED_GPIO_Port
#define ERR_LED_PIN         ERR_LED_Pin
#define STOP_LED_PORT       STOP_LED_GPIO_Port
#define STOP_LED_PIN        STOP_LED_Pin
#define RS485_DE_PORT       U485_DE_RE_GPIO_Port
#define RS485_DE_PIN        U485_DE_RE_Pin

// Relay pins (these will be handled by GPIO Manager now)
#define Q0_6_PORT           GPIOE
#define Q0_6_PIN            GPIO_PIN_6
#define Q0_7_PORT           GPIOA
#define Q0_7_PIN            GPIO_PIN_10

/* Private variables ---------------------------------------------------------*/
uint32_t message_count = 0;
uint8_t w5500_initialized = 0;
uint8_t modbus_initialized = 0; // Add this line
uint8_t gpio_manager_initialized = 0; // Add GPIO Manager status
uint8_t flash_initialized = 0;
uint8_t hmi_initialized = 0;
// sd_card_initialized and sd_card_present are now defined in sd_card.c

// Simulated sensor data (will be replaced by Modbus data)
uint16_t sensor_temp = 254;
uint16_t sensor_press = 120;
uint16_t sensor_flow = 18;
uint16_t sensor_level = 75;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
void Process_DebugCommands(char* command);
void Display_SystemStatus(void);

// Add SD Card function prototypes
SD_Card_Status_t SD_Card_Complete_Setup(void);
void SD_Card_Quick_Health_Check(void);
void SD_Card_Performance_Test(void);
void SD_Card_Multi_Block_Test(void);

// Add these new function prototypes at the end of the file, before #ifdef __cplusplus
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

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/
  MPU_Config();
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_SPI4_Init();
  MX_SPI2_Init();  // Add this line for flash memory
  MX_UART4_Init();
  MX_UART8_Init();
  MX_UART7_Init();  // Add this line - will be 115200 baud

  /* Initialize peripherals */
  Init_UARTs();
  HAL_Delay(100);
  Send_Debug_Data("=== SYSTEM STARTUP ===\r\n");
  Send_Debug_Data("STM32H7B0VB Chiller Control System\r\n");
  Send_Debug_Data("GPIO Manager + Network + Modbus Integration\r\n");

  // Turn on RUN_LED, turn off STOP_LED
  HAL_GPIO_WritePin(RUN_LED_PORT, RUN_LED_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(STOP_LED_PORT, STOP_LED_PIN, GPIO_PIN_RESET);

  /* === GPIO MANAGER INITIALIZATION === */
  Send_Debug_Data("=== Initializing GPIO Manager ===\r\n");
  GPIO_Manager_Init();
  gpio_manager_initialized = 1;
  Send_Debug_Data("GPIO Manager: 16 Relays + 16 Inputs ready\r\n");

  // Show available GPIO commands
  Send_Debug_Data("Available GPIO Commands:\r\n");
  Send_Debug_Data("- relay_test : Test all relays\r\n");
  Send_Debug_Data("- input_monitor : Monitor inputs\r\n");
  Send_Debug_Data("- gpio_help : Show all commands\r\n");

   // Initialize SPI for W5500
  SPI_W5500_Init();

  // Test SPI
  SPI_W5500_Test_Communication();

  // Test W5500
  W5500_Test_CS();
  W5500_Test_Reset();

  // Run self-test
  if (W5500_SelfTest()) {
      w5500_initialized = 1;
      Send_Debug_Data("W5500 SelfTest: PASSED!\r\n");
  } else {
      w5500_initialized = 0;
      HAL_GPIO_WritePin(RUN_LED_PORT, RUN_LED_PIN, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(STOP_LED_PORT, STOP_LED_PIN, GPIO_PIN_SET);
      Send_Debug_Data("W5500 SelfTest: FAILED!\r\n");
  }

 /* === EQUIPMENT CONFIGURATION INITIALIZATION === */
Send_Debug_Data("=== Initializing Equipment Configuration ===\r\n");
if (EquipmentConfig_Init() == EQUIPMENT_STATUS_OK) {
    Send_Debug_Data("Equipment Configuration: READY\r\n");
} else {
    Send_Debug_Data("Equipment Configuration: FAILED\r\n");
}
 
  // === MODBUS SENSOR INITIALIZATION ===
  Modbus_System_Init();  // Initialize everything
  Modbus_System_SetInterval(60000);  // Set to 60 seconds instead of 10
  modbus_initialized = 1;  // Enable Modbus in main loop

  // === ADD MODBUS INIT AND FIRST READ AFTER NETWORK IS UP ===
  if (w5500_initialized && W5500_Init()) {
      Send_Debug_Data("=== W5500 Network Initialized ===\r\n");

      uint8_t mac[6] = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56};
      uint8_t ip[4] = {192, 168, 8, 100};
      uint8_t subnet[4] = {255, 255, 255, 0};
      uint8_t gateway[4] = {192, 168, 8, 1};

      W5500_ConfigureNetwork(mac, ip, subnet, gateway);

      uint8_t read_ip[4];
      W5500_GetIPConfig(read_ip, NULL, NULL, NULL);
      char msg[100];
      snprintf(msg, sizeof(msg), "IP Configured: %d.%d.%d.%d\r\n",
               read_ip[0], read_ip[1], read_ip[2], read_ip[3]);
      Send_Debug_Data(msg);

      // === MODBUS FIRST READ AFTER NETWORK IS UP ===
      Modbus_System_Start();  // Single line to start first read

      // Turn ON Relays Q0.6 and Q0.7 using GPIO Manager - DISABLED
      // if (gpio_manager_initialized) {
      //     Relay_Set(6, 1);  // Q0.6 (relay index 6)
      //     Relay_Set(7, 1);  // Q0.7 (relay index 7)
      //     Send_Debug_Data("Relays Q0.6 and Q0.7: ON (via GPIO Manager)\r\n");
      // } else {
      //     // Fallback to direct GPIO control
      //     HAL_GPIO_WritePin(Q0_6_PORT, Q0_6_PIN, GPIO_PIN_SET);
      //     HAL_GPIO_WritePin(Q0_7_PORT, Q0_7_PIN, GPIO_PIN_SET);
      //     Send_Debug_Data("Relays Q0.6 and Q0.7: ON (direct GPIO)\r\n");
      // }
      
      Send_Debug_Data("All relays initialized to OFF state\r\n");
  } else {
      Send_Debug_Data("W5500: Initialization FAILED!\r\n");
  }

  /* === FLASH MEMORY INITIALIZATION === */
  if (Flash_Init() == 0) {
      flash_initialized = 1;
  } else {
      flash_initialized = 0;
      Send_Debug_Data("Flash Memory: INITIALIZATION FAILED\r\n");
  }

  /* === HMI INITIALIZATION === */
  Send_Debug_Data("=== Initializing HMI Communication ===\r\n");
  if (HMI_Init() == 1) {
      Send_Debug_Data("HMI: Initialization successful\r\n");
      HMI_Set_Initialized(1);
  } else {
      Send_Debug_Data("HMI: Initialization failed\r\n");
      HMI_Set_Initialized(0);
  }

  /* === SD CARD INITIALIZATION - CLEAN === */
  // Use only basic initialization - no debug spam
  if (SD_Card_Init() == SD_CARD_OK) {
      sd_card_initialized = 1;
      
      // Run automatic tests silently (no debug output)
      SD_Card_Run_Automatic_Tests();
      
  } else {
      sd_card_initialized = 0;
  }

  Send_Debug_Data("=== System Initialization Complete ===\r\n");
  Send_Debug_Data("All systems ready - entering main loop\r\n");

  /* Infinite loop */
  while (1)
  {
    /* --- Task 1: W5500 Network Maintenance --- */
    if (w5500_initialized) {
        // This static variable belongs to the W5500 task
        static uint32_t link_check_time = 0;
        
        // Check link status periodically
        if (HAL_GetTick() - link_check_time > 2000) {
            link_check_time = HAL_GetTick();
            // All the original link up/down logic goes here
            uint8_t link_is_up = W5500_CheckLinkStatus();
            if (link_is_up) {
                // Link up logic (your original code)
                static uint8_t link_up_logged = 0;
                if (!link_up_logged) {
                    Send_Debug_Data("Network Link: UP\r\n");
                    link_up_logged = 1;
                }
            } else {
                // Link down logic (your original code)
                static uint8_t link_down_logged = 0;
                if (!link_down_logged) {
                    Send_Debug_Data("Network Link: DOWN\r\n");
                    link_down_logged = 1;
                }
            }
        }
        // You would also put TCP_Server_Process() here if you were using it
    }

    /* --- Task 2: Modbus Sensor Processing --- */
    if (modbus_initialized) {
        Modbus_System_Process();
        // HAL_Delay(1000);  // REMOVE THIS - it's blocking the main loop
    }
/* --- Task 6: Equipment Configuration Processing --- */
EquipmentConfig_ProcessPeriodicTasks();
    /* --- Task 3: GPIO Manager Processing --- */
    if (gpio_manager_initialized) {
        // Monitor input changes continuously
        Monitor_Input_Changes_Continuous();
        
        // Non-blocking output test - runs every 30 seconds (completely silent)
        static uint32_t auto_output_test_time = 0;
        static uint8_t test_started = 0;
        
        // Start test every 30 seconds (silent)
        if (!Test_Is_Running() && (HAL_GetTick() - auto_output_test_time > 30000)) {
            auto_output_test_time = HAL_GetTick();
            test_started = 1;
            // Completely silent - no debug messages
        }
        
        // Run the non-blocking test
        if (test_started) {
            Test_All_Outputs_NonBlocking();
            if (!Test_Is_Running()) {
                test_started = 0;
                // Completely silent - no debug messages
            }
        }
        
        // Periodic GPIO status display every 60 seconds (only if inputs active)
        static uint32_t gpio_status_time = 0;
        if (HAL_GetTick() - gpio_status_time > 60000) {
            gpio_status_time = HAL_GetTick();
            Display_GPIO_Status();
        }
    }

    /* --- Task 4: Flash Memory Test --- */
    if (flash_initialized) {
        // Flash memory tested during boot - no periodic operations
        // Flash is ready for use when needed
    }

    /* --- Task 4: HMI Processing --- */
    if (HMI_Is_Initialized()) {
        HMI_Process();
    }

    /* --- Task 5: UI / Debug Output --- */
    // LED blinking task (keep this)
    static uint32_t last_blink_time = 0;
    if (HAL_GetTick() - last_blink_time > 500) {
        last_blink_time = HAL_GetTick();
        HAL_GPIO_TogglePin(RUN_LED_GPIO_Port, RUN_LED_PIN);
    }

    /* --- Task 7: SD Card Processing - SAFE MODE --- */
    SD_Card_Process();              // Keep existing function
    // SD_Card_Auto_Test_Process();    // REMOVED - causes hangs

    message_count++;
    // Removed other debug output
  }
}

/* USER CODE BEGIN 4 */

/**
 * @brief Process debug console commands
 * @note Integrate with your existing command system
 */
void Process_DebugCommands(char* command)
{
    if (strncmp(command, "relay_test", 10) == 0) {
        Send_Debug_Data("Starting relay test sequence...\r\n");
        Test_AllRelays_Sequential();
    }
    else if (strncmp(command, "output_test", 11) == 0) {
        Send_Debug_Data("Starting sequential output test...\r\n");
        Test_All_Outputs_Sequential();
    }
    else if (strncmp(command, "gpio_status", 11) == 0) {
        Display_GPIO_Status();
    }
    else if (strncmp(command, "relay_status", 12) == 0) {
        GPIO_PrintRelayStatus();
    }
    else if (strncmp(command, "relay_all_off", 13) == 0) {
        Relay_AllOff();
        Send_Debug_Data("All relays turned OFF\r\n");
    }
    else if (strncmp(command, "input_monitor", 13) == 0) {
        Send_Debug_Data("Starting input monitoring (infinite loop)...\r\n");
        Send_Debug_Data("Press reset to stop monitoring\r\n");
        Test_AllInputs_ChangeDetection();
    }
    else if (strncmp(command, "input_status", 12) == 0) {
        Test_AllInputs_Status();
    }
    else if (strncmp(command, "gpio_help", 9) == 0) {
        GPIO_DebugCommands();
    }
    else if (strncmp(command, "system_status", 13) == 0) {
        Display_SystemStatus();
    }
    else if (strncmp(command, "modbus_status", 13) == 0) {
        Modbus_Debug_Status();
    }
    else if (strncmp(command, "modbus_enable", 13) == 0) {
        Modbus_System_Enable();
    }
    else if (strncmp(command, "modbus_disable", 14) == 0) {
        Modbus_System_Disable();
    }
else if (strncmp(command, "hmi", 3) == 0) {
        HMI_Process_Debug_Command(command);
    }
    else if (strncmp(command, "sd_text", 7) == 0) {
        SD_Card_Text_Test();
    }
    else if (strncmp(command, "sd_test", 7) == 0) {
        SD_Card_Test();
    }
    else if (strncmp(command, "sd_capacity", 11) == 0) {
        SD_Card_Capacity_Test();
    }
    else if (strncmp(command, "sd_status", 9) == 0) {
        SD_Card_Status_Display();
    }
    else if (strncmp(command, "sd_check", 8) == 0) {
        SD_Card_Manual_Check();
    }
    else if (strncmp(command, "sd_advanced", 11) == 0) {
        SD_Card_Complete_Setup();
    }
else if (strncmp(command, "sd_performance", 14) == 0) {
        SD_Card_Performance_Test();
    }
    else if (strncmp(command, "sd_multiblock", 13) == 0) {
        SD_Card_Multi_Block_Test();
    }
    else if (strncmp(command, "sd_auto_on", 10) == 0) {
        SD_Card_Set_Auto_Test(1);
    }
    else if (strncmp(command, "sd_auto_off", 11) == 0) {
        SD_Card_Set_Auto_Test(0);
    }
    else if (strncmp(command, "sd_auto_config", 14) == 0) {
        SD_Card_Display_Auto_Test_Config();
    }
    else if (strncmp(command, "sd_auto_full", 12) == 0) {
        SD_Card_Run_Full_Auto_Test();
    }
else if (strncmp(command, "sd_safe_init", 12) == 0) {
    Send_Debug_Data("Running ultra-safe SD card initialization...\r\n");
    SD_Card_Complete_Setup();
}
else if (strncmp(command, "sd_recovery", 11) == 0) {
    SD_Card_Emergency_Recovery();
}
else if (strncmp(command, "sd_reset", 8) == 0) {
    Send_Debug_Data("Resetting SD card to 1-bit mode...\r\n");
    HAL_SD_DeInit(&hsd1);
    HAL_Delay(100);
    // sd_card_initialized = 0; // This line is removed as per the edit hint
    // sd_card_present = 0; // This line is removed as per the edit hint
    Send_Debug_Data("SD card reset complete\r\n");
}
else if (strncmp(command, "sd_detect", 9) == 0) {
    SD_Card_Manual_Detection();
}
else if (strncmp(command, "modbus_60s", 10) == 0) {
    Modbus_System_SetInterval(60000);
    Send_Debug_Data("Modbus interval set to 60 seconds\r\n");
}
    else {
        char response[100];
        snprintf(response, sizeof(response), "Unknown command: %s\r\n", command);
        Send_Debug_Data(response);
        Send_Debug_Data("Available commands:\r\n");
        Send_Debug_Data("- relay_test, output_test, gpio_status\r\n");
        Send_Debug_Data("- hmi_version, hmi_status, system_status\r\n");
        Send_Debug_Data("- sd_test, sd_capacity, sd_status\r\n");
        Send_Debug_Data("- sd_advanced : Complete SD setup\r\n");
        Send_Debug_Data("- sd_performance : Performance test\r\n");
        Send_Debug_Data("- sd_multiblock : Multi-block test\r\n");
    }
}
else if (strncmp(command, "config_show", 11) == 0) {
    EquipmentConfig_DisplayStatus();
}
else if (strncmp(command, "config_defaults", 15) == 0) {
    if (EquipmentConfig_LoadDefaults() == EQUIPMENT_STATUS_OK) {
        Send_Debug_Data("38°C optimized defaults loaded\r\n");
        EquipmentConfig_DisplayStatus();
    } else {
        Send_Debug_Data("Failed to load defaults\r\n");
    }
}
else if (strncmp(command, "config_mode_eco", 15) == 0) {
    if (EquipmentConfig_SetCapacityMode(CAPACITY_MODE_ECONOMIC) == EQUIPMENT_STATUS_OK) {
        Send_Debug_Data("Switched to ECONOMIC mode (2 compressors max)\r\n");
    }
}
else if (strncmp(command, "config_mode_normal", 18) == 0) {
    if (EquipmentConfig_SetCapacityMode(CAPACITY_MODE_NORMAL) == EQUIPMENT_STATUS_OK) {
        Send_Debug_Data("Switched to NORMAL mode (4 compressors max)\r\n");
    }
}
else if (strncmp(command, "config_mode_full", 16) == 0) {
    if (EquipmentConfig_SetCapacityMode(CAPACITY_MODE_FULL) == EQUIPMENT_STATUS_OK) {
        Send_Debug_Data("Switched to FULL mode (6 compressors max)\r\n");
    }
}
else if (strncmp(command, "config_save", 11) == 0) {
    if (EquipmentConfig_SaveToFlash() == EQUIPMENT_STATUS_OK) {
        Send_Debug_Data("Configuration saved to flash memory\r\n");
    } else {
        Send_Debug_Data("Flash save failed\r\n");
    }
}
/**
 * @brief Display comprehensive system status
 */
void Display_SystemStatus(void)
{
    char msg[200];

    Send_Debug_Data("\r\n=== SYSTEM STATUS ===\r\n");

    snprintf(msg, sizeof(msg), "Uptime: %lu seconds\r\n", HAL_GetTick() / 1000);
    Send_Debug_Data(msg);

    snprintf(msg, sizeof(msg), "MCU: STM32H7B0VB @ %luMHz\r\n", HAL_RCC_GetSysClockFreq() / 1000000);
    Send_Debug_Data(msg);

    Send_Debug_Data("Flash: 128KB, RAM: ~1.4MB\r\n");

    // System component status
    snprintf(msg, sizeof(msg), "W5500 Network: %s\r\n", w5500_initialized ? "INITIALIZED" : "FAILED");
    Send_Debug_Data(msg);

    snprintf(msg, sizeof(msg), "Modbus System: %s\r\n", modbus_initialized ? "RUNNING" : "STOPPED");
    Send_Debug_Data(msg);

    snprintf(msg, sizeof(msg), "GPIO Manager: %s\r\n", gpio_manager_initialized ? "ACTIVE" : "INACTIVE");
    Send_Debug_Data(msg);

    // Show active relays
    Send_Debug_Data("Active Relays: ");
    uint8_t active_relays = 0;
    for (int i = 0; i < 16; i++) {
        if (Relay_Get(i)) {
            if (active_relays > 0) Send_Debug_Data(", ");
            snprintf(msg, sizeof(msg), "Q%d.%d", i/8, i%8);
            Send_Debug_Data(msg);
            active_relays++;
        }
    }
    if (active_relays == 0) Send_Debug_Data("None");
    Send_Debug_Data("\r\n");

    // Show active inputs
    Send_Debug_Data("Active Inputs: ");
    uint8_t active_inputs = 0;
    for (int i = 0; i < 16; i++) {
        if (Input_Read(i) == 0) { // Active = LOW
            if (active_inputs > 0) Send_Debug_Data(", ");
            snprintf(msg, sizeof(msg), "I%d.%d", i/8, i%8);
            Send_Debug_Data(msg);
            active_inputs++;
        }
    }
    if (active_inputs == 0) Send_Debug_Data("None");
    Send_Debug_Data("\r\n");

    snprintf(msg, sizeof(msg), "HMI System: %s\r\n", HMI_Is_Initialized() ? "ACTIVE" : "INACTIVE");
    Send_Debug_Data(msg);

    snprintf(msg, sizeof(msg), "SD Card: %s\r\n", SD_Card_Is_Initialized() ? "ACTIVE" : "INACTIVE"); // Changed to SD_Card_Is_Initialized()
    Send_Debug_Data(msg);

    Send_Debug_Data("=====================\r\n\r\n");
}

/* USER CODE END 4 */

/**
  * @brief System Clock Configuration
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = 64;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 35;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  Configure MPU
  */
static void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  HAL_MPU_Disable();
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x00;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/**
  * @brief  This function is executed in case of error occurrence.
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
    HAL_GPIO_TogglePin(ERR_LED_PORT, ERR_LED_PIN);
    HAL_Delay(200);
  }
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports assert error
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  char msg[100];
  snprintf(msg, sizeof(msg), "Assert: %s @ %lu\r\n", file, line);
  Send_Debug_Data(msg);
}
#endif /* USE_FULL_ASSERT */

