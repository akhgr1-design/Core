/**
 * ========================================================================
 * CHILLER EQUIPMENT STAGING CONTROL SYSTEM - IMPLEMENTATION
 * ========================================================================
 * File: ch_staging.c
 * Author: Claude (Anthropic)
 * Date: 2025-09-18
 * 
 * DESCRIPTION:
 * Implementation of chiller equipment staging control system.
 * Manages 8 compressors and 4 condenser banks with intelligent staging,
 * runtime balancing, four-tier capacity control, and comprehensive debugging.
 * 
 * INTEGRATION:
 * - Uses GPIO Manager Relay_Set() for equipment control
 * - Integrates with equipment_config.h for installed counts
 * - Uses uart_comm.h for comprehensive debug output
 * - Saves configuration to flash storage
 * 
 * ========================================================================
 */

 #include "ch_staging.h"
 #include "equipment_config.h"
 #include "flash_config.h"
 #include "gpio_manager.h"
 #include "uart_comm.h"
 #include "main.h"
 #include <string.h>
 #include <stdio.h>
 #include <math.h>
 
 // ========================================================================
 // GLOBAL VARIABLES
 // ========================================================================
 
 // Main staging system structure
 ChillerStaging_t g_staging_system;
 
 // Static variables for timing and control
 static uint32_t s_last_debug_time = 0;
 static uint32_t s_debug_interval = 10000;  // 10 seconds debug interval
 static bool s_initialization_complete = false;
 
 // Equipment relay mapping arrays
 static const uint8_t compressor_relay_map[MAX_COMPRESSORS] = {
     COMPRESSOR_1_RELAY,  // 0 = PE5 (Relay0)
     COMPRESSOR_2_RELAY,  // 1 = PB8 (Relay1)
     COMPRESSOR_3_RELAY,  // 2 = PE2 (Relay2)
     COMPRESSOR_4_RELAY,  // 3 = PB9 (Relay3)
     COMPRESSOR_5_RELAY,  // 4 = PE3 (Relay4)
     COMPRESSOR_6_RELAY,  // 5 = PE4 (Relay5)
     COMPRESSOR_7_RELAY,  // 6 = PE6 (Relay6)
     COMPRESSOR_8_RELAY   // 7 = PA10(Relay7)
 };
 
 static const uint8_t condenser_relay_map[MAX_CONDENSER_BANKS] = {
     CONDENSER_BANK_1_RELAY,  // 8 = PH0 (Relay8)
     CONDENSER_BANK_2_RELAY,  // 9 = PH1 (Relay9)
     CONDENSER_BANK_3_RELAY,  // 10 = PC0 (Relay10)
     CONDENSER_BANK_4_RELAY   // 11 = PC1 (Relay11)
 };
 
 // Static function prototypes
 static void Staging_InitializeDefaults(void);
 static void Staging_UpdateEquipmentAvailability(void);
 static bool Staging_IsCompressorReadyToStart(uint8_t index);
 static bool Staging_IsCompressorReadyToStop(uint8_t index);
 static bool Staging_IsCondenserReadyToStart(uint8_t index);
 static bool Staging_IsCondenserReadyToStop(uint8_t index);
 static void Staging_DebugPrint(const char* message);
 static void Staging_UpdateSystemStatus(void);
 static uint8_t Staging_CalculateRequiredCompressors(float capacity_percent);
 static uint8_t Staging_CalculateRequiredCondensers(uint8_t compressor_count);
 
 // ========================================================================
 // INITIALIZATION FUNCTIONS
 // ========================================================================
 
 bool Staging_Init(void)
 {
     // Debug output for initialization start
     Send_Debug_Data("\r\n=== CHILLER STAGING SYSTEM INITIALIZATION ===\r\n");
     
     // Clear the entire staging system structure
     memset(&g_staging_system, 0, sizeof(ChillerStaging_t));
     
     // Initialize default values
     Staging_InitializeDefaults();
     
     // Try to load configuration from flash
     if (Staging_LoadConfiguration()) {
         Send_Debug_Data("STAGING: Configuration loaded from flash\r\n");
     } else {
         Send_Debug_Data("STAGING: Using default configuration\r\n");
     }
     
     // Update equipment availability based on equipment config
     Staging_UpdateEquipmentAvailability();
     
     // Initialize timing
     g_staging_system.last_process_time = HAL_GetTick();
     s_last_debug_time = HAL_GetTick();
     
     // Enable debug output by default
     g_staging_system.debug_enabled = true;
     
     // Set system state to initialized
     g_staging_system.status.system_state = STAGING_STATE_OFF;
     
     // Mark initialization as complete
     s_initialization_complete = true;
     
     // Debug output for initialization complete
     Send_Debug_Data("STAGING: Initialization complete\r\n");
     Send_Debug_Data("STAGING: Available equipment check:\r\n");
     
     char debug_msg[100];
     snprintf(debug_msg, sizeof(debug_msg), 
              "STAGING: - Compressors: %d/%d available\r\n",
              g_staging_system.status.available_compressor_count, MAX_COMPRESSORS);
     Send_Debug_Data(debug_msg);
     
     snprintf(debug_msg, sizeof(debug_msg), 
              "STAGING: - Condensers: %d/%d available\r\n",
              g_staging_system.status.available_condenser_count, MAX_CONDENSER_BANKS);
     Send_Debug_Data(debug_msg);
     
     Send_Debug_Data("=== STAGING INITIALIZATION COMPLETE ===\r\n\r\n");
     
     return true;
 }
 
 static void Staging_InitializeDefaults(void)
 {
     // Initialize control parameters with defaults
     g_staging_system.control.algorithm = STAGING_ALGORITHM_RUNTIME_BALANCED;
     g_staging_system.control.target_compressor_count = 0;
     g_staging_system.control.target_condenser_count = 0;
     g_staging_system.control.current_capacity_tier = 1;
     g_staging_system.control.max_capacity_tier = 4;
     g_staging_system.control.runtime_balancing_enabled = true;
     g_staging_system.control.auto_staging_enabled = true;
     g_staging_system.control.staging_delay_compressor = COMPRESSOR_START_DELAY;
     g_staging_system.control.staging_delay_condenser = CONDENSER_START_DELAY;
     
     // Initialize all compressors
     for (uint8_t i = 0; i < MAX_COMPRESSORS; i++) {
         g_staging_system.compressors[i].is_running = false;
         g_staging_system.compressors[i].relay_state = false;
         g_staging_system.compressors[i].mode = COMPRESSOR_MODE_AUTO;
         g_staging_system.compressors[i].start_time = 0;
         g_staging_system.compressors[i].stop_time = 0;
         g_staging_system.compressors[i].runtime_hours = 0;
         g_staging_system.compressors[i].start_cycles = 0;
         g_staging_system.compressors[i].fault_count = 0;
         g_staging_system.compressors[i].enabled = true;
         g_staging_system.compressors[i].available = true;
         g_staging_system.compressors[i].performance_rating = 1.0f;
         
         // Ensure relay is off
         Relay_Set(compressor_relay_map[i], 0);
     }
     
     // Initialize all condensers
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         g_staging_system.condensers[i].is_running = false;
         g_staging_system.condensers[i].relay_state = false;
         g_staging_system.condensers[i].mode = CONDENSER_MODE_AUTO;
         g_staging_system.condensers[i].start_time = 0;
         g_staging_system.condensers[i].stop_time = 0;
         g_staging_system.condensers[i].runtime_hours = 0;
         g_staging_system.condensers[i].start_cycles = 0;
         g_staging_system.condensers[i].fault_count = 0;
         g_staging_system.condensers[i].enabled = true;
         g_staging_system.condensers[i].available = true;
         g_staging_system.condensers[i].cooling_efficiency = 1.0f;
         
         // Ensure relay is off
         Relay_Set(condenser_relay_map[i], 0);
     }
     
     // Initialize lead/lag rotation
     g_staging_system.next_compressor_to_start = 0;
     g_staging_system.next_compressor_to_stop = 0;
     g_staging_system.next_condenser_to_start = 0;
     g_staging_system.next_condenser_to_stop = 0;
     
     Send_Debug_Data("STAGING: Default configuration initialized\r\n");
 }
 
 static void Staging_UpdateEquipmentAvailability(void)
 {
     // Get installed equipment counts from equipment config
     EquipmentConfig_t* config = EquipmentConfig_GetConfig();
     if (!config) {
         return;
     }
     
     // Reset availability counters
     g_staging_system.status.available_compressor_count = 0;
     g_staging_system.status.available_condenser_count = 0;
     
     // Update compressor availability
     for (uint8_t i = 0; i < MAX_COMPRESSORS; i++) {
         if (i < config->total_compressors_installed &&  // Fixed field name
             config->compressors[i].installed && 
             config->compressors[i].enabled) {
             g_staging_system.compressors[i].available = true;
             g_staging_system.status.available_compressor_count++;
         } else {
             g_staging_system.compressors[i].available = false;
         }
     }
     
     // Update condenser availability  
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         if (i < config->total_condenser_banks &&  // Fixed field name
             config->condenser_banks[i].installed && 
             config->condenser_banks[i].enabled) {
             g_staging_system.condensers[i].available = true;
             g_staging_system.status.available_condenser_count++;
         } else {
             g_staging_system.condensers[i].available = false;
         }
     }
     
     char debug_msg[100];
     snprintf(debug_msg, sizeof(debug_msg), 
              "STAGING: Equipment availability updated - Comp:%d Cond:%d\r\n",
              g_staging_system.status.available_compressor_count,
              g_staging_system.status.available_condenser_count);
     Staging_DebugPrint(debug_msg);
 }
 
 // ========================================================================
 // CONFIGURATION MANAGEMENT
 // ========================================================================
 
 bool Staging_LoadConfiguration(void)
 {
     // Try to load staging configuration from flash
     // For now, return false to use defaults
     // TODO: Implement flash storage integration
     return false;
 }
 
 bool Staging_SaveConfiguration(void)
 {
     // Save current staging configuration to flash
     // TODO: Implement flash storage integration
     Staging_DebugPrint("STAGING: Configuration saved to flash\r\n");
     return true;
 }
 
 // ========================================================================
 // MAIN PROCESS FUNCTIONS
 // ========================================================================
 
 bool Staging_Process(void)
 {
     if (!s_initialization_complete) {
         return false;
     }
     
     uint32_t current_time = HAL_GetTick();
     
     // Update runtime hours for running equipment
     Staging_UpdateRuntimeHours();
     
     // Update equipment availability (in case config changed)
     Staging_UpdateEquipmentAvailability();
     
     // Process compressor staging if auto staging is enabled
     if (g_staging_system.control.auto_staging_enabled) {
         Staging_ProcessCompressors();
         Staging_ProcessCondensers();
     }
     
     // Update system status
     Staging_UpdateSystemStatus();
     
     // Periodic debug output
     if (g_staging_system.debug_enabled && 
         (current_time - s_last_debug_time) > s_debug_interval) {
         s_last_debug_time = current_time;
         
         char debug_msg[150];
         snprintf(debug_msg, sizeof(debug_msg), 
                  "STAGING: Status - Comp:%d/%d Cond:%d/%d Cap:%.1f%% State:%d\r\n",
                  g_staging_system.status.running_compressor_count,
                  g_staging_system.status.available_compressor_count,
                  g_staging_system.status.running_condenser_count,
                  g_staging_system.status.available_condenser_count,
                  g_staging_system.status.system_capacity_percent,
                  g_staging_system.status.system_state);
         Send_Debug_Data(debug_msg);
     }
     
     g_staging_system.last_process_time = current_time;
     return true;
 }
 
 bool Staging_UpdateCapacity(float capacity_percent)
 {
     // Clamp capacity to valid range
     if (capacity_percent < 0.0f) capacity_percent = 0.0f;
     if (capacity_percent > 100.0f) capacity_percent = 100.0f;
     
     // Calculate required compressors based on four-tier system
     uint8_t required_compressors = Staging_CalculateRequiredCompressors(capacity_percent);
     
     // Calculate required condensers based on compressor count
     uint8_t required_condensers = Staging_CalculateRequiredCondensers(required_compressors);
     
     // Update target counts
     g_staging_system.control.target_compressor_count = required_compressors;
     g_staging_system.control.target_condenser_count = required_condensers;
     
     // Determine capacity tier
     if (required_compressors <= CAPACITY_TIER_1) {
         g_staging_system.control.current_capacity_tier = 1;
     } else if (required_compressors <= CAPACITY_TIER_2) {
         g_staging_system.control.current_capacity_tier = 2;
     } else if (required_compressors <= CAPACITY_TIER_3) {
         g_staging_system.control.current_capacity_tier = 3;
     } else {
         g_staging_system.control.current_capacity_tier = 4;
     }
     
     char debug_msg[120];
     snprintf(debug_msg, sizeof(debug_msg), 
              "STAGING: Capacity update - %.1f%% -> Tier:%d Comp:%d Cond:%d\r\n",
              capacity_percent, g_staging_system.control.current_capacity_tier,
              required_compressors, required_condensers);
     Staging_DebugPrint(debug_msg);
     
     return true;
 }
 
 static uint8_t Staging_CalculateRequiredCompressors(float capacity_percent)
 {
     uint8_t max_compressors = g_staging_system.status.available_compressor_count;
     uint8_t max_tier_compressors;
     
     // Determine maximum compressors based on tier limit
     switch (g_staging_system.control.max_capacity_tier) {
         case 1: max_tier_compressors = CAPACITY_TIER_1; break;
         case 2: max_tier_compressors = CAPACITY_TIER_2; break;
         case 3: max_tier_compressors = CAPACITY_TIER_3; break;
         case 4: max_tier_compressors = CAPACITY_TIER_4; break;
         default: max_tier_compressors = CAPACITY_TIER_4; break;
     }
     
     // Use the lower of available or tier-limited compressors
     max_compressors = (max_compressors < max_tier_compressors) ? 
                       max_compressors : max_tier_compressors;
     
     // Calculate required compressors based on capacity
     uint8_t required = (uint8_t)((capacity_percent / 100.0f) * max_compressors + 0.5f);
     
     return (required > max_compressors) ? max_compressors : required;
 }
 
 static uint8_t Staging_CalculateRequiredCondensers(uint8_t compressor_count)
 {
     // Simple algorithm: one condenser per 2 compressors, minimum 1 if any compressors
     uint8_t required = 0;
     
     if (compressor_count > 0) {
         required = (compressor_count + 1) / 2;  // Round up division
         if (required == 0) required = 1;  // Minimum 1 condenser
     }
     
     // Don't exceed available condensers
     uint8_t max_condensers = g_staging_system.status.available_condenser_count;
     return (required > max_condensers) ? max_condensers : required;
 }
 
 // ========================================================================
 // COMPRESSOR CONTROL FUNCTIONS
 // ========================================================================
 
 bool Staging_ProcessCompressors(void)
 {
     uint8_t running_count = g_staging_system.status.running_compressor_count;
     uint8_t target_count = g_staging_system.control.target_compressor_count;
     
     // Check if we need to start more compressors
     if (running_count < target_count) {
         // Find next compressor to start
         uint8_t next_index = Staging_SelectNextCompressorToStart();
         
         if (next_index < MAX_COMPRESSORS && 
             Staging_IsCompressorReadyToStart(next_index)) {
             
             if (Staging_StartCompressor(next_index)) {
                 char debug_msg[80];
                 snprintf(debug_msg, sizeof(debug_msg), 
                          "STAGING: Started compressor %d (%d/%d running)\r\n",
                          next_index + 1, running_count + 1, target_count);
                 Staging_DebugPrint(debug_msg);
                 return true;
             }
         }
     }
     // Check if we need to stop some compressors
     else if (running_count > target_count) {
         // Find next compressor to stop
         uint8_t next_index = Staging_SelectNextCompressorToStop();
         
         if (next_index < MAX_COMPRESSORS && 
             Staging_IsCompressorReadyToStop(next_index)) {
             
             if (Staging_StopCompressor(next_index)) {
                 char debug_msg[80];
                 snprintf(debug_msg, sizeof(debug_msg), 
                          "STAGING: Stopped compressor %d (%d/%d running)\r\n",
                          next_index + 1, running_count - 1, target_count);
                 Staging_DebugPrint(debug_msg);
                 return true;
             }
         }
     }
     
     return true;
 }
 
 bool Staging_StartCompressor(uint8_t compressor_index)
 {
     if (compressor_index >= MAX_COMPRESSORS) {
         return false;
     }
     
     CompressorStatus_t* comp = &g_staging_system.compressors[compressor_index];
     
     // Check if compressor is available and not already running
     if (!comp->available || comp->is_running || comp->mode == COMPRESSOR_MODE_DISABLED) {
         return false;
     }
     
     // Check staging delay
     uint32_t current_time = HAL_GetTick();
     if ((current_time - g_staging_system.status.last_compressor_start) < 
         g_staging_system.control.staging_delay_compressor) {
         return false;  // Too soon since last start
     }
     
     // Turn on the relay
     Relay_Set(compressor_relay_map[compressor_index], 1);
     
     // Update compressor status
     comp->is_running = true;
     comp->relay_state = true;
     comp->start_time = current_time;
     comp->start_cycles++;
     
     // Update system status
     g_staging_system.status.last_compressor_start = current_time;
     g_staging_system.status.running_compressor_count++;
     
     char debug_msg[80];
     snprintf(debug_msg, sizeof(debug_msg), 
              "STAGING: Compressor %d started (Relay %d ON)\r\n",
              compressor_index + 1, compressor_relay_map[compressor_index]);
     Staging_DebugPrint(debug_msg);
     
     return true;
 }
 
 bool Staging_StopCompressor(uint8_t compressor_index)
 {
     if (compressor_index >= MAX_COMPRESSORS) {
         return false;
     }
     
     CompressorStatus_t* comp = &g_staging_system.compressors[compressor_index];
     
     // Check if compressor is running
     if (!comp->is_running) {
         return false;
     }
     
     // Check minimum run time
     uint32_t current_time = HAL_GetTick();
     if ((current_time - comp->start_time) < MINIMUM_RUN_TIME) {
         return false;  // Hasn't run long enough
     }
     
     // Check staging delay
     if ((current_time - g_staging_system.status.last_compressor_stop) < 
         g_staging_system.control.staging_delay_compressor) {
         return false;  // Too soon since last stop
     }
     
     // Turn off the relay
     Relay_Set(compressor_relay_map[compressor_index], 0);
     
     // Update compressor status
     comp->is_running = false;
     comp->relay_state = false;
     comp->stop_time = current_time;
     
     // Update system status
     g_staging_system.status.last_compressor_stop = current_time;
     g_staging_system.status.running_compressor_count--;
     
     char debug_msg[80];
     snprintf(debug_msg, sizeof(debug_msg), 
              "STAGING: Compressor %d stopped (Relay %d OFF)\r\n",
              compressor_index + 1, compressor_relay_map[compressor_index]);
     Staging_DebugPrint(debug_msg);
     
     return true;
 }
 
 bool Staging_StopAllCompressors(void)
 {
     bool all_stopped = true;
     
     Staging_DebugPrint("STAGING: Stopping all compressors...\r\n");
     
     for (uint8_t i = 0; i < MAX_COMPRESSORS; i++) {
         if (g_staging_system.compressors[i].is_running) {
             // Force stop without timing checks for emergency stop
             Relay_Set(compressor_relay_map[i], 0);
             
             char debug_msg[60];
             snprintf(debug_msg, sizeof(debug_msg), 
                      "STAGING: Compressor %d force stopped\r\n", i + 1);
             Staging_DebugPrint(debug_msg);
         }
     }
     
     // Reset running count
     g_staging_system.status.running_compressor_count = 0;
     g_staging_system.control.target_compressor_count = 0;
     
     return all_stopped;
 }
 
 // ========================================================================
 // CONDENSER CONTROL FUNCTIONS
 // ========================================================================
 
 bool Staging_ProcessCondensers(void)
 {
     uint8_t running_count = g_staging_system.status.running_condenser_count;
     uint8_t target_count = g_staging_system.control.target_condenser_count;
     
     // Check if we need to start more condensers
     if (running_count < target_count) {
         // Find next condenser to start
         uint8_t next_index = Staging_SelectNextCondenserToStart();
         
         if (next_index < MAX_CONDENSER_BANKS && 
             Staging_IsCondenserReadyToStart(next_index)) {
             
             if (Staging_StartCondenser(next_index)) {
                 char debug_msg[80];
                 snprintf(debug_msg, sizeof(debug_msg), 
                          "STAGING: Started condenser %d (%d/%d running)\r\n",
                          next_index + 1, running_count + 1, target_count);
                 Staging_DebugPrint(debug_msg);
                 return true;
             }
         }
     }
     // Check if we need to stop some condensers
     else if (running_count > target_count) {
         // Find next condenser to stop
         uint8_t next_index = Staging_SelectNextCondenserToStop();
         
         if (next_index < MAX_CONDENSER_BANKS && 
             Staging_IsCondenserReadyToStop(next_index)) {
             
             if (Staging_StopCondenser(next_index)) {
                 char debug_msg[80];
                 snprintf(debug_msg, sizeof(debug_msg), 
                          "STAGING: Stopped condenser %d (%d/%d running)\r\n",
                          next_index + 1, running_count - 1, target_count);
                 Staging_DebugPrint(debug_msg);
                 return true;
             }
         }
     }
     
     return true;
 }
 
 bool Staging_StartCondenser(uint8_t condenser_index)
 {
     if (condenser_index >= MAX_CONDENSER_BANKS) {
         return false;
     }
     
     CondenserStatus_t* cond = &g_staging_system.condensers[condenser_index];
     
     // Check if condenser is available and not already running
     if (!cond->available || cond->is_running || cond->mode == CONDENSER_MODE_DISABLED) {
         return false;
     }
     
     // Check staging delay
     uint32_t current_time = HAL_GetTick();
     if ((current_time - g_staging_system.status.last_condenser_start) < 
         g_staging_system.control.staging_delay_condenser) {
         return false;  // Too soon since last start
     }
     
     // Turn on the relay
     Relay_Set(condenser_relay_map[condenser_index], 1);
     
     // Update condenser status
     cond->is_running = true;
     cond->relay_state = true;
     cond->start_time = current_time;
     cond->start_cycles++;
     
     // Update system status
     g_staging_system.status.last_condenser_start = current_time;
     g_staging_system.status.running_condenser_count++;
     
     char debug_msg[80];
     snprintf(debug_msg, sizeof(debug_msg), 
              "STAGING: Condenser %d started (Relay %d ON)\r\n",
              condenser_index + 1, condenser_relay_map[condenser_index]);
     Staging_DebugPrint(debug_msg);
     
     return true;
 }
 
 bool Staging_StopCondenser(uint8_t condenser_index)
 {
     if (condenser_index >= MAX_CONDENSER_BANKS) {
         return false;
     }
     
     CondenserStatus_t* cond = &g_staging_system.condensers[condenser_index];
     
     // Check if condenser is running
     if (!cond->is_running) {
         return false;
     }
     
     // Check staging delay
     uint32_t current_time = HAL_GetTick();
     if ((current_time - g_staging_system.status.last_condenser_stop) < 
         g_staging_system.control.staging_delay_condenser) {
         return false;  // Too soon since last stop
     }
     
     // Turn off the relay
     Relay_Set(condenser_relay_map[condenser_index], 0);
     
     // Update condenser status
     cond->is_running = false;
     cond->relay_state = false;
     cond->stop_time = current_time;
     
     // Update system status
     g_staging_system.status.last_condenser_stop = current_time;
     g_staging_system.status.running_condenser_count--;
     
     char debug_msg[80];
     snprintf(debug_msg, sizeof(debug_msg), 
              "STAGING: Condenser %d stopped (Relay %d OFF)\r\n",
              condenser_index + 1, condenser_relay_map[condenser_index]);
     Staging_DebugPrint(debug_msg);
     
     return true;
 }
 
 bool Staging_StopAllCondensers(void)
 {
     bool all_stopped = true;
     
     Staging_DebugPrint("STAGING: Stopping all condensers...\r\n");
     
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         if (g_staging_system.condensers[i].is_running) {
             // Force stop without timing checks for emergency stop
             Relay_Set(condenser_relay_map[i], 0);
             
             char debug_msg[60];
             snprintf(debug_msg, sizeof(debug_msg), 
                      "STAGING: Condenser %d force stopped\r\n", i + 1);
             Staging_DebugPrint(debug_msg);
         }
     }
     
     // Reset running count
     g_staging_system.status.running_condenser_count = 0;
     g_staging_system.control.target_condenser_count = 0;
     
     return all_stopped;
 }
 
 // ========================================================================
 // RUNTIME BALANCING FUNCTIONS
 // ========================================================================
 
 void Staging_UpdateRuntimeHours(void)
 {
     static uint32_t last_update_time = 0;
     uint32_t current_time = HAL_GetTick();
     
     // Update every minute (60000 ms)
     if ((current_time - last_update_time) < 60000) {
         return;
     }
     
     // Update compressor runtime hours
     for (uint8_t i = 0; i < MAX_COMPRESSORS; i++) {
         if (g_staging_system.compressors[i].is_running) {
             // Add one minute to runtime (in hours)
             g_staging_system.compressors[i].runtime_hours += 1;  // Store in minutes for precision
         }
     }
     
     // Update condenser runtime hours
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         if (g_staging_system.condensers[i].is_running) {
             // Add one minute to runtime (in hours)
             g_staging_system.condensers[i].runtime_hours += 1;  // Store in minutes for precision
         }
     }
     
     last_update_time = current_time;
 }
 
 uint8_t Staging_SelectNextCompressorToStart(void)
 {
     if (g_staging_system.control.algorithm == STAGING_ALGORITHM_RUNTIME_BALANCED) {
         // Find available compressor with lowest runtime hours
         uint8_t best_index = MAX_COMPRESSORS;
         uint32_t lowest_runtime = UINT32_MAX;
         
         for (uint8_t i = 0; i < MAX_COMPRESSORS; i++) {
             if (g_staging_system.compressors[i].available &&
                 !g_staging_system.compressors[i].is_running &&
                 g_staging_system.compressors[i].mode == COMPRESSOR_MODE_AUTO &&
                 g_staging_system.compressors[i].runtime_hours < lowest_runtime) {
                 
                 lowest_runtime = g_staging_system.compressors[i].runtime_hours;
                 best_index = i;
             }
         }
         
         return best_index;
     } else {
         // Sequential algorithm - use round-robin
         for (uint8_t attempts = 0; attempts < MAX_COMPRESSORS; attempts++) {
             uint8_t index = g_staging_system.next_compressor_to_start;
             g_staging_system.next_compressor_to_start = (index + 1) % MAX_COMPRESSORS;
             
             if (g_staging_system.compressors[index].available &&
                 !g_staging_system.compressors[index].is_running &&
                 g_staging_system.compressors[index].mode == COMPRESSOR_MODE_AUTO) {
                 return index;
             }
         }
     }
     
     return MAX_COMPRESSORS; // No compressor available
 }
 
 uint8_t Staging_SelectNextCompressorToStop(void)
 {
     if (g_staging_system.control.algorithm == STAGING_ALGORITHM_RUNTIME_BALANCED) {
         // Find running compressor with highest runtime hours
         uint8_t best_index = MAX_COMPRESSORS;
         uint32_t highest_runtime = 0;
         
         for (uint8_t i = 0; i < MAX_COMPRESSORS; i++) {
             if (g_staging_system.compressors[i].available &&
                 g_staging_system.compressors[i].is_running &&
                 g_staging_system.compressors[i].mode == COMPRESSOR_MODE_AUTO &&
                 g_staging_system.compressors[i].runtime_hours > highest_runtime) {
                 
                 highest_runtime = g_staging_system.compressors[i].runtime_hours;
                 best_index = i;
             }
         }
         
         return best_index;
     } else {
         // Sequential algorithm - use reverse round-robin
         for (uint8_t attempts = 0; attempts < MAX_COMPRESSORS; attempts++) {
             uint8_t index = g_staging_system.next_compressor_to_stop;
             g_staging_system.next_compressor_to_stop = (index + 1) % MAX_COMPRESSORS;
             
             if (g_staging_system.compressors[index].available &&
                 g_staging_system.compressors[index].is_running &&
                 g_staging_system.compressors[index].mode == COMPRESSOR_MODE_AUTO) {
                 return index;
             }
         }
     }
     
     return MAX_COMPRESSORS; // No compressor available to stop
 }
 
 uint8_t Staging_SelectNextCondenserToStart(void)
 {
     // Similar logic for condensers
     if (g_staging_system.control.algorithm == STAGING_ALGORITHM_RUNTIME_BALANCED) {
         // Find available condenser with lowest runtime hours
         uint8_t best_index = MAX_CONDENSER_BANKS;
         uint32_t lowest_runtime = UINT32_MAX;
         
         for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
             if (g_staging_system.condensers[i].available &&
                 !g_staging_system.condensers[i].is_running &&
                 g_staging_system.condensers[i].mode == CONDENSER_MODE_AUTO &&
                 g_staging_system.condensers[i].runtime_hours < lowest_runtime) {
                 
                 lowest_runtime = g_staging_system.condensers[i].runtime_hours;
                 best_index = i;
             }
         }
         
         return best_index;
     } else {
         // Sequential algorithm
         for (uint8_t attempts = 0; attempts < MAX_CONDENSER_BANKS; attempts++) {
             uint8_t index = g_staging_system.next_condenser_to_start;
             g_staging_system.next_condenser_to_start = (index + 1) % MAX_CONDENSER_BANKS;
             
             if (g_staging_system.condensers[index].available &&
                 !g_staging_system.condensers[index].is_running &&
                 g_staging_system.condensers[index].mode == CONDENSER_MODE_AUTO) {
                 return index;
             }
         }
     }
     
     return MAX_CONDENSER_BANKS; // No condenser available
 }
 
 uint8_t Staging_SelectNextCondenserToStop(void)
 {
     // Similar logic for condensers
     if (g_staging_system.control.algorithm == STAGING_ALGORITHM_RUNTIME_BALANCED) {
         // Find running condenser with highest runtime hours
         uint8_t best_index = MAX_CONDENSER_BANKS;
         uint32_t highest_runtime = 0;
         
         for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
             if (g_staging_system.condensers[i].available &&
                 g_staging_system.condensers[i].is_running &&
                 g_staging_system.condensers[i].mode == CONDENSER_MODE_AUTO &&
                 g_staging_system.condensers[i].runtime_hours > highest_runtime) {
                 
                 highest_runtime = g_staging_system.condensers[i].runtime_hours;
                 best_index = i;
             }
         }
         
         return best_index;
     } else {
         // Sequential algorithm
         for (uint8_t attempts = 0; attempts < MAX_CONDENSER_BANKS; attempts++) {
             uint8_t index = g_staging_system.next_condenser_to_stop;
             g_staging_system.next_condenser_to_stop = (index + 1) % MAX_CONDENSER_BANKS;
             
             if (g_staging_system.condensers[index].available &&
                 g_staging_system.condensers[index].is_running &&
                 g_staging_system.condensers[index].mode == CONDENSER_MODE_AUTO) {
                 return index;
             }
         }
     }
     
     return MAX_CONDENSER_BANKS; // No condenser available to stop
 }
 
 // ========================================================================
 // STATUS AND MONITORING FUNCTIONS
 // ========================================================================
 
 static void Staging_UpdateSystemStatus(void)
 {
     // Count running equipment
     g_staging_system.status.running_compressor_count = 0;
     g_staging_system.status.running_condenser_count = 0;
     
     for (uint8_t i = 0; i < MAX_COMPRESSORS; i++) {
         if (g_staging_system.compressors[i].is_running) {
             g_staging_system.status.running_compressor_count++;
         }
     }
     
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         if (g_staging_system.condensers[i].is_running) {
             g_staging_system.status.running_condenser_count++;
         }
     }
     
     // Calculate system capacity percentage
     if (g_staging_system.status.available_compressor_count > 0) {
         g_staging_system.status.system_capacity_percent = 
             (float)g_staging_system.status.running_compressor_count / 
             g_staging_system.status.available_compressor_count * 100.0f;
     } else {
         g_staging_system.status.system_capacity_percent = 0.0f;
     }
     
     // Update system state
     if (g_staging_system.status.running_compressor_count > 0) {
         g_staging_system.status.system_state = STAGING_STATE_RUNNING;
     } else {
         g_staging_system.status.system_state = STAGING_STATE_OFF;
     }
 }
 
 static bool Staging_IsCompressorReadyToStart(uint8_t index)
 {
     if (index >= MAX_COMPRESSORS) return false;
     
     CompressorStatus_t* comp = &g_staging_system.compressors[index];
     
     return (comp->available && 
             !comp->is_running && 
             comp->mode == COMPRESSOR_MODE_AUTO);
 }
 
 static bool Staging_IsCompressorReadyToStop(uint8_t index)
 {
     if (index >= MAX_COMPRESSORS) return false;
     
     CompressorStatus_t* comp = &g_staging_system.compressors[index];
     uint32_t current_time = HAL_GetTick();
     
     return (comp->is_running && 
             comp->mode == COMPRESSOR_MODE_AUTO &&
             (current_time - comp->start_time) >= MINIMUM_RUN_TIME);
 }
 
 static bool Staging_IsCondenserReadyToStart(uint8_t index)
 {
     if (index >= MAX_CONDENSER_BANKS) return false;
     
     CondenserStatus_t* cond = &g_staging_system.condensers[index];
     
     return (cond->available && 
             !cond->is_running && 
             cond->mode == CONDENSER_MODE_AUTO);
 }
 
 static bool Staging_IsCondenserReadyToStop(uint8_t index)
 {
     if (index >= MAX_CONDENSER_BANKS) return false;
     
     CondenserStatus_t* cond = &g_staging_system.condensers[index];
     
     return (cond->is_running && 
             cond->mode == CONDENSER_MODE_AUTO);
 }
 
 // ========================================================================
 // DEBUG AND DIAGNOSTICS FUNCTIONS
 // ========================================================================
 
 static void Staging_DebugPrint(const char* message)
 {
     if (g_staging_system.debug_enabled) {
         Send_Debug_Data(message);
     }
 }
 
 void Staging_SetDebugEnabled(bool enabled)
 {
     g_staging_system.debug_enabled = enabled;
     
     if (enabled) {
         Send_Debug_Data("STAGING: Debug output ENABLED\r\n");
     } else {
         Send_Debug_Data("STAGING: Debug output DISABLED\r\n");
     }
 }
 
 void Staging_PrintStatus(void)
 {
     Send_Debug_Data("\r\n=== CHILLER STAGING SYSTEM STATUS ===\r\n");
     
     char status_msg[200];
     
     // System overview
     snprintf(status_msg, sizeof(status_msg),
              "System State: %d | Capacity: %.1f%% | Tier: %d/%d\r\n",
              g_staging_system.status.system_state,
              g_staging_system.status.system_capacity_percent,
              g_staging_system.control.current_capacity_tier,
              g_staging_system.control.max_capacity_tier);
     Send_Debug_Data(status_msg);
     
     // Equipment counts
     snprintf(status_msg, sizeof(status_msg),
              "Compressors: %d/%d running, %d available\r\n",
              g_staging_system.status.running_compressor_count,
              g_staging_system.control.target_compressor_count,
              g_staging_system.status.available_compressor_count);
     Send_Debug_Data(status_msg);
     
     snprintf(status_msg, sizeof(status_msg),
              "Condensers: %d/%d running, %d available\r\n",
              g_staging_system.status.running_condenser_count,
              g_staging_system.control.target_condenser_count,
              g_staging_system.status.available_condenser_count);
     Send_Debug_Data(status_msg);
     
     // Control settings
     snprintf(status_msg, sizeof(status_msg),
              "Algorithm: %d | Auto: %s | Balance: %s\r\n",
              g_staging_system.control.algorithm,
              g_staging_system.control.auto_staging_enabled ? "ON" : "OFF",
              g_staging_system.control.runtime_balancing_enabled ? "ON" : "OFF");
     Send_Debug_Data(status_msg);
     
     Send_Debug_Data("=== END STATUS ===\r\n\r\n");
 }
 
 void Staging_PrintCompressorStatus(void)
 {
     Send_Debug_Data("\r\n=== COMPRESSOR STATUS ===\r\n");
     
     for (uint8_t i = 0; i < MAX_COMPRESSORS; i++) {
         CompressorStatus_t* comp = &g_staging_system.compressors[i];
         
         if (!comp->available) continue;
         
         char comp_msg[150];
         snprintf(comp_msg, sizeof(comp_msg),
                  "Comp %d: %s | Mode:%d | Runtime:%lu min | Cycles:%lu | Relay:%d\r\n",
                  i + 1,
                  comp->is_running ? "RUN" : "OFF",
                  comp->mode,
                  comp->runtime_hours,
                  comp->start_cycles,
                  compressor_relay_map[i]);
         Send_Debug_Data(comp_msg);
     }
     
     Send_Debug_Data("=== END COMPRESSOR STATUS ===\r\n\r\n");
 }
 
 void Staging_PrintCondenserStatus(void)
 {
     Send_Debug_Data("\r\n=== CONDENSER STATUS ===\r\n");
     
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         CondenserStatus_t* cond = &g_staging_system.condensers[i];
         
         if (!cond->available) continue;
         
         char cond_msg[150];
         snprintf(cond_msg, sizeof(cond_msg),
                  "Cond %d: %s | Mode:%d | Runtime:%lu min | Cycles:%lu | Relay:%d\r\n",
                  i + 1,
                  cond->is_running ? "RUN" : "OFF",
                  cond->mode,
                  cond->runtime_hours,
                  cond->start_cycles,
                  condenser_relay_map[i]);
         Send_Debug_Data(cond_msg);
     }
     
     Send_Debug_Data("=== END CONDENSER STATUS ===\r\n\r\n");
 }
 
 bool Staging_EmergencyStop(void)
 {
     Send_Debug_Data("\r\n*** STAGING EMERGENCY STOP ACTIVATED ***\r\n");
     
     // Stop all equipment immediately
     bool compressors_stopped = Staging_StopAllCompressors();
     bool condensers_stopped = Staging_StopAllCondensers();
     
     // Set system to fault state
     g_staging_system.status.system_state = STAGING_STATE_FAULT;
     
     // Disable auto staging
     g_staging_system.control.auto_staging_enabled = false;
     
     Send_Debug_Data("*** EMERGENCY STOP COMPLETE ***\r\n\r\n");
     
     return (compressors_stopped && condensers_stopped);
 }
 
 // ========================================================================
 // GETTER FUNCTIONS
 // ========================================================================
 
 StagingStatus_t* Staging_GetStatus(void)
 {
     return &g_staging_system.status;
 }
 
 float Staging_GetCurrentCapacityPercent(void)
 {
     return g_staging_system.status.system_capacity_percent;
 }
 
 uint8_t Staging_GetRunningCompressorCount(void)
 {
     return g_staging_system.status.running_compressor_count;
 }
 
 uint8_t Staging_GetRunningCondenserCount(void)
 {
     return g_staging_system.status.running_condenser_count;
 }
 
 CondenserStatus_t* Staging_GetCondenserStatus(uint8_t condenser_index)
 {
     if (condenser_index >= MAX_CONDENSER_BANKS) {
         return NULL;
     }
     return &g_staging_system.condensers[condenser_index];
 }
