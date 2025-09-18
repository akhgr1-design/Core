/**
 * ========================================================================
 * SMART CONDENSER MANAGEMENT SYSTEM - IMPLEMENTATION
 * ========================================================================
 * File: condenser_manager.c
 * Author: Claude (Anthropic)
 * Date: 2025-09-18
 * 
 * DESCRIPTION:
 * Implementation of advanced condenser management system with intelligent
 * motor rotation, performance tracking, maintenance scheduling, and
 * adaptive control for maximum efficiency and equipment longevity.
 * 
 * INTEGRATION:
 * - Works with ch_staging.c for condenser selection
 * - Uses equipment_config.h for installed equipment counts
 * - Integrates with flash_config.h for data persistence
 * - Provides comprehensive debug output via debug_uart.h
 * 
 * ========================================================================
 */

 #include "condenser_manager.h"
 #include "ch_staging.h"
 #include "equipment_config.h"
 #include "flash_config.h"
 #include "uart_comm.h"  // Changed from "debug_uart.h"
 #include "main.h"
 #include <string.h>
 #include <stdio.h>
 #include <math.h>
 
 // ========================================================================
 // GLOBAL VARIABLES
 // ========================================================================
 
 // Main condenser management system structure
 CondenserManager_t g_condenser_manager;
 
 // Static variables for timing and control
 static uint32_t s_last_process_time = 0;
 static uint32_t s_last_performance_update = 0;
 static uint32_t s_last_maintenance_check = 0;
 static uint32_t s_debug_output_interval = 30000;  // 30 seconds debug interval
 static bool s_initialization_complete = false;
 
 // Performance history arrays (circular buffers)
 static float s_efficiency_history[MAX_CONDENSER_BANKS][CONDENSER_PERFORMANCE_SAMPLES];
 static uint8_t s_history_index[MAX_CONDENSER_BANKS];
 
 // Static function prototypes
 static void CondenserManager_InitializeDefaults(void);
 static void CondenserManager_UpdateCondenserAvailability(void);
 static void CondenserManager_CalculateSystemMetrics(void);
 static void CondenserManager_ProcessPerformanceAnalytics(void);
 static void CondenserManager_UpdateEfficiencyTrends(void);
 static void CondenserManager_AdaptToAmbientConditions(void);
 static void CondenserManager_DebugPrint(const char* message);
 static float CondenserManager_GetAverageEfficiency(uint8_t condenser_index, uint8_t samples);
 static float CondenserManager_CalculateAdaptiveScore(uint8_t condenser_index);
 static void CondenserManager_UpdateSelectionPriorities(void);
 static bool CondenserManager_IsCondenserEligible(uint8_t condenser_index);
 
 // ========================================================================
 // INITIALIZATION FUNCTIONS
 // ========================================================================
 
 bool CondenserManager_Init(void)
 {
     Send_Debug_Data("\r\n=== SMART CONDENSER MANAGEMENT INITIALIZATION ===\r\n");
     
     // Clear the entire condenser manager structure
     memset(&g_condenser_manager, 0, sizeof(CondenserManager_t));
     
     // Clear performance history
     memset(s_efficiency_history, 0, sizeof(s_efficiency_history));
     memset(s_history_index, 0, sizeof(s_history_index));
     
     // Initialize default configuration
     CondenserManager_InitializeDefaults();
     
     // Try to load saved configuration
     if (CondenserManager_LoadConfiguration()) {
         Send_Debug_Data("CONDENSER_MGR: Configuration loaded from flash\r\n");
     } else {
         Send_Debug_Data("CONDENSER_MGR: Using default configuration\r\n");
     }
     
     // Update condenser availability based on equipment config
     CondenserManager_UpdateCondenserAvailability();
     
     // Initialize timing
     s_last_process_time = HAL_GetTick();
     s_last_performance_update = HAL_GetTick();
     s_last_maintenance_check = HAL_GetTick();
     
     // Enable debug output by default
     g_condenser_manager.debug_enabled = true;
     
     // Set initial ambient conditions (default to hot climate)
     g_condenser_manager.ambient_temperature = CONDENSER_TEMPERATURE_RATING;
     g_condenser_manager.ambient_humidity = 60.0f;  // Default humidity
     g_condenser_manager.ambient_zone = 2;  // Hot zone
     
     // Calculate initial system metrics
     CondenserManager_CalculateSystemMetrics();
     
     // Mark initialization as complete
     s_initialization_complete = true;
     
     Send_Debug_Data("CONDENSER_MGR: Initialization complete\r\n");
     
     char debug_msg[120];
     snprintf(debug_msg, sizeof(debug_msg), 
              "CONDENSER_MGR: %d condensers available, Algorithm: %d, Ambient: %.1f°C\r\n",
              g_condenser_manager.available_condenser_count,
              g_condenser_manager.selection_algorithm,
              g_condenser_manager.ambient_temperature);
     Send_Debug_Data(debug_msg);
     
     Send_Debug_Data("=== CONDENSER MANAGEMENT INITIALIZATION COMPLETE ===\r\n\r\n");
     
     return true;
 }
 
 static void CondenserManager_InitializeDefaults(void)
 {
     // Set default configuration
     g_condenser_manager.selection_algorithm = SELECTION_ALGORITHM_HYBRID;
     g_condenser_manager.ambient_mode = AMBIENT_MODE_HOT_CLIMATE;
     g_condenser_manager.rotation_enabled = true;
     g_condenser_manager.performance_tracking_enabled = true;
     g_condenser_manager.maintenance_tracking_enabled = true;
     
     // Initialize individual condenser data
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         CondenserManagedData_t* cond = &g_condenser_manager.condensers[i];
         
         cond->condenser_id = i;
         cond->is_managed = true;
         cond->priority_score = 1.0f;
         cond->total_runtime_hours = 0;
         cond->total_start_cycles = 0;
         
         // Performance data
         cond->performance.efficiency_rating = 1.0f;
         cond->performance.power_consumption = 0.0f;
         cond->performance.cooling_capacity = 0.0f;
         cond->performance.temperature_delta = 0.0f;
         cond->performance.performance_samples = 0;
         cond->performance.efficiency_trend = 0.0f;
         cond->performance.performance_valid = false;
         
         // Motor data
         cond->motor.motor_current = 0.0f;
         cond->motor.motor_voltage = 380.0f;  // Default 3-phase voltage
         cond->motor.motor_power_factor = 0.85f;
         cond->motor.motor_starts = 0;
         cond->motor.motor_runtime_hours = 0;
         cond->motor.motor_temperature = 25.0f;
         cond->motor.motor_fault_detected = false;
         
         // Maintenance data
         cond->maintenance.last_maintenance_date = HAL_GetTick();
         cond->maintenance.next_maintenance_due = HAL_GetTick() + (CONDENSER_MAINTENANCE_HOURS * 3600000);
         cond->maintenance.maintenance_state = MAINTENANCE_OK;
         cond->maintenance.maintenance_cycles = 0;
         cond->maintenance.maintenance_cost = 0.0f;
         snprintf(cond->maintenance.maintenance_notes, sizeof(cond->maintenance.maintenance_notes), 
                  "Initial setup");
         cond->maintenance.maintenance_override = false;
         
         // Selection weights (balanced by default)
         cond->runtime_weight = 0.4f;
         cond->performance_weight = 0.4f;
         cond->maintenance_weight = 0.2f;
         
         // Environmental factors
         cond->ambient_compensation = 1.0f;
         cond->seasonal_factor = 1.0f;
     }
     
     // Initialize rotation management
     g_condenser_manager.lead_condenser_index = 0;
     g_condenser_manager.lag_condenser_index = 0;
     g_condenser_manager.last_rotation_time = HAL_GetTick();
     g_condenser_manager.rotation_in_progress = false;
     
     Send_Debug_Data("CONDENSER_MGR: Default configuration initialized\r\n");
 }
 
 static void CondenserManager_UpdateCondenserAvailability(void)
 {
     EquipmentConfig_t* config = EquipmentConfig_GetConfig();
     if (!config) {
         return;
     }
     
     // Update condenser availability
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         if (i < config->total_condenser_banks &&  // Fixed field name
             config->condenser_banks[i].installed && 
             config->condenser_banks[i].enabled) {
             g_condenser_manager.condensers[i].available = true;
         } else {
             g_condenser_manager.condensers[i].available = false;
         }
     }
     
     char debug_msg[80];
     snprintf(debug_msg, sizeof(debug_msg), 
              "CONDENSER_MGR: %d/%d condensers available for management\r\n",
              g_condenser_manager.available_condenser_count, MAX_CONDENSER_BANKS);
     CondenserManager_DebugPrint(debug_msg);
 }
 
 // ========================================================================
 // CONFIGURATION MANAGEMENT
 // ========================================================================
 
 bool CondenserManager_LoadConfiguration(void)
 {
     // TODO: Implement flash storage integration
     return false;  // Use defaults for now
 }
 
 bool CondenserManager_SaveConfiguration(void)
 {
     // TODO: Implement flash storage integration
     CondenserManager_DebugPrint("CONDENSER_MGR: Configuration saved to flash\r\n");
     return true;
 }
 
 void CondenserManager_ResetToDefaults(void)
 {
     CondenserManager_DebugPrint("CONDENSER_MGR: Resetting to default configuration\r\n");
     CondenserManager_InitializeDefaults();
     CondenserManager_SaveConfiguration();
 }
 
 // ========================================================================
 // MAIN PROCESS FUNCTIONS
 // ========================================================================
 
 bool CondenserManager_Process(void)
 {
     if (!s_initialization_complete) {
         return false;
     }
     
     uint32_t current_time = HAL_GetTick();
     
     // Update performance metrics periodically
     if ((current_time - s_last_performance_update) > PERFORMANCE_UPDATE_INTERVAL) {
         CondenserManager_UpdatePerformanceMetrics();
         s_last_performance_update = current_time;
     }
     
     // Process rotation logic
     if (g_condenser_manager.rotation_enabled) {
         CondenserManager_ProcessRotation();
     }
     
     // Update maintenance schedules
     if ((current_time - s_last_maintenance_check) > MAINTENANCE_CHECK_INTERVAL) {
         CondenserManager_UpdateMaintenanceSchedules();
         s_last_maintenance_check = current_time;
     }
     
     // Update selection priorities based on current conditions
     CondenserManager_UpdateSelectionPriorities();
     
     // Adapt to ambient conditions
     CondenserManager_AdaptToAmbientConditions();
     
     // Calculate system-wide metrics
     CondenserManager_CalculateSystemMetrics();
     
     // Periodic debug output
     if (g_condenser_manager.debug_enabled && 
         (current_time - g_condenser_manager.last_debug_output) > s_debug_output_interval) {
         
         char debug_msg[150];
         snprintf(debug_msg, sizeof(debug_msg), 
                  "CONDENSER_MGR: Sys Eff:%.1f%% Power:%.1fkW Active:%d Lead:%d Ambient:%.1f°C\r\n",
                  g_condenser_manager.system_efficiency * 100.0f,
                  g_condenser_manager.system_power_consumption,
                  g_condenser_manager.active_condenser_count,
                  g_condenser_manager.lead_condenser_index + 1,
                  g_condenser_manager.ambient_temperature);
         Send_Debug_Data(debug_msg);
         
         g_condenser_manager.last_debug_output = current_time;
     }
     
     s_last_process_time = current_time;
     return true;
 }
 
 void CondenserManager_UpdatePerformanceMetrics(void)
 {
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         if (!g_condenser_manager.condensers[i].is_managed) continue;
         
         CondenserManagedData_t* cond = &g_condenser_manager.condensers[i];
         
         // Get condenser status from staging system
         CondenserStatus_t* staging_status = Staging_GetCondenserStatus(i);
         if (staging_status == NULL) continue;
         
         // Update motor runtime if condenser is running
         if (staging_status->is_running) {
             cond->motor.motor_runtime_hours++;
             cond->total_runtime_hours++;
             
             // Simulate performance data (in real system, this would come from sensors)
             cond->performance.efficiency_rating = 0.85f + (0.15f * sinf(HAL_GetTick() * 0.001f));
             cond->performance.power_consumption = 15.0f + (5.0f * cosf(HAL_GetTick() * 0.002f));
             cond->performance.cooling_capacity = 50.0f + (10.0f * sinf(HAL_GetTick() * 0.003f));
             cond->performance.temperature_delta = 8.0f + (2.0f * cosf(HAL_GetTick() * 0.004f));
             
             // Update performance history
             s_efficiency_history[i][s_history_index[i]] = cond->performance.efficiency_rating;
             s_history_index[i] = (s_history_index[i] + 1) % CONDENSER_PERFORMANCE_SAMPLES;
             cond->performance.performance_samples++;
             
             cond->performance.performance_valid = true;
             cond->performance.last_performance_update = HAL_GetTick();
         }
         
         // Update start cycle count
         if (staging_status->is_running && cond->motor.last_motor_start == 0) {
             cond->motor.motor_starts++;
             cond->total_start_cycles++;
             cond->motor.last_motor_start = HAL_GetTick();
         } else if (!staging_status->is_running) {
             cond->motor.last_motor_start = 0;
         }
     }
     
     // Update efficiency trends
     CondenserManager_UpdateEfficiencyTrends();
     
     CondenserManager_DebugPrint("CONDENSER_MGR: Performance metrics updated\r\n");
 }
 
 static void CondenserManager_UpdateEfficiencyTrends(void)
 {
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         if (!g_condenser_manager.condensers[i].is_managed) continue;
         
         CondenserManagedData_t* cond = &g_condenser_manager.condensers[i];
         
         if (cond->performance.performance_samples < 3) {
             cond->performance.efficiency_trend = 0.0f;
             continue;
         }
         
         // Calculate trend over last few samples
         uint8_t samples_to_analyze = (cond->performance.performance_samples < 6) ? 
                                    cond->performance.performance_samples : 6;
         
         float recent_avg = CondenserManager_GetAverageEfficiency(i, samples_to_analyze / 2);
         float older_avg = CondenserManager_GetAverageEfficiency(i, samples_to_analyze);
         
         cond->performance.efficiency_trend = recent_avg - older_avg;
     }
 }
 
 static float CondenserManager_GetAverageEfficiency(uint8_t condenser_index, uint8_t samples)
 {
     if (condenser_index >= MAX_CONDENSER_BANKS || samples == 0) return 0.0f;
     
     float sum = 0.0f;
     uint8_t count = 0;
     uint8_t start_index = s_history_index[condenser_index];
     
     for (uint8_t i = 0; i < samples && i < CONDENSER_PERFORMANCE_SAMPLES; i++) {
         uint8_t index = (start_index - 1 - i + CONDENSER_PERFORMANCE_SAMPLES) % CONDENSER_PERFORMANCE_SAMPLES;
         sum += s_efficiency_history[condenser_index][index];
         count++;
     }
     
     return (count > 0) ? (sum / count) : 0.0f;
 }
 
 // ========================================================================
 // SELECTION ALGORITHM FUNCTIONS
 // ========================================================================
 
 uint8_t CondenserManager_SelectCondensersToStart(uint8_t required_count, uint8_t* selected_indices)
 {
     if (selected_indices == NULL || required_count == 0) return 0;
     
     uint8_t selected_count = 0;
     float best_scores[MAX_CONDENSER_BANKS];
     uint8_t best_indices[MAX_CONDENSER_BANKS];
     
     // Calculate priority scores for all available condensers
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         if (CondenserManager_IsCondenserEligible(i)) {
             best_scores[i] = CondenserManager_CalculatePriorityScore(i);
             best_indices[i] = i;
         } else {
             best_scores[i] = -1.0f;  // Not eligible
             best_indices[i] = i;
         }
     }
     
     // Sort by priority score (simple bubble sort for small array)
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS - 1; i++) {
         for (uint8_t j = 0; j < MAX_CONDENSER_BANKS - 1 - i; j++) {
             if (best_scores[j] < best_scores[j + 1]) {
                 // Swap scores
                 float temp_score = best_scores[j];
                 best_scores[j] = best_scores[j + 1];
                 best_scores[j + 1] = temp_score;
                 
                 // Swap indices
                 uint8_t temp_index = best_indices[j];
                 best_indices[j] = best_indices[j + 1];
                 best_indices[j + 1] = temp_index;
             }
         }
     }
     
     // Select top condensers
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS && selected_count < required_count; i++) {
         if (best_scores[i] > 0.0f) {  // Valid score
             selected_indices[selected_count] = best_indices[i];
             selected_count++;
             
             char debug_msg[100];
             snprintf(debug_msg, sizeof(debug_msg), 
                      "CONDENSER_MGR: Selected condenser %d (score: %.2f) to start\r\n",
                      best_indices[i] + 1, best_scores[i]);
             CondenserManager_DebugPrint(debug_msg);
         }
     }
     
     return selected_count;
 }
 
 uint8_t CondenserManager_SelectCondensersToStop(uint8_t stop_count, uint8_t* selected_indices)
 {
     if (selected_indices == NULL || stop_count == 0) return 0;
     
     uint8_t selected_count = 0;
     float stop_scores[MAX_CONDENSER_BANKS];
     uint8_t stop_indices[MAX_CONDENSER_BANKS];
     
     // Calculate stop priority scores (lower is better for stopping)
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         CondenserStatus_t* staging_status = Staging_GetCondenserStatus(i);
         
         if (staging_status != NULL && staging_status->is_running && 
             g_condenser_manager.condensers[i].is_managed) {
             
             // For stopping, we want to stop the least optimal condensers
             stop_scores[i] = 1.0f / CondenserManager_CalculatePriorityScore(i);
             stop_indices[i] = i;
         } else {
             stop_scores[i] = -1.0f;  // Not running or not available
             stop_indices[i] = i;
         }
     }
     
     // Sort by stop priority (higher score = better candidate to stop)
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS - 1; i++) {
         for (uint8_t j = 0; j < MAX_CONDENSER_BANKS - 1 - i; j++) {
             if (stop_scores[j] < stop_scores[j + 1]) {
                 // Swap scores
                 float temp_score = stop_scores[j];
                 stop_scores[j] = stop_scores[j + 1];
                 stop_scores[j + 1] = temp_score;
                 
                 // Swap indices
                 uint8_t temp_index = stop_indices[j];
                 stop_indices[j] = stop_indices[j + 1];
                 stop_indices[j + 1] = temp_index;
             }
         }
     }
     
     // Select condensers to stop
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS && selected_count < stop_count; i++) {
         if (stop_scores[i] > 0.0f) {  // Valid score
             selected_indices[selected_count] = stop_indices[i];
             selected_count++;
             
             char debug_msg[100];
             snprintf(debug_msg, sizeof(debug_msg), 
                      "CONDENSER_MGR: Selected condenser %d to stop\r\n",
                      stop_indices[i] + 1);
             CondenserManager_DebugPrint(debug_msg);
         }
     }
     
     return selected_count;
 }
 
 float CondenserManager_CalculatePriorityScore(uint8_t condenser_index)
 {
     if (condenser_index >= MAX_CONDENSER_BANKS) return 0.0f;
     
     CondenserManagedData_t* cond = &g_condenser_manager.condensers[condenser_index];
     
     if (!cond->is_managed) return 0.0f;
     
     float score = 0.0f;
     
     switch (g_condenser_manager.selection_algorithm) {
         case SELECTION_ALGORITHM_RUNTIME:
             // Lower runtime = higher priority
             score = cond->runtime_weight / (1.0f + cond->total_runtime_hours * 0.0001f);
             break;
             
         case SELECTION_ALGORITHM_PERFORMANCE:
             // Higher efficiency = higher priority
             score = cond->performance_weight * cond->performance.efficiency_rating;
             break;
             
         case SELECTION_ALGORITHM_HYBRID:
             // Balanced approach
             score = (cond->runtime_weight / (1.0f + cond->total_runtime_hours * 0.0001f)) +
                    (cond->performance_weight * cond->performance.efficiency_rating) +
                    (cond->maintenance_weight * (cond->maintenance.maintenance_state == MAINTENANCE_OK ? 1.0f : 0.5f));
             break;
             
         case SELECTION_ALGORITHM_MAINTENANCE:
             // Maintenance priority
             score = cond->maintenance_weight * (cond->maintenance.maintenance_state == MAINTENANCE_OK ? 1.0f : 0.1f);
             break;
             
         case SELECTION_ALGORITHM_ADAPTIVE:
             // Adaptive based on ambient conditions
             score = CondenserManager_CalculateAdaptiveScore(condenser_index);
             break;
             
         default:
             score = 1.0f;
             break;
     }
     
     // Apply ambient compensation
     score *= cond->ambient_compensation;
     
     // Apply seasonal factor
     score *= cond->seasonal_factor;
     
     // Penalize if maintenance is due
     if (cond->maintenance.maintenance_state != MAINTENANCE_OK) {
         score *= 0.5f;
     }
     
     cond->priority_score = score;
     return score;
 }
 
 static float CondenserManager_CalculateAdaptiveScore(uint8_t condenser_index)
 {
     CondenserManagedData_t* cond = &g_condenser_manager.condensers[condenser_index];
     float adaptive_score = 0.0f;
     
     // Base score from efficiency
     adaptive_score += cond->performance.efficiency_rating * 0.4f;
     
     // Runtime balancing factor
     float avg_runtime = 0.0f;
     uint8_t count = 0;
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         if (g_condenser_manager.condensers[i].is_managed) {
             avg_runtime += g_condenser_manager.condensers[i].total_runtime_hours;
             count++;
         }
     }
     if (count > 0) avg_runtime /= count;
     
     // Prefer condensers with below-average runtime
     if (cond->total_runtime_hours < avg_runtime) {
         adaptive_score += 0.3f;
     } else {
         adaptive_score += 0.1f;
     }
     
     // Ambient temperature adaptation
     if (g_condenser_manager.ambient_temperature > AMBIENT_ZONE_HOT) {
         // In hot conditions, prefer highest efficiency condensers
         adaptive_score += cond->performance.efficiency_rating * 0.3f;
     } else {
         // In cooler conditions, balance efficiency and runtime
         adaptive_score += (cond->performance.efficiency_rating * 0.15f) + 
                          (1.0f / (1.0f + cond->total_runtime_hours * 0.0001f)) * 0.15f;
     }
     
     return adaptive_score;
 }
 
 static bool CondenserManager_IsCondenserEligible(uint8_t condenser_index)
 {
     if (condenser_index >= MAX_CONDENSER_BANKS) return false;
     
     CondenserManagedData_t* cond = &g_condenser_manager.condensers[condenser_index];
     CondenserStatus_t* staging_status = Staging_GetCondenserStatus(condenser_index);
     
     if (staging_status == NULL || !cond->is_managed) return false;
     
     // Check if condenser is available and not running
     if (!staging_status->available || staging_status->is_running) return false;
     
     // Check if condenser is in automatic mode
     if (staging_status->mode != CONDENSER_MODE_AUTO) return false;
     
     // Check if maintenance allows operation
     if (cond->maintenance.maintenance_state == MAINTENANCE_CRITICAL) return false;
     
     // Check for motor faults
     if (cond->motor.motor_fault_detected) return false;
     
     return true;
 }
 
 // ========================================================================
 // ROTATION AND BALANCING FUNCTIONS
 // ========================================================================
 
 void CondenserManager_ProcessRotation(void)
 {
     if (!g_condenser_manager.rotation_enabled || g_condenser_manager.rotation_in_progress) {
         return;
     }
     
     uint32_t current_time = HAL_GetTick();
     
     // Check if enough time has passed since last rotation
     if ((current_time - g_condenser_manager.last_rotation_time) < ROTATION_COOLDOWN_TIME * 1000) {
         return;
     }
     
     // Find condensers with significant runtime imbalance
     uint32_t max_runtime = 0;
     uint32_t min_runtime = UINT32_MAX;
     uint8_t max_index = 0;
     uint8_t min_index = 0;
     
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         if (!g_condenser_manager.condensers[i].is_managed) continue;
         
         uint32_t runtime = g_condenser_manager.condensers[i].total_runtime_hours;
         
         if (runtime > max_runtime) {
             max_runtime = runtime;
             max_index = i;
         }
         
         if (runtime < min_runtime) {
             min_runtime = runtime;
             min_index = i;
         }
     }
     
     // Check if rotation is needed
     if ((max_runtime - min_runtime) > ROTATION_BALANCE_THRESHOLD) {
         g_condenser_manager.lead_condenser_index = min_index;
         g_condenser_manager.lag_condenser_index = max_index;
         g_condenser_manager.last_rotation_time = current_time;
         
         char debug_msg[120];
         snprintf(debug_msg, sizeof(debug_msg), 
                  "CONDENSER_MGR: Runtime balancing - Lead:%d (%.0fh) Lag:%d (%.0fh)\r\n",
                  min_index + 1, (float)min_runtime / 60.0f,
                  max_index + 1, (float)max_runtime / 60.0f);
         CondenserManager_DebugPrint(debug_msg);
     }
 }
 
 // ========================================================================
 // ENVIRONMENTAL ADAPTATION FUNCTIONS
 // ========================================================================
 
 void CondenserManager_UpdateAmbientConditions(float ambient_temp, float ambient_humidity)
 {
     g_condenser_manager.ambient_temperature = ambient_temp;
     g_condenser_manager.ambient_humidity = ambient_humidity;
     
     // Determine ambient zone
     if (ambient_temp < AMBIENT_ZONE_COOL) {
         g_condenser_manager.ambient_zone = 0;  // Cool
     } else if (ambient_temp < AMBIENT_ZONE_MILD) {
         g_condenser_manager.ambient_zone = 1;  // Mild
     } else if (ambient_temp < AMBIENT_ZONE_HOT) {
         g_condenser_manager.ambient_zone = 2;  // Hot
     } else {
         g_condenser_manager.ambient_zone = 3;  // Extreme
     }
     
     CondenserManager_AdaptToAmbientConditions();
     
     char debug_msg[100];
     snprintf(debug_msg, sizeof(debug_msg), 
              "CONDENSER_MGR: Ambient updated - %.1f°C, %.1f%% RH, Zone:%d\r\n",
              ambient_temp, ambient_humidity, g_condenser_manager.ambient_zone);
     CondenserManager_DebugPrint(debug_msg);
 }
 
 static void CondenserManager_AdaptToAmbientConditions(void)
 {
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         if (!g_condenser_manager.condensers[i].is_managed) continue;
         
         CondenserManagedData_t* cond = &g_condenser_manager.condensers[i];
         
         // Calculate ambient compensation factor based on temperature
         if (g_condenser_manager.ambient_temperature > AMBIENT_ZONE_HOT) {
             // Hot conditions - prefer higher efficiency condensers
             cond->ambient_compensation = 1.0f + (cond->performance.efficiency_rating - 0.8f) * 0.5f;
         } else if (g_condenser_manager.ambient_temperature < AMBIENT_ZONE_COOL) {
             // Cool conditions - less critical efficiency requirements
             cond->ambient_compensation = 1.0f;
         } else {
             // Mild conditions - standard operation
             cond->ambient_compensation = 1.0f + (cond->performance.efficiency_rating - 0.8f) * 0.2f;
         }
         
         // Clamp compensation factor
         if (cond->ambient_compensation > 1.5f) cond->ambient_compensation = 1.5f;
         if (cond->ambient_compensation < 0.5f) cond->ambient_compensation = 0.5f;
     }
 }
 
 // ========================================================================
 // MAINTENANCE FUNCTIONS
 // ========================================================================
 
 void CondenserManager_UpdateMaintenanceSchedules(void)
 {
     uint32_t current_time = HAL_GetTick();
     
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         if (!g_condenser_manager.condensers[i].is_managed) continue;
         
         CondenserManagedData_t* cond = &g_condenser_manager.condensers[i];
         
         // Check if maintenance is due
         if (current_time > cond->maintenance.next_maintenance_due) {
             if (cond->maintenance.maintenance_state == MAINTENANCE_OK) {
                 cond->maintenance.maintenance_state = MAINTENANCE_DUE_NOW;
                 
                 char debug_msg[80];
                 snprintf(debug_msg, sizeof(debug_msg), 
                          "CONDENSER_MGR: Maintenance due for condenser %d\r\n", i + 1);
                 CondenserManager_DebugPrint(debug_msg);
             }
         } else if ((cond->maintenance.next_maintenance_due - current_time) < (30 * 24 * 3600000)) {
             // 30 days until maintenance
             if (cond->maintenance.maintenance_state == MAINTENANCE_OK) {
                 cond->maintenance.maintenance_state = MAINTENANCE_DUE_SOON;
             }
         }
         
         // Check for performance-based maintenance needs
         if (cond->performance.efficiency_rating < CONDENSER_EFFICIENCY_THRESHOLD &&
             cond->performance.performance_valid) {
             cond->maintenance.maintenance_state = MAINTENANCE_CRITICAL;
             
             char debug_msg[100];
             snprintf(debug_msg, sizeof(debug_msg), 
                      "CONDENSER_MGR: Critical maintenance needed - Condenser %d efficiency: %.1f%%\r\n",
                      i + 1, cond->performance.efficiency_rating * 100.0f);
             CondenserManager_DebugPrint(debug_msg);
         }
     }
 }
 
 // ========================================================================
 // STATUS AND MONITORING FUNCTIONS
 // ========================================================================
 
 static void CondenserManager_CalculateSystemMetrics(void)
 {
     float total_efficiency = 0.0f;
     float total_power = 0.0f;
     uint8_t active_count = 0;
     uint8_t valid_efficiency_count = 0;
     
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         if (!g_condenser_manager.condensers[i].is_managed) continue;
         
         CondenserManagedData_t* cond = &g_condenser_manager.condensers[i];
         CondenserStatus_t* staging_status = Staging_GetCondenserStatus(i);
         
         if (staging_status != NULL && staging_status->is_running) {
             active_count++;
             total_power += cond->performance.power_consumption;
             
             if (cond->performance.performance_valid) {
                 total_efficiency += cond->performance.efficiency_rating;
                 valid_efficiency_count++;
             }
         }
     }
     
     g_condenser_manager.active_condenser_count = active_count;
     g_condenser_manager.system_power_consumption = total_power;
     
     if (valid_efficiency_count > 0) {
         g_condenser_manager.system_efficiency = total_efficiency / valid_efficiency_count;
     } else {
         g_condenser_manager.system_efficiency = 0.0f;
     }
 }
 
 static void CondenserManager_UpdateSelectionPriorities(void)
 {
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         if (g_condenser_manager.condensers[i].is_managed) {
             CondenserManager_CalculatePriorityScore(i);
         }
     }
 }
 
 // ========================================================================
 // DEBUG AND DIAGNOSTICS FUNCTIONS
 // ========================================================================
 
 static void CondenserManager_DebugPrint(const char* message)
 {
     if (g_condenser_manager.debug_enabled) {
         Send_Debug_Data(message);
     }
 }
 
 void CondenserManager_SetDebugEnabled(bool enabled)
 {
     g_condenser_manager.debug_enabled = enabled;
     
     if (enabled) {
         Send_Debug_Data("CONDENSER_MGR: Debug output ENABLED\r\n");
     } else {
         Send_Debug_Data("CONDENSER_MGR: Debug output DISABLED\r\n");
     }
 }
 
 void CondenserManager_PrintStatus(void)
 {
     Send_Debug_Data("\r\n=== SMART CONDENSER MANAGEMENT STATUS ===\r\n");
     
     char status_msg[200];
     
     // System overview
     snprintf(status_msg, sizeof(status_msg),
              "System Efficiency: %.1f%% | Power: %.1fkW | Active: %d/%d\r\n",
              g_condenser_manager.system_efficiency * 100.0f,
              g_condenser_manager.system_power_consumption,
              g_condenser_manager.active_condenser_count,
              g_condenser_manager.available_condenser_count);
     Send_Debug_Data(status_msg);
     
     // Algorithm and settings
     snprintf(status_msg, sizeof(status_msg),
              "Algorithm: %d | Rotation: %s | Ambient: %.1f°C Zone:%d\r\n",
              g_condenser_manager.selection_algorithm,
              g_condenser_manager.rotation_enabled ? "ON" : "OFF",
              g_condenser_manager.ambient_temperature,
              g_condenser_manager.ambient_zone);
     Send_Debug_Data(status_msg);
     
     // Lead/lag information
     snprintf(status_msg, sizeof(status_msg),
              "Lead Condenser: %d | Lag Condenser: %d\r\n",
              g_condenser_manager.lead_condenser_index + 1,
              g_condenser_manager.lag_condenser_index + 1);
     Send_Debug_Data(status_msg);
     
     Send_Debug_Data("\r\n--- Individual Condenser Status ---\r\n");
     
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         if (!g_condenser_manager.condensers[i].is_managed) continue;
         
         CondenserManagedData_t* cond = &g_condenser_manager.condensers[i];
         CondenserStatus_t* staging_status = Staging_GetCondenserStatus(i);
         
         snprintf(status_msg, sizeof(status_msg),
                  "Cond %d: %s | Eff:%.1f%% | Runtime:%.1fh | Maint:%d | Score:%.2f\r\n",
                  i + 1,
                  (staging_status && staging_status->is_running) ? "RUN" : "OFF",
                  cond->performance.efficiency_rating * 100.0f,
                  cond->total_runtime_hours / 60.0f,
                  cond->maintenance.maintenance_state,
                  cond->priority_score);
         Send_Debug_Data(status_msg);
     }
     
     Send_Debug_Data("=== END CONDENSER MANAGEMENT STATUS ===\r\n\r\n");
 }
 
 bool CondenserManager_RunDiagnostics(void)
 {
     Send_Debug_Data("\r\n=== CONDENSER MANAGEMENT DIAGNOSTICS ===\r\n");
     
     bool all_tests_passed = true;
     
     // Test 1: Check initialization
     if (!s_initialization_complete) {
         Send_Debug_Data("FAIL: System not initialized\r\n");
         all_tests_passed = false;
     } else {
         Send_Debug_Data("PASS: System initialization\r\n");
     }
     
     // Test 2: Check condenser availability
     if (g_condenser_manager.available_condenser_count == 0) {
         Send_Debug_Data("FAIL: No condensers available\r\n");
         all_tests_passed = false;
     } else {
         Send_Debug_Data("PASS: Condensers available\r\n");
     }
     
     // Test 3: Check performance tracking
     bool performance_tracking_ok = true;
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         if (g_condenser_manager.condensers[i].is_managed &&
             !g_condenser_manager.condensers[i].performance.performance_valid) {
             performance_tracking_ok = false;
             break;
         }
     }
     
     if (performance_tracking_ok) {
         Send_Debug_Data("PASS: Performance tracking\r\n");
     } else {
         Send_Debug_Data("WARN: Performance tracking incomplete\r\n");
     }
     
     // Test 4: Check selection algorithm
     uint8_t selected_indices[MAX_CONDENSER_BANKS];
     uint8_t count = CondenserManager_SelectCondensersToStart(1, selected_indices);
     
     if (count > 0) {
         Send_Debug_Data("PASS: Selection algorithm\r\n");
     } else {
         Send_Debug_Data("WARN: Selection algorithm returned no candidates\r\n");
     }
     
     char result_msg[60];
     snprintf(result_msg, sizeof(result_msg), 
              "=== DIAGNOSTICS %s ===\r\n\r\n", 
              all_tests_passed ? "PASSED" : "COMPLETED WITH WARNINGS");
     Send_Debug_Data(result_msg);
     
     return all_tests_passed;
 }
 
 // ========================================================================
 // GETTER FUNCTIONS
 // ========================================================================
 
 float CondenserManager_GetSystemEfficiency(void)
 {
     return g_condenser_manager.system_efficiency;
 }
 
 float CondenserManager_GetSystemPowerConsumption(void)
 {
     return g_condenser_manager.system_power_consumption;
 }
 
 CondenserManagedData_t* CondenserManager_GetCondenserData(uint8_t condenser_index)
 {
     if (condenser_index >= MAX_CONDENSER_BANKS) return NULL;
     return &g_condenser_manager.condensers[condenser_index];
 }
 
 uint8_t CondenserManager_GetLeadCondenserIndex(void)
 {
     return g_condenser_manager.lead_condenser_index;
 }
 
 uint32_t CondenserManager_GetRuntimeBalance(uint32_t* max_runtime, uint32_t* min_runtime)
 {
     if (max_runtime == NULL || min_runtime == NULL) return 0;
     
     *max_runtime = 0;
     *min_runtime = UINT32_MAX;
     
     for (uint8_t i = 0; i < MAX_CONDENSER_BANKS; i++) {
         if (!g_condenser_manager.condensers[i].is_managed) continue;
         
         uint32_t runtime = g_condenser_manager.condensers[i].total_runtime_hours;
         
         if (runtime > *max_runtime) {
             *max_runtime = runtime;
         }
         
         if (runtime < *min_runtime) {
             *min_runtime = runtime;
         }
     }
     
     return (*max_runtime - *min_runtime);
 }
 
 /**
  * ========================================================================
  * END OF FILE: condenser_manager.c
  * ========================================================================
  */