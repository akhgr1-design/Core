/* gpio_manager.c
 * GPIO Management Implementation for STM32H7B0VB Chiller Control System
 */

#include "gpio_manager.h"
#include <stdarg.h>

// External debug function
extern void Send_Debug_Data(const char *message);

// Wrapper function for formatted debug output
void Send_Debug_Formatted(const char *format, ...) {
    char buffer[200];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Send_Debug_Data(buffer);
}

/* Static Configuration Arrays - Local to this file only */
static RelayConfig_t relay_config[RELAY_COUNT] = {
    // Q0 Bank (8 relays)
    {"Q0.0", GPIOE, GPIO_PIN_5, 1, 0},   // PE5
    {"Q0.1", GPIOB, GPIO_PIN_8, 1, 0},   // PB8
    {"Q0.2", GPIOE, GPIO_PIN_2, 1, 0},   // PE2
    {"Q0.3", GPIOB, GPIO_PIN_9, 1, 0},   // PB9
    {"Q0.4", GPIOE, GPIO_PIN_3, 1, 0},   // PE3
    {"Q0.5", GPIOE, GPIO_PIN_4, 1, 0},   // PE4
    {"Q0.6", GPIOE, GPIO_PIN_6, 1, 0},   // PE6
    {"Q0.7", GPIOA, GPIO_PIN_10, 1, 0},  // PA10

    // Q1 Bank (8 relays)
    {"Q1.0", GPIOH, GPIO_PIN_0, 1, 0},   // PH0
    {"Q1.1", GPIOH, GPIO_PIN_1, 1, 0},   // PH1
    {"Q1.2", GPIOC, GPIO_PIN_0, 1, 0},   // PC0
    {"Q1.3", GPIOC, GPIO_PIN_1, 1, 0},   // PC1
    {"Q1.4", GPIOB, GPIO_PIN_6, 1, 0},   // PB6
    {"Q1.5", GPIOA, GPIO_PIN_11, 1, 0},  // PA11
    {"Q1.6", GPIOD, GPIO_PIN_14, 1, 0},  // PD14
    {"Q1.7", GPIOD, GPIO_PIN_15, 1, 0}   // PD15
};

static InputConfig_t input_config[INPUT_COUNT] = {
    // I0 Bank (8 inputs)
    {"I0.0", GPIOA, GPIO_PIN_0, 0, 1, 1},   // PA0, active_low, default_high
    {"I0.1", GPIOA, GPIO_PIN_1, 0, 1, 1},   // PA1
    {"I0.2", GPIOC, GPIO_PIN_6, 0, 1, 1},   // PC6
    {"I0.3", GPIOC, GPIO_PIN_7, 0, 1, 1},   // PC7
    {"I0.4", GPIOC, GPIO_PIN_4, 0, 1, 1},   // PC4
    {"I0.5", GPIOC, GPIO_PIN_5, 0, 1, 1},   // PC5
    {"I0.6", GPIOA, GPIO_PIN_8, 0, 1, 1},   // PA8
    {"I0.7", GPIOA, GPIO_PIN_9, 0, 1, 1},   // PA9

    // I1 Bank (8 inputs)
    {"I1.0", GPIOD, GPIO_PIN_12, 0, 1, 1},  // PD12
    {"I1.1", GPIOD, GPIO_PIN_13, 0, 1, 1},  // PD13
    {"I1.2", GPIOE, GPIO_PIN_15, 0, 1, 1},  // PE15
    {"I1.3", GPIOD, GPIO_PIN_3, 0, 1, 1},   // PD3
    {"I1.4", GPIOB, GPIO_PIN_11, 0, 1, 1},  // PB11
    {"I1.5", GPIOD, GPIO_PIN_10, 0, 1, 1},  // PD10
    {"I1.6", GPIOC, GPIO_PIN_2, 0, 1, 1},   // PC2
    {"I1.7", GPIOC, GPIO_PIN_3, 0, 1, 1}    // PC3
};

// Add these static variables at the top of the file (after includes)
static uint32_t test_start_time = 0;
static uint8_t test_state = 0;  // 0=idle, 1=turning_on, 2=all_on, 3=turning_off
static uint8_t current_relay = 0;
static uint32_t last_action_time = 0;

/**
 * @brief Initialize GPIO Manager - Main initialization function
 */
void GPIO_Manager_Init(void)
{
    Send_Debug_Formatted("\n=== GPIO MANAGER INITIALIZATION ===\r\n");

    // NOTE: GPIO clocks should already be enabled by MX_GPIO_Init()
    // We'll just ensure they're enabled (safe to call multiple times)
    __HAL_RCC_GPIOA_CLK_ENABLE();  // PA0,PA1,PA8,PA9,PA10,PA11
    __HAL_RCC_GPIOB_CLK_ENABLE();  // PB6,PB8,PB9,PB11
    __HAL_RCC_GPIOC_CLK_ENABLE();  // PC0,PC1,PC2,PC3,PC4,PC5,PC6,PC7
    __HAL_RCC_GPIOD_CLK_ENABLE();  // PD3,PD10,PD12,PD13,PD14,PD15
    __HAL_RCC_GPIOE_CLK_ENABLE();  // PE2,PE3,PE4,PE5,PE6,PE15
    __HAL_RCC_GPIOH_CLK_ENABLE();  // PH0,PH1

    // Initialize relays and inputs
    // NOTE: This will reconfigure pins that MX_GPIO_Init() already set up
    GPIO_InitAllRelays();
    GPIO_InitAllInputs();

    // Read current state of all relays (in case some were already set)
    for (int i = 0; i < RELAY_COUNT; i++) {
        GPIO_PinState current_pin_state = HAL_GPIO_ReadPin(relay_config[i].gpio_port, relay_config[i].gpio_pin);
        relay_config[i].current_state = (current_pin_state == GPIO_PIN_SET) ? 1 : 0;

        // Debug: Show current relay states
        if (relay_config[i].current_state) {
            Send_Debug_Formatted("Found %s already ON\r\n", relay_config[i].relay_name);
        }
    }

    Send_Debug_Formatted("GPIO Manager initialized successfully\r\n");
    Send_Debug_Formatted("- 16 Relay outputs configured (ULN2803 active HIGH)\r\n");
    Send_Debug_Formatted("- 16 Digital inputs configured (Optically isolated active LOW)\r\n");
    Send_Debug_Formatted("=====================================\r\n\r\n");
}

/**
 * @brief Initialize all relay output pins
 */
void GPIO_InitAllRelays(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Configure relay output pins
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    for (int i = 0; i < RELAY_COUNT; i++) {
        GPIO_InitStruct.Pin = relay_config[i].gpio_pin;
        HAL_GPIO_Init(relay_config[i].gpio_port, &GPIO_InitStruct);

        // DON'T automatically turn relays OFF - preserve existing state
        // Just read the current state instead
        GPIO_PinState current_state = HAL_GPIO_ReadPin(relay_config[i].gpio_port, relay_config[i].gpio_pin);
        relay_config[i].current_state = (current_state == GPIO_PIN_SET) ? 1 : 0;

        Send_Debug_Formatted("Relay %s (%s%d): %s\r\n",
               relay_config[i].relay_name,
               (relay_config[i].gpio_port == GPIOA) ? "PA" :
               (relay_config[i].gpio_port == GPIOB) ? "PB" :
               (relay_config[i].gpio_port == GPIOC) ? "PC" :
               (relay_config[i].gpio_port == GPIOD) ? "PD" :
               (relay_config[i].gpio_port == GPIOE) ? "PE" :
               (relay_config[i].gpio_port == GPIOH) ? "PH" : "??",
               __builtin_ctz(relay_config[i].gpio_pin),
               relay_config[i].current_state ? "ON" : "OFF");
    }

    Send_Debug_Formatted("All 16 relay outputs initialized (states preserved)\r\n");
}

/**
 * @brief Initialize all digital input pins
 */
void GPIO_InitAllInputs(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Configure input pins with pull-ups (critical for optical isolation)
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;  // CRITICAL for optical isolation
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    for (int i = 0; i < INPUT_COUNT; i++) {
        GPIO_InitStruct.Pin = input_config[i].gpio_pin;
        HAL_GPIO_Init(input_config[i].gpio_port, &GPIO_InitStruct);

        // Initialize current and previous states
        input_config[i].current_state = Input_Read(i);
        input_config[i].previous_state = input_config[i].current_state;
    }

    Send_Debug_Formatted("All 16 digital inputs initialized with pull-ups enabled\r\n");
}

/**
 * @brief Set relay state (silent version)
 * @param relay_id: Relay ID (0-15)
 * @param state: 0=OFF, 1=ON
 */
void Relay_Set(uint8_t relay_id, uint8_t state)
{
    if (relay_id >= RELAY_COUNT) {
        return; // Silent error
    }

    RelayConfig_t* relay = &relay_config[relay_id];
    
    // Set GPIO pin state
    if (state) {
        HAL_GPIO_WritePin(relay->gpio_port, relay->gpio_pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(relay->gpio_port, relay->gpio_pin, GPIO_PIN_RESET);
    }
    
    // Update current state
    relay->current_state = state;
    
    // Completely silent - no debug output
}

/**
 * @brief Get relay state
 * @param relay_id: Relay number (0-15)
 * @return: Current relay state (0=OFF, 1=ON)
 */
uint8_t Relay_Get(uint8_t relay_id)
{
    if (relay_id >= RELAY_COUNT) return 0;
    return relay_config[relay_id].current_state;
}

/**
 * @brief Toggle relay state
 * @param relay_id: Relay number (0-15)
 */
void Relay_Toggle(uint8_t relay_id)
{
    if (relay_id >= RELAY_COUNT) return;

    uint8_t current_state = Relay_Get(relay_id);
    Relay_Set(relay_id, !current_state);
}

/**
 * @brief Turn off all relays
 */
void Relay_AllOff(void)
{
    for (int i = 0; i < RELAY_COUNT; i++) {
        Relay_Set(i, 0);
    }
    Send_Debug_Formatted("All relays turned OFF\r\n");
}

/**
 * @brief Read digital input state
 * @param input_id: Input number (0-15)
 * @return: Logic state (0=Active/24V applied, 1=Inactive/No signal)
 */
uint8_t Input_Read(uint8_t input_id)
{
    if (input_id >= INPUT_COUNT) return 0xFF; // Error

    InputConfig_t* input = &input_config[input_id];

    // Read GPIO pin
    GPIO_PinState pin_state = HAL_GPIO_ReadPin(input->gpio_port, input->gpio_pin);

    // Return logic state (remember: active LOW for optical isolation)
    return (pin_state == GPIO_PIN_RESET) ? 0 : 1;  // LOW=0 (Active), HIGH=1 (Inactive)
}

/**
 * @brief Read input with simple debouncing
 * @param input_id: Input number (0-15)
 * @return: Debounced logic state
 */
uint8_t Input_ReadDebounced(uint8_t input_id)
{
    uint8_t state1 = Input_Read(input_id);
    HAL_Delay(5); // 5ms debounce delay
    uint8_t state2 = Input_Read(input_id);

    return (state1 == state2) ? state1 : input_config[input_id].current_state;
}

/**
 * @brief Read all inputs as bitmask
 * @return: 16-bit value with each bit representing an input
 */
uint16_t Input_ReadAll(void)
{
    uint16_t result = 0;

    for (int i = 0; i < INPUT_COUNT; i++) {
        if (Input_Read(i) == 0) { // Active (24V applied)
            result |= (1 << i);
        }
    }

    return result;
}

/**
 * @brief Sequential test of all relays
 */
void Test_AllRelays_Sequential(void)
{
    Send_Debug_Formatted("\n=== RELAY TEST SEQUENCE STARTING ===\r\n");
    Send_Debug_Formatted("Total Relays: %d (Q0.0-Q0.7, Q1.0-Q1.7)\r\n", RELAY_COUNT);
    Send_Debug_Formatted("Test Duration: 10 seconds per relay\r\n");
    Send_Debug_Formatted("Status LEDs: Should illuminate with relay activation\r\n");
    Send_Debug_Formatted("ULN2803 Driver: Active HIGH logic confirmed\r\n");
    Send_Debug_Formatted("No Load Connected - Visual/LED monitoring only\r\n");
    Send_Debug_Formatted("==========================================\r\n\r\n");

    // Test each relay sequentially
    for (int i = 0; i < RELAY_COUNT; i++) {
        Test_SingleRelay(i);
        HAL_Delay(500); // 500ms pause between relays
    }

    Send_Debug_Formatted("=== RELAY TEST SEQUENCE COMPLETED ===\r\n");
    Send_Debug_Formatted("All %d relays tested successfully\r\n", RELAY_COUNT);
    Send_Debug_Formatted("Q0.0-Q0.7: Bank 0 relays (8 total)\r\n");
    Send_Debug_Formatted("Q1.0-Q1.7: Bank 1 relays (8 total)\r\n");
    Send_Debug_Formatted("Check: All LEDs should be OFF now\r\n");
    Send_Debug_Formatted("Ready for next test sequence\r\n");
    Send_Debug_Formatted("=========================================\r\n\r\n");
}

/**
 * @brief Test single relay with timing and feedback
 * @param relay_id: Relay to test (0-15)
 */
void Test_SingleRelay(uint8_t relay_id)
{
    if (relay_id >= RELAY_COUNT) return;

    RelayConfig_t* relay = &relay_config[relay_id];

    // Get current time for logging
    uint32_t current_time = HAL_GetTick();
    uint32_t hours = (current_time / 3600000) % 24;
    uint32_t minutes = (current_time / 60000) % 60;
    uint32_t seconds = (current_time / 1000) % 60;

    // Turn relay ON
    Send_Debug_Formatted("[%02lu:%02lu:%02lu] Testing %s (%s) - RELAY ON  - LED should be illuminated\r\n",
           hours, minutes, seconds, relay->relay_name,
           (relay->gpio_port == GPIOA) ? "PA" :
           (relay->gpio_port == GPIOB) ? "PB" :
           (relay->gpio_port == GPIOC) ? "PC" :
           (relay->gpio_port == GPIOD) ? "PD" :
           (relay->gpio_port == GPIOE) ? "PE" :
           (relay->gpio_port == GPIOH) ? "PH" : "??");

    Relay_Set(relay_id, 1);

    // Wait 10 seconds with countdown
    for (int i = 10; i > 0; i--) {
        current_time = HAL_GetTick();
        hours = (current_time / 3600000) % 24;
        minutes = (current_time / 60000) % 60;
        seconds = (current_time / 1000) % 60;

        Send_Debug_Formatted("[%02lu:%02lu:%02lu] %s ON - %d seconds remaining...\r\n",
               hours, minutes, seconds, relay->relay_name, i);
        HAL_Delay(1000);
    }

    // Turn relay OFF
    current_time = HAL_GetTick();
    hours = (current_time / 3600000) % 24;
    minutes = (current_time / 60000) % 60;
    seconds = (current_time / 1000) % 60;

    Send_Debug_Formatted("[%02lu:%02lu:%02lu] Testing %s - RELAY OFF - LED should be extinguished\r\n",
           hours, minutes, seconds, relay->relay_name);
    Relay_Set(relay_id, 0);

    Send_Debug_Formatted("[%02lu:%02lu:%02lu] Moving to next relay...\r\n", hours, minutes, seconds);
    Send_Debug_Formatted("---\r\n\r\n");
}

/**
 * @brief Monitor all inputs for changes (change detection only)
 */
void Test_AllInputs_ChangeDetection(void)
{
    Send_Debug_Formatted("\n=== DIGITAL INPUT CHANGE MONITORING ===\r\n");
    Send_Debug_Formatted("Optically Isolated Inputs: 24V = ACTIVE (LOW)\r\n");
    Send_Debug_Formatted("Monitoring I0.0-I0.7, I1.0-I1.7 for changes only\r\n");
    Send_Debug_Formatted("Apply/Remove 24V signals to test...\r\n");
    Send_Debug_Formatted("Press CTRL+C or reset to stop monitoring\r\n");
    Send_Debug_Formatted("==========================================\r\n");

    // Initialize all inputs to current state (no initial display)
    for (int i = 0; i < INPUT_COUNT; i++) {
        input_config[i].previous_state = Input_Read(i);
    }

    Send_Debug_Formatted("Ready - Monitoring for input changes...\r\n\r\n");

    // Continuous monitoring loop - only report changes
    while (1) {
        for (int i = 0; i < INPUT_COUNT; i++) {
            uint8_t current_state = Input_Read(i);

            // Only act on state changes
            if (current_state != input_config[i].previous_state) {

                // Get timestamp
                uint32_t current_time = HAL_GetTick();
                uint32_t hours = (current_time / 3600000) % 24;
                uint32_t minutes = (current_time / 60000) % 60;
                uint32_t seconds = (current_time / 1000) % 60;

                // Report only the new state (cleaner output)
                if (current_state == 0) {
                    // Input activated (24V applied)
                    Send_Debug_Formatted("[%02lu:%02lu:%02lu] %s: ON\r\n",
                           hours, minutes, seconds, input_config[i].input_name);
                } else {
                    // Input deactivated (24V removed)
                    Send_Debug_Formatted("[%02lu:%02lu:%02lu] %s: OFF\r\n",
                           hours, minutes, seconds, input_config[i].input_name);
                }

                // Update previous state
                input_config[i].previous_state = current_state;
            }
        }

        HAL_Delay(50); // 50ms scan rate
    }
}

/**
 * @brief Display current status of all inputs (one-shot)
 */
void Test_AllInputs_Status(void)
{
    Send_Debug_Formatted("\n=== DIGITAL INPUT STATUS CHECK ===\r\n");
    Send_Debug_Formatted("Reading all %d inputs once...\r\n\r\n", INPUT_COUNT);

    uint32_t current_time = HAL_GetTick();
    uint32_t hours = (current_time / 3600000) % 24;
    uint32_t minutes = (current_time / 60000) % 60;
    uint32_t seconds = (current_time / 1000) % 60;

    Send_Debug_Formatted("[%02lu:%02lu:%02lu] Input Status:\r\n", hours, minutes, seconds);
    Send_Debug_Formatted("Input  | Pin | Pin State | Logic State | Status\r\n");
    Send_Debug_Formatted("-------|-----|-----------|-------------|----------\r\n");

    for (int i = 0; i < INPUT_COUNT; i++) {
        uint8_t logic_state = Input_Read(i);
        GPIO_PinState pin_state = HAL_GPIO_ReadPin(input_config[i].gpio_port,
                                                   input_config[i].gpio_pin);

        Send_Debug_Formatted("%-6s | %-3s | %-9s | %-11s | %s\r\n",
               input_config[i].input_name,
               (input_config[i].gpio_port == GPIOA) ? "PA" :
               (input_config[i].gpio_port == GPIOB) ? "PB" :
               (input_config[i].gpio_port == GPIOC) ? "PC" :
               (input_config[i].gpio_port == GPIOD) ? "PD" :
               (input_config[i].gpio_port == GPIOE) ? "PE" : "??",
               (pin_state == GPIO_PIN_SET) ? "HIGH" : "LOW",
               (logic_state == 1) ? "INACTIVE" : "ACTIVE",
               (logic_state == 0) ? "24V APPLIED" : "NO SIGNAL");
    }

    Send_Debug_Formatted("\r\nTest completed.\r\n");
    Send_Debug_Formatted("Expected: HIGH = No 24V signal, LOW = 24V signal applied\r\n");
    Send_Debug_Formatted("======================================\r\n\r\n");
}

/**
 * @brief Print current relay status
 */
void GPIO_PrintRelayStatus(void)
{
    Send_Debug_Formatted("\n=== RELAY STATUS ===\r\n");
    for (int i = 0; i < RELAY_COUNT; i++) {
        Send_Debug_Formatted("%s: %s\r\n", relay_config[i].relay_name,
               relay_config[i].current_state ? "ON" : "OFF");
    }
    Send_Debug_Formatted("===================\r\n\r\n");
}

/**
 * @brief Print current input status
 */
void GPIO_PrintInputStatus(void)
{
    Send_Debug_Formatted("\n=== INPUT STATUS ===\r\n");
    uint8_t any_active = 0;
    Send_Debug_Formatted("Active Inputs: ");

    for (int i = 0; i < INPUT_COUNT; i++) {
        uint8_t state = Input_Read(i);
        if (state == 0) { // Active (24V applied)
            if (any_active) Send_Debug_Formatted(", ");
            Send_Debug_Formatted("%s", input_config[i].input_name);
            any_active = 1;
        }
    }

    if (!any_active) {
        Send_Debug_Formatted("None (All inputs OFF)");
    }

    Send_Debug_Formatted("\r\n==================\r\n\r\n");
}

/**
 * @brief Debug command interface
 */
void GPIO_DebugCommands(void)
{
    Send_Debug_Formatted("\n=== GPIO DEBUG COMMANDS ===\r\n");
    Send_Debug_Formatted("Available commands:\r\n");
    Send_Debug_Formatted("- relay_test        : Test all relays sequentially\r\n");
    Send_Debug_Formatted("- relay_status      : Show current relay status\r\n");
    Send_Debug_Formatted("- relay_all_off     : Turn off all relays\r\n");
    Send_Debug_Formatted("- input_monitor     : Start input change monitoring\r\n");
    Send_Debug_Formatted("- input_status      : Show current input status\r\n");
    Send_Debug_Formatted("- gpio_help         : Show this help\r\n");
    Send_Debug_Formatted("===========================\r\n\r\n");
}

/**
 * @brief Test all outputs sequentially with 10-second delays
 */
void Test_All_Outputs_Sequential(void) {
    Send_Debug_Formatted("=== STARTING SEQUENTIAL OUTPUT TEST ===\r\n");
    Send_Debug_Formatted("Turning ON all outputs one by one (10s delay each)\r\n");
    
    // Turn ON all outputs sequentially
    for (int i = 0; i < 16; i++) {
        Relay_Set(i, 1);  // Turn ON relay i
        
        char msg[50];
        snprintf(msg, sizeof(msg), "Output Q%d.%d: ON\r\n", i/8, i%8);
        Send_Debug_Formatted(msg);
        
        // Wait 10 seconds with input monitoring
        for (int delay = 0; delay < 100; delay++) {
            HAL_Delay(100);  // 100ms * 100 = 10 seconds
            Monitor_Input_Changes_Once();  // Check inputs during delay
        }
    }
    
    Send_Debug_Formatted("All outputs are now ON. Waiting 5 seconds...\r\n");
    HAL_Delay(5000);
    
    Send_Debug_Formatted("Turning OFF all outputs one by one (10s delay each)\r\n");
    
    // Turn OFF all outputs sequentially
    for (int i = 0; i < 16; i++) {
        Relay_Set(i, 0);  // Turn OFF relay i
        
        char msg[50];
        snprintf(msg, sizeof(msg), "Output Q%d.%d: OFF\r\n", i/8, i%8);
        Send_Debug_Formatted(msg);
        
        // Wait 10 seconds with input monitoring
        for (int delay = 0; delay < 100; delay++) {
            HAL_Delay(100);  // 100ms * 100 = 10 seconds
            Monitor_Input_Changes_Once();  // Check inputs during delay
        }
    }
    
    Send_Debug_Formatted("=== SEQUENTIAL OUTPUT TEST COMPLETE ===\r\n");
}

/**
 * @brief Simple test - turn on all outputs with double delays, repeating
 */
void Test_All_Outputs_Simple(void) {
    Send_Debug_Data("=== SIMPLE OUTPUT TEST (REPEATING) ===\r\n");
    Send_Debug_Data("Starting test at: ");
    
    // Show current time
    uint32_t current_time = HAL_GetTick();
    uint32_t hours = (current_time / 3600000) % 24;
    uint32_t minutes = (current_time / 60000) % 60;
    uint32_t seconds = (current_time / 1000) % 60;
    char time_msg[50];
    snprintf(time_msg, sizeof(time_msg), "%02lu:%02lu:%02lu\r\n", hours, minutes, seconds);
    Send_Debug_Data(time_msg);
    
    Send_Debug_Data("Turning ON all outputs one by one (4s delay each)...\r\n");
    
    // Turn ON all outputs with 4 second delay between each
    for (int i = 0; i < 16; i++) {
        Send_Debug_Data("Turning ON relay ");
        char relay_msg[20];
        snprintf(relay_msg, sizeof(relay_msg), "Q%d.%d\r\n", i/8, i%8);
        Send_Debug_Data(relay_msg);
        
        Relay_Set(i, 1);
        
        Send_Debug_Data("Waiting 4 seconds...\r\n");
        HAL_Delay(4000);  // 4 second delay between each relay
    }
    
    Send_Debug_Data("All outputs ON. Waiting 20 seconds...\r\n");
    HAL_Delay(20000);  // Wait 20 seconds with all outputs on
    
    Send_Debug_Data("Turning OFF all outputs one by one (4s delay each)...\r\n");
    
    // Turn OFF all outputs with 4 second delay between each
    for (int i = 0; i < 16; i++) {
        Send_Debug_Data("Turning OFF relay ");
        char relay_msg[20];
        snprintf(relay_msg, sizeof(relay_msg), "Q%d.%d\r\n", i/8, i%8);
        Send_Debug_Data(relay_msg);
        
        Relay_Set(i, 0);
        
        Send_Debug_Data("Waiting 4 seconds...\r\n");
        HAL_Delay(4000);  // 4 second delay between each relay
    }
    
    Send_Debug_Data("All outputs OFF. Waiting 10 seconds before next cycle...\r\n");
    HAL_Delay(10000);  // Wait 10 seconds before repeating
    
    Send_Debug_Data("=== CYCLE COMPLETE - WILL REPEAT ===\r\n");
}

/**
 * @brief Monitor inputs once and display changes only
 */
void Monitor_Input_Changes_Once(void) {
    static uint16_t last_input_state = 0xFFFF;
    static uint8_t first_run = 1;
    
    uint16_t current_input_state = 0;
    
    // Read all inputs
    for (int i = 0; i < 16; i++) {
        if (Input_Read(i) == 0) {  // Active = LOW
            current_input_state |= (1 << i);
        }
    }
    
    // On first run, just store the initial state without displaying
    if (first_run) {
        last_input_state = current_input_state;
        first_run = 0;
        return; // Don't display anything on first run
    }
    
    // Check for changes and only display when individual pin status changes
    if (current_input_state != last_input_state) {
        uint16_t changed = current_input_state ^ last_input_state;
        for (int i = 0; i < 16; i++) {
            if (changed & (1 << i)) {
                char input_msg[60];
                snprintf(input_msg, sizeof(input_msg), 
                        "INPUT CHANGE: I%d.%d = %s\r\n", 
                        i/8, i%8, 
                        (current_input_state & (1 << i)) ? "ACTIVE" : "INACTIVE");
                Send_Debug_Data(input_msg);
            }
        }
        last_input_state = current_input_state;
    }
}

/**
 * @brief Monitor inputs continuously in main loop
 */
void Monitor_Input_Changes_Continuous(void) {
    static uint16_t last_input_state = 0xFFFF;
    static uint32_t last_check_time = 0;
    static uint8_t first_run = 1;
    
    // Check inputs every 100ms
    if (HAL_GetTick() - last_check_time > 100) {
        last_check_time = HAL_GetTick();
        
        uint16_t current_input_state = 0;
        
        // Read all inputs
        for (int i = 0; i < 16; i++) {
            if (Input_Read(i) == 0) {  // Active = LOW
                current_input_state |= (1 << i);
            }
        }
        
        // On first run, just store the initial state without displaying
        if (first_run) {
            last_input_state = current_input_state;
            first_run = 0;
            return; // Don't display anything on first run
        }
        
        // Check for changes and only display when individual pin status changes
        if (current_input_state != last_input_state) {
            uint16_t changed = current_input_state ^ last_input_state;
            for (int i = 0; i < 16; i++) {
                if (changed & (1 << i)) {
                    char input_msg[60];
                    snprintf(input_msg, sizeof(input_msg), 
                            "INPUT CHANGE: I%d.%d = %s\r\n", 
                            i/8, i%8, 
                            (current_input_state & (1 << i)) ? "ACTIVE" : "INACTIVE");
                    Send_Debug_Data(input_msg);
                }
            }
            last_input_state = current_input_state;
        }
    }
}

/**
 * @brief Display current GPIO status (minimal)
 */
void Display_GPIO_Status(void) {
    // Only show active inputs, not all status
    uint8_t active_inputs = 0;
    for (int i = 0; i < 16; i++) {
        if (Input_Read(i) == 0) {  // Active = LOW
            active_inputs++;
        }
    }
    
    if (active_inputs > 0) {
        Send_Debug_Data("Active Inputs: ");
        for (int i = 0; i < 16; i++) {
            if (Input_Read(i) == 0) {  // Active = LOW
                if (active_inputs > 1) Send_Debug_Data(", ");
                char msg[10];
                snprintf(msg, sizeof(msg), "I%d.%d", i/8, i%8);
                Send_Debug_Data(msg);
                active_inputs--;
            }
        }
        Send_Debug_Data("\r\n");
    }
}

/**
 * @brief Non-blocking output test - runs in main loop (completely silent)
 */
void Test_All_Outputs_NonBlocking(void) {
    uint32_t current_time = HAL_GetTick();
    
    switch(test_state) {
        case 0: // Idle - start test
            test_start_time = current_time;
            last_action_time = current_time;
            current_relay = 0;
            test_state = 1;
            break;
            
        case 1: // Turning on relays one by one
            if (current_time - last_action_time >= 4000) { // 4 second delay
                if (current_relay < 16) {
                    Relay_Set(current_relay, 1);
                    // Completely silent - no debug output
                    current_relay++;
                    last_action_time = current_time;
                } else {
                    // All relays on, wait 20 seconds
                    test_state = 2;
                    last_action_time = current_time;
                }
            }
            break;
            
        case 2: // All relays on, waiting
            if (current_time - last_action_time >= 20000) { // 20 seconds
                current_relay = 0;
                test_state = 3;
                last_action_time = current_time;
            }
            break;
            
        case 3: // Turning off relays one by one
            if (current_time - last_action_time >= 4000) { // 4 second delay
                if (current_relay < 16) {
                    Relay_Set(current_relay, 0);
                    // Completely silent - no debug output
                    current_relay++;
                    last_action_time = current_time;
                } else {
                    // All relays off, test complete
                    test_state = 0; // Reset to idle
                }
            }
            break;
    }
}

/**
 * @brief Check if test is running
 */
uint8_t Test_Is_Running(void) {
    return (test_state != 0);
}
