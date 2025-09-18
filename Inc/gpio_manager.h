/* gpio_manager.h
 * GPIO Management for STM32H7B0VB Chiller Control System
 * Handles 16 Relay Outputs and 16 Digital Inputs
 */

#ifndef GPIO_MANAGER_H
#define GPIO_MANAGER_H

#include "main.h"
#include <stdio.h>
#include <string.h>

/* Configuration Constants */
#define RELAY_COUNT         16
#define INPUT_COUNT         16
#define TEST_RELAY_ON_TIME  10000  // 10 seconds in milliseconds

/* Relay Configuration Structure */
typedef struct {
    char relay_name[8];          // "Q0.0", "Q0.1", etc.
    GPIO_TypeDef* gpio_port;     // GPIO port (GPIOA, GPIOB, etc.)
    uint16_t gpio_pin;           // GPIO pin (GPIO_PIN_0, etc.)
    uint8_t active_level;        // 1 = active HIGH (for ULN2803)
    uint8_t current_state;       // Current relay state (0=OFF, 1=ON)
} RelayConfig_t;

/* Input Configuration Structure */
typedef struct {
    char input_name[8];          // "I0.0", "I0.1", etc.
    GPIO_TypeDef* gpio_port;     // GPIO port
    uint16_t gpio_pin;           // GPIO pin
    uint8_t active_level;        // 0 = active LOW (for optical isolation)
    uint8_t current_state;       // Current input state
    uint8_t previous_state;      // Previous state for change detection
} InputConfig_t;

/* Function Prototypes */

/* Initialization Functions */
void GPIO_Manager_Init(void);
void GPIO_InitAllRelays(void);
void GPIO_InitAllInputs(void);

/* Relay Control Functions */
void Relay_Set(uint8_t relay_id, uint8_t state);
uint8_t Relay_Get(uint8_t relay_id);
void Relay_Toggle(uint8_t relay_id);
void Relay_AllOff(void);

/* Input Reading Functions */
uint8_t Input_Read(uint8_t input_id);
uint8_t Input_ReadDebounced(uint8_t input_id);
uint16_t Input_ReadAll(void);

/* Test Functions */
void Test_AllRelays_Sequential(void);
void Test_SingleRelay(uint8_t relay_id);
void Test_AllInputs_ChangeDetection(void);
void Test_AllInputs_Status(void);

// Test and monitoring functions
void Test_All_Outputs_Sequential(void);
void Monitor_Input_Changes_Once(void);
void Monitor_Input_Changes_Continuous(void);
void Display_GPIO_Status(void);

/* Utility Functions */
void GPIO_PrintRelayStatus(void);
void GPIO_PrintInputStatus(void);
const char* GPIO_GetTimeString(void);

/* Debug Functions */
void GPIO_DebugCommands(void);

// Simple test function
void Test_All_Outputs_Simple(void);

// Non-blocking test functions
void Test_All_Outputs_NonBlocking(void);
uint8_t Test_Is_Running(void);

#endif /* GPIO_MANAGER_H */
