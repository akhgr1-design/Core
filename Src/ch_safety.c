/**
 * ========================================================================
 * CHILLER SAFETY SYSTEM - IMPLEMENTATION FILE
 * ========================================================================
 * 
 * File: ch_safety.c
 * Author: Chiller Control System
 * Version: 1.0
 * Date: September 2025
 * 
 * SAFETY SYSTEM IMPLEMENTATION:
 * Complete safety monitoring and protection system for industrial chiller
 * with hot climate optimization (38°C baseline ambient temperature).
 * 
 * FEATURES:
 * - Multi-level alarm system with automatic response
 * - Temperature protection with hot climate adaptation
 * - Pressure monitoring with ambient compensation  
 * - Digital input safety interlocks
 * - Emergency stop and system shutdown logic
 * - Comprehensive fault logging and history
 * - HMI integration for operator interface
 * - Debug commands for system testing
 * 
 * ========================================================================
 */

 #include "ch_safety.h"
 #include "hmi.h"
 #include "usart.h"
 #include <stdio.h>
 #include <string.h>
 #include <stdlib.h>
 #include <math.h>
 
 // ========================================================================
 // GLOBAL VARIABLES
 // ========================================================================
 
 Safety_SystemData_t safety_system;
 Safety_Config_t safety_config;
 
 // Private variables
 static bool safety_initialized = false;
 static uint32_t safety_uptime_start = 0;
 static char debug_buffer[256];
 
 // Alarm delay tracking
 static uint32_t alarm_delay_timers[SAFETY_ALARM_COUNT];
 static bool alarm_delay_active[SAFETY_ALARM_COUNT];
 
 // ========================================================================
 // PRIVATE FUNCTION PROTOTYPES
 // ========================================================================
 
 static void Safety_InitializeAlarmSystem(void);
 static void Safety_ProcessAlarmDelays(void);
 static void Safety_AddAlarmToHistory(SafetyAlarm_t* alarm);
 static void Safety_UpdateSystemState(void);
 static float Safety_GetSensorValue(uint8_t sensor_id, bool* is_valid);
 static void Safety_LogEvent(const char* event, SafetyLevel_t level);
 static void Safety_SendDebugMessage(const char* message);
 
 // ========================================================================
 // INITIALIZATION FUNCTIONS
 // ========================================================================
 
 /**
  * @brief Initialize the safety system
  * @return true if successful, false if error
  */
 bool Safety_Init(void)
 {
     // Clear all data structures
     memset(&safety_system, 0, sizeof(Safety_SystemData_t));
     memset(&safety_config, 0, sizeof(Safety_Config_t));
     memset(alarm_delay_timers, 0, sizeof(alarm_delay_timers));
     memset(alarm_delay_active, 0, sizeof(alarm_delay_active));
     
     // Set default configuration
     Safety_SetDefaultConfiguration();
     
     // Try to load configuration from flash
     if (!Safety_LoadConfiguration()) {
         Safety_SendDebugMessage("Safety: Using default configuration");
     }
     
     // Initialize alarm system
     Safety_InitializeAlarmSystem();
     
     // Set initial system state
     safety_system.system_state = SAFETY_STATE_NORMAL;
     safety_uptime_start = HAL_GetTick();
     
     // Initialize timing
     safety_system.last_fast_check = HAL_GetTick();
     safety_system.last_normal_check = HAL_GetTick();
     safety_system.last_slow_check = HAL_GetTick();
     
     safety_initialized = true;
     
     Safety_SendDebugMessage("Safety System: Initialized successfully");
     Safety_LogEvent("Safety system started", SAFETY_LEVEL_INFO);
     
     return true;
 }
 
 /**
  * @brief Load safety configuration from flash memory
  */
 bool Safety_LoadConfiguration(void)
 {
     // Try to load from flash using flash_config system
     if (FlashConfig_ReadConfigData("safety_config", &safety_config, sizeof(Safety_Config_t))) {
         Safety_SendDebugMessage("Safety: Configuration loaded from flash");
         return true;
     }
     return false;
 }
 
 /**
  * @brief Set default safety configuration
  */
 void Safety_SetDefaultConfiguration(void)
 {
     // Temperature limits (hot climate optimized)
     safety_config.compressor_temp_alarm_limit = SAFETY_COMPRESSOR_TEMP_ALARM;
     safety_config.compressor_temp_trip_limit = SAFETY_COMPRESSOR_TEMP_TRIP;
     safety_config.oil_temp_alarm_limit = SAFETY_OIL_TEMP_ALARM;
     safety_config.oil_temp_trip_limit = SAFETY_OIL_TEMP_TRIP;
     
     // Pressure limits (will be adapted for ambient)
     safety_config.high_pressure_alarm_limit = SAFETY_HIGH_PRESSURE_ALARM;
     safety_config.high_pressure_trip_limit = SAFETY_HIGH_PRESSURE_TRIP;
     safety_config.low_pressure_alarm_limit = SAFETY_LOW_PRESSURE_ALARM;
     safety_config.low_pressure_trip_limit = SAFETY_LOW_PRESSURE_TRIP;
     
     // Timing settings
     safety_config.alarm_delay_ms = SAFETY_ALARM_DELAY_MS;
     safety_config.trip_delay_ms = SAFETY_TRIP_DELAY_MS;
     safety_config.lockout_time_ms = SAFETY_LOCKOUT_TIME_MS;
     
     // Enable all protection features by default
     safety_config.temperature_protection_enable = true;
     safety_config.pressure_protection_enable = true;
     safety_config.digital_input_monitoring_enable = true;
     safety_config.automatic_reset_enable = false; // Manual reset for safety
 }
 
 /**
  * @brief Initialize the alarm system
  */
 static void Safety_InitializeAlarmSystem(void)
 {
     // Clear all active alarms
     safety_system.active_alarm_count = 0;
     memset(safety_system.active_alarms, 0, sizeof(safety_system.active_alarms));
     
     // Initialize alarm history
     safety_system.alarm_history_index = 0;
     memset(safety_system.alarm_history, 0, sizeof(safety_system.alarm_history));
     
     // Clear alarm delay timers
     memset(alarm_delay_timers, 0, sizeof(alarm_delay_timers));
     memset(alarm_delay_active, false, sizeof(alarm_delay_active));
 }
 
 // ========================================================================
 // MAIN PROCESSING FUNCTIONS
 // ========================================================================
 
 /**
  * @brief Main safety system processing function
  * Call this from your main loop every 100ms or faster
  */
 void Safety_Process(void)
 {
     if (!safety_initialized) {
         return;
     }
     
     uint32_t current_time = HAL_GetTick();
     
     // Process alarm delays
     Safety_ProcessAlarmDelays();
     
     // Fast checks (critical safety - every 100ms)
     if (current_time - safety_system.last_fast_check >= SAFETY_FAST_CHECK_INTERVAL) {
         Safety_ProcessFastChecks();
         safety_system.last_fast_check = current_time;
     }
     
     // Normal checks (standard monitoring - every 1 second)
     if (current_time - safety_system.last_normal_check >= SAFETY_NORMAL_CHECK_INTERVAL) {
         Safety_ProcessNormalChecks();
         safety_system.last_normal_check = current_time;
     }
     
     // Slow checks (system health - every 5 seconds)
     if (current_time - safety_system.last_slow_check >= SAFETY_SLOW_CHECK_INTERVAL) {
         Safety_ProcessSlowChecks();
         safety_system.last_slow_check = current_time;
     }
     
     // Update overall system state
     Safety_UpdateSystemState();
     
     // Update HMI with current status
     Safety_UpdateHMI();
 }
 
 /**
  * @brief Fast safety checks (100ms interval) - Critical safety functions
  */
 void Safety_ProcessFastChecks(void)
 {
     // Emergency stop check (highest priority)
     if (safety_config.digital_input_monitoring_enable) {
         if (!Safety_CheckEmergencyStop()) {
             Safety_EmergencyStop("Emergency stop button pressed");
             return;
         }
     }
     
     // Critical temperature checks
     if (safety_config.temperature_protection_enable) {
         for (uint8_t i = 0; i < SAFETY_MAX_COMPRESSORS; i++) {
             if (EquipmentConfig_IsCompressorInstalled(i)) {
                 // Check for trip-level temperatures
                 bool temp_valid = false;
                 float temp = Safety_GetSensorValue(MODBUS_SENSOR_COMP_TEMP_1 + i, &temp_valid);
                 
                 if (temp_valid && temp > safety_config.compressor_temp_trip_limit) {
                     snprintf(debug_buffer, sizeof(debug_buffer), 
                             "Compressor %d temperature trip: %.1f°C", i + 1, temp);
                     Safety_SystemShutdown(debug_buffer);
                     return;
                 }
             }
         }
     }
     
     // Critical pressure checks
     if (safety_config.pressure_protection_enable) {
         if (!Safety_CheckHighPressure()) {
             Safety_SystemShutdown("High pressure trip");
             return;
         }
     }
 }
 
 /**
  * @brief Normal safety checks (1 second interval)
  */
 void Safety_ProcessNormalChecks(void)
 {
     // Temperature monitoring
     if (safety_config.temperature_protection_enable) {
         Safety_CheckTemperatures();
     }
     
     // Pressure monitoring
     if (safety_config.pressure_protection_enable) {
         Safety_CheckPressures();
     }
     
     // Digital input monitoring
     if (safety_config.digital_input_monitoring_enable) {
         Safety_CheckDigitalInputs();
     }
 }
 
 /**
  * @brief Slow safety checks (5 second interval)
  */
 void Safety_ProcessSlowChecks(void)
 {
     // Adapt pressure limits based on ambient temperature
     bool ambient_valid = false;
     float ambient_temp = Safety_GetSensorValue(MODBUS_SENSOR_AMBIENT_TEMP, &ambient_valid);
     
     if (ambient_valid) {
         Safety_AdaptPressureLimitsForAmbient(ambient_temp);
         safety_system.ambient_temp = ambient_temp;
         
         // Check for extreme ambient conditions
         if (ambient_temp > SAFETY_AMBIENT_CRITICAL) {
             Safety_SetAlarm(SAFETY_ALARM_HIGH_AMBIENT_TEMP, SAFETY_LEVEL_CRITICAL,
                           "Critical ambient temperature - system protection active");
         }
     }
     
     // Check for lockout expiration
     if (Safety_IsSystemLocked() && HAL_GetTick() > safety_system.lockout_end_time) {
         Safety_SendDebugMessage("Safety: Lockout period expired");
         Safety_LogEvent("Safety lockout expired", SAFETY_LEVEL_INFO);
     }
 }
 
 // ========================================================================
 // TEMPERATURE MONITORING
 // ========================================================================
 
 /**
  * @brief Check all temperature sensors
  */
 void Safety_CheckTemperatures(void)
 {
     // Check each installed compressor
     for (uint8_t i = 0; i < SAFETY_MAX_COMPRESSORS; i++) {
         if (EquipmentConfig_IsCompressorInstalled(i)) {
             Safety_CheckCompressorTemperature(i);
             Safety_CheckOilTemperature(i);
         }
     }
     
     // Check return water temperature
     Safety_CheckReturnWaterTemperature();
     
     // Check ambient temperature
     Safety_CheckAmbientTemperature();
 }
 
 /**
  * @brief Check individual compressor temperature
  */
 bool Safety_CheckCompressorTemperature(uint8_t compressor_id)
 {
     if (compressor_id >= SAFETY_MAX_COMPRESSORS) return false;
     
     bool temp_valid = false;
     float temp = Safety_GetSensorValue(MODBUS_SENSOR_COMP_TEMP_1 + compressor_id, &temp_valid);
     
     if (!temp_valid) {
         Safety_SetAlarm(SAFETY_ALARM_SENSOR_FAULT_TEMP, SAFETY_LEVEL_WARNING,
                        "Compressor temperature sensor fault");
         return false;
     }
     
     safety_system.compressor_temps[compressor_id] = temp;
     
     // Check for alarm level
     if (temp > safety_config.compressor_temp_alarm_limit) {
         snprintf(debug_buffer, sizeof(debug_buffer),
                 "Compressor %d temperature high: %.1f°C", compressor_id + 1, temp);
         Safety_SetAlarm(SAFETY_ALARM_COMPRESSOR_TEMP_HIGH, SAFETY_LEVEL_ALARM, debug_buffer);
     }
     
     return true;
 }
 
 /**
  * @brief Check oil temperature for specific compressor
  */
 bool Safety_CheckOilTemperature(uint8_t compressor_id)
 {
     if (compressor_id >= SAFETY_MAX_COMPRESSORS) return false;
     
     bool temp_valid = false;
     float temp = Safety_GetSensorValue(MODBUS_SENSOR_OIL_TEMP_1 + compressor_id, &temp_valid);
     
     if (!temp_valid) {
         return false; // Oil temp sensors may not be installed on all units
     }
     
     safety_system.oil_temps[compressor_id] = temp;
     
     // Check for trip level (critical)
     if (temp > safety_config.oil_temp_trip_limit) {
         snprintf(debug_buffer, sizeof(debug_buffer),
                 "Compressor %d oil temperature trip: %.1f°C", compressor_id + 1, temp);
         Safety_SystemShutdown(debug_buffer);
         return false;
     }
     
     // Check for alarm level
     if (temp > safety_config.oil_temp_alarm_limit) {
         snprintf(debug_buffer, sizeof(debug_buffer),
                 "Compressor %d oil temperature high: %.1f°C", compressor_id + 1, temp);
         Safety_SetAlarm(SAFETY_ALARM_COMPRESSOR_OIL_TEMP_HIGH, SAFETY_LEVEL_ALARM, debug_buffer);
     }
     
     return true;
 }
 
 /**
  * @brief Check return water temperature
  */
 bool Safety_CheckReturnWaterTemperature(void)
 {
     bool temp_valid = false;
     float temp = Safety_GetSensorValue(MODBUS_SENSOR_RETURN_WATER_TEMP, &temp_valid);
     
     if (!temp_valid) {
         Safety_SetAlarm(SAFETY_ALARM_SENSOR_FAULT_TEMP, SAFETY_LEVEL_WARNING,
                        "Return water temperature sensor fault");
         return false;
     }
     
     safety_system.return_water_temp = temp;
     
     // Check for high return water temperature (cooling system problem)
     if (temp > SAFETY_RETURN_WATER_MAX) {
         snprintf(debug_buffer, sizeof(debug_buffer),
                 "Return water temperature high: %.1f°C", temp);
         Safety_SetAlarm(SAFETY_ALARM_HIGH_RETURN_WATER_TEMP, SAFETY_LEVEL_ALARM, debug_buffer);
     }
     
     return true;
 }
 
 /**
  * @brief Check ambient temperature
  */
 bool Safety_CheckAmbientTemperature(void)
 {
     bool temp_valid = false;
     float temp = Safety_GetSensorValue(MODBUS_SENSOR_AMBIENT_TEMP, &temp_valid);
     
     if (!temp_valid) {
         return false;
     }
     
     safety_system.ambient_temp = temp;
     
     // Hot climate monitoring - warn at extreme temperatures
     if (temp > SAFETY_AMBIENT_CRITICAL) {
         snprintf(debug_buffer, sizeof(debug_buffer),
                 "Extreme ambient temperature: %.1f°C", temp);
         Safety_SetAlarm(SAFETY_ALARM_HIGH_AMBIENT_TEMP, SAFETY_LEVEL_CRITICAL, debug_buffer);
     }
     
     return true;
 }
 
 // ========================================================================
 // PRESSURE MONITORING
 // ========================================================================
 
 /**
  * @brief Check all pressure parameters
  */
 void Safety_CheckPressures(void)
 {
     Safety_CheckHighPressure();
     Safety_CheckLowPressure();
 }
 
 /**
  * @brief Check high pressure
  */
 bool Safety_CheckHighPressure(void)
 {
     bool pressure_valid = false;
     float pressure = Safety_GetSensorValue(MODBUS_SENSOR_HIGH_PRESSURE, &pressure_valid);
     
     if (!pressure_valid) {
         Safety_SetAlarm(SAFETY_ALARM_SENSOR_FAULT_PRESSURE, SAFETY_LEVEL_WARNING,
                        "High pressure sensor fault");
         return false;
     }
     
     safety_system.high_pressure = pressure;
     
     // Check for trip level
     if (pressure > safety_config.high_pressure_trip_limit) {
         snprintf(debug_buffer, sizeof(debug_buffer),
                 "High pressure trip: %.1f bar", pressure);
         Safety_SystemShutdown(debug_buffer);
         return false;
     }
     
     // Check for alarm level
     if (pressure > safety_config.high_pressure_alarm_limit) {
         snprintf(debug_buffer, sizeof(debug_buffer),
                 "High pressure alarm: %.1f bar", pressure);
         Safety_SetAlarm(SAFETY_ALARM_HIGH_PRESSURE, SAFETY_LEVEL_ALARM, debug_buffer);
     }
     
     return true;
 }
 
 /**
  * @brief Check low pressure
  */
 bool Safety_CheckLowPressure(void)
 {
     bool pressure_valid = false;
     float pressure = Safety_GetSensorValue(MODBUS_SENSOR_LOW_PRESSURE, &pressure_valid);
     
     if (!pressure_valid) {
         return false;
     }
     
     safety_system.low_pressure = pressure;
     
     // Check for trip level
     if (pressure < safety_config.low_pressure_trip_limit) {
         snprintf(debug_buffer, sizeof(debug_buffer),
                 "Low pressure trip: %.1f bar", pressure);
         Safety_SystemShutdown(debug_buffer);
         return false;
     }
     
     // Check for alarm level
     if (pressure < safety_config.low_pressure_alarm_limit) {
         snprintf(debug_buffer, sizeof(debug_buffer),
                 "Low pressure alarm: %.1f bar", pressure);
         Safety_SetAlarm(SAFETY_ALARM_LOW_PRESSURE, SAFETY_LEVEL_ALARM, debug_buffer);
     }
     
     return true;
 }
 
 /**
  * @brief Adapt pressure limits based on ambient temperature
  * Hot climate requires higher pressure limits
  */
 void Safety_AdaptPressureLimitsForAmbient(float ambient_temp)
 {
     float base_high_alarm = SAFETY_HIGH_PRESSURE_ALARM;
     float base_high_trip = SAFETY_HIGH_PRESSURE_TRIP;
     
     // Increase limits for hot ambient conditions
     if (ambient_temp > 35.0f) {
         float temp_factor = 1.0f + ((ambient_temp - 35.0f) * 0.02f); // 2% per degree above 35°C
         safety_config.high_pressure_alarm_limit = base_high_alarm * temp_factor;
         safety_config.high_pressure_trip_limit = base_high_trip * temp_factor;
     } else {
         // Use base limits for normal temperatures
         safety_config.high_pressure_alarm_limit = base_high_alarm;
         safety_config.high_pressure_trip_limit = base_high_trip;
     }
 }
 
 // ========================================================================
 // DIGITAL INPUT MONITORING
 // ========================================================================
 
 /**
  * @brief Check all digital safety inputs
  */
 void Safety_CheckDigitalInputs(void)
 {
     Safety_CheckEmergencyStop();
     Safety_CheckWaterFlow();
     Safety_CheckPhaseMonitor();
     Safety_CheckThermalOverloads();
 }
 
 /**
  * @brief Check emergency stop button
  */
 bool Safety_CheckEmergencyStop(void)
 {
     // Read emergency stop input (normally closed contact)
     bool estop_ok = Input_Read(GPIO_INPUT_EMERGENCY_STOP);
     safety_system.emergency_stop = !estop_ok; // Inverted logic for NC contact
     
     if (safety_system.emergency_stop) {
         Safety_SetAlarm(SAFETY_ALARM_EMERGENCY_STOP, SAFETY_LEVEL_EMERGENCY,
                        "Emergency stop activated");
         return false;
     }
     
     return true;
 }
 
 /**
  * @brief Check water flow switch
  */
 bool Safety_CheckWaterFlow(void)
 {
     bool flow_ok = Input_Read(GPIO_INPUT_WATER_FLOW);
     safety_system.water_flow_ok = flow_ok;
     
     if (!flow_ok) {
         Safety_SetAlarm(SAFETY_ALARM_WATER_FLOW_FAULT, SAFETY_LEVEL_CRITICAL,
                        "Water flow fault detected");
         return false;
     }
     
     return true;
 }
 
 /**
  * @brief Check phase monitor
  */
 bool Safety_CheckPhaseMonitor(void)
 {
     bool phase_ok = Input_Read(GPIO_INPUT_PHASE_MONITOR);
     safety_system.phase_monitor_ok = phase_ok;
     
     if (!phase_ok) {
         Safety_SetAlarm(SAFETY_ALARM_PHASE_LOSS, SAFETY_LEVEL_CRITICAL,
                        "Phase loss or phase sequence fault");
         return false;
     }
     
     return true;
 }
 
 /**
  * @brief Check thermal overload contacts
  */
 bool Safety_CheckThermalOverloads(void)
 {
     bool all_ok = true;
     
     for (uint8_t i = 0; i < SAFETY_MAX_COMPRESSORS; i++) {
         if (EquipmentConfig_IsCompressorInstalled(i)) {
             bool overload_ok = Input_Read(GPIO_INPUT_COMP_OVERLOAD_1 + i);
             safety_system.thermal_overload[i] = !overload_ok; // Inverted for NC contact
             
             if (!overload_ok) {
                 snprintf(debug_buffer, sizeof(debug_buffer),
                         "Compressor %d thermal overload", i + 1);
                 Safety_SetAlarm(SAFETY_ALARM_OVERLOAD, SAFETY_LEVEL_CRITICAL, debug_buffer);
                 all_ok = false;
             }
         }
     }
     
     return all_ok;
 }
 
 // ========================================================================
 // ALARM MANAGEMENT
 // ========================================================================
 
 /**
  * @brief Set an alarm with specified level and description
  */
 bool Safety_SetAlarm(SafetyAlarmType_t alarm_type, SafetyLevel_t level, const char* description)
 {
     if (alarm_type >= SAFETY_ALARM_COUNT) return false;
     
     // Check if alarm already exists
     for (uint8_t i = 0; i < safety_system.active_alarm_count; i++) {
         if (safety_system.active_alarms[i].alarm_id == alarm_type) {
             // Update existing alarm
             safety_system.active_alarms[i].level = level;
             safety_system.active_alarms[i].timestamp = HAL_GetTick();
             strncpy(safety_system.active_alarms[i].description, description, 63);
             safety_system.active_alarms[i].description[63] = '\0';
             return true;
         }
     }
     
     // Add new alarm if space available
     if (safety_system.active_alarm_count < SAFETY_MAX_ALARMS) {
         SafetyAlarm_t* alarm = &safety_system.active_alarms[safety_system.active_alarm_count];
         
         alarm->alarm_id = alarm_type;
         alarm->level = level;
         alarm->timestamp = HAL_GetTick();
         alarm->active = true;
         alarm->acknowledged = false;
         alarm->data = 0;
         strncpy(alarm->description, description, 63);
         alarm->description[63] = '\0';
         
         safety_system.active_alarm_count++;
         
         // Add to history
         Safety_AddAlarmToHistory(alarm);
         
         // Log to flash
         Safety_LogAlarmToFlash(alarm);
         
         // Send debug message
         snprintf(debug_buffer, sizeof(debug_buffer),
                 "ALARM: %s (%s)", description, Safety_GetLevelDescription(level));
         Safety_SendDebugMessage(debug_buffer);
         
         return true;
     }
     
     return false; // No space for more alarms
 }
 
 /**
  * @brief Clear a specific alarm
  */
 bool Safety_ClearAlarm(SafetyAlarmType_t alarm_type)
 {
     for (uint8_t i = 0; i < safety_system.active_alarm_count; i++) {
         if (safety_system.active_alarms[i].alarm_id == alarm_type) {
             // Remove alarm by shifting remaining alarms
             for (uint8_t j = i; j < safety_system.active_alarm_count - 1; j++) {
                 safety_system.active_alarms[j] = safety_system.active_alarms[j + 1];
             }
             safety_system.active_alarm_count--;
             
             Safety_SendDebugMessage("Alarm cleared");
             return true;
         }
     }
     return false;
 }
 
 /**
  * @brief Clear all active alarms
  */
 void Safety_ClearAllAlarms(void)
 {
     safety_system.active_alarm_count = 0;
     memset(safety_system.active_alarms, 0, sizeof(safety_system.active_alarms));
     Safety_SendDebugMessage("All alarms cleared");
 }
 
 /**
  * @brief Add alarm to history buffer
  */
 static void Safety_AddAlarmToHistory(SafetyAlarm_t* alarm)
 {
     // Copy alarm to history buffer
     safety_system.alarm_history[safety_system.alarm_history_index] = *alarm;
     
     // Advance index with wrap-around
     safety_system.alarm_history_index = (safety_system.alarm_history_index + 1) % SAFETY_ALARM_HISTORY_SIZE;
 }
 
 /**
  * @brief Process alarm delays for debouncing
  */
 static void Safety_ProcessAlarmDelays(void)
 {
     uint32_t current_time = HAL_GetTick();
     
     for (uint16_t i = 0; i < SAFETY_ALARM_COUNT; i++) {
         if (alarm_delay_active[i]) {
             if (current_time - alarm_delay_timers[i] >= safety_config.alarm_delay_ms) {
                 alarm_delay_active[i] = false;
                 // Alarm delay expired - alarm would be processed here if needed
             }
         }
     }
 }
 
 // ========================================================================
 // SYSTEM CONTROL FUNCTIONS
 // ========================================================================
 
 /**
  * @brief Emergency stop - immediate system shutdown
  */
 void Safety_EmergencyStop(const char* reason)
 {
     safety_system.system_state = SAFETY_STATE_EMERGENCY;
     safety_system.lockout_end_time = HAL_GetTick() + safety_config.lockout_time_ms;
     
     // Turn off all outputs immediately
     for (uint8_t i = 0; i < SAFETY_MAX_COMPRESSORS; i++) {
         Relay_Set(GPIO_RELAY_COMPRESSOR_1 + i, false);
     }
     for (uint8_t i = 0; i < SAFETY_MAX_CONDENSERS; i++) {
         Relay_Set(GPIO_RELAY_CONDENSER_1 + i, false);
     }
     
     // Set emergency alarm
     Safety_SetAlarm(SAFETY_ALARM_EMERGENCY_STOP, SAFETY_LEVEL_EMERGENCY, reason);
     
     snprintf(debug_buffer, sizeof(debug_buffer), "EMERGENCY STOP: %s", reason);
     Safety_SendDebugMessage(debug_buffer);
     Safety_LogEvent(debug_buffer, SAFETY_LEVEL_EMERGENCY);
 }
 
 /**
  * @brief System shutdown due to safety fault
  */
 void Safety_SystemShutdown(const char* reason)
 {
     safety_system.system_state = SAFETY_STATE_CRITICAL;
     safety_system.lockout_end_time = HAL_GetTick() + safety_config.lockout_time_ms;
     safety_system.trip_count++;
     
     // Controlled shutdown of all equipment
     for (uint8_t i = 0; i < SAFETY_MAX_COMPRESSORS; i++) {
         Relay_Set(GPIO_RELAY_COMPRESSOR_1 + i, false);
     }
     
     // Keep condensers running briefly to cool system
     HAL_Delay(2000);
     for (uint8_t i = 0; i < SAFETY_MAX_CONDENSERS; i++) {
         Relay_Set(GPIO_RELAY_CONDENSER_1 + i, false);
     }
     
     Safety_SetAlarm(SAFETY_ALARM_SYSTEM_FAULT, SAFETY_LEVEL_CRITICAL, reason);
     
     snprintf(debug_buffer, sizeof(debug_buffer), "SYSTEM SHUTDOWN: %s", reason);
     Safety_SendDebugMessage(debug_buffer);
     Safety_LogEvent(debug_buffer, SAFETY_LEVEL_CRITICAL);
 }
 
 /**
  * @brief Reset the safety system
  */
 bool Safety_SystemReset(void)
 {
     if (Safety_IsSystemLocked()) {
         Safety_SendDebugMessage("Cannot reset - system is locked");
         return false;
     }
     
     // Clear alarms and reset state
     Safety_ClearAllAlarms();
     safety_system.system_state = SAFETY_STATE_NORMAL;
     safety_system.fault_count = 0;
     
     Safety_SendDebugMessage("Safety system reset successful");
     Safety_LogEvent("Safety system reset", SAFETY_LEVEL_INFO);
     
     return true;
 }
 
 /**
  * @brief Check if system is in lockout state
  */
 bool Safety_IsSystemLocked(void)
 {
     return (HAL_GetTick() < safety_system.lockout_end_time);
 }
 
 /**
  * @brief Check if a compressor can be started
  */
 bool Safety_CanStartCompressor(uint8_t compressor_id)
 {
     if (compressor_id >= SAFETY_MAX_COMPRESSORS) return false;
     
     // Check overall system state
     if (safety_system.system_state > SAFETY_STATE_ALARM) return false;
     
     // Check if system is locked
     if (Safety_IsSystemLocked()) return false;
     
     // Check specific compressor conditions
     if (safety_system.compressor_temps[compressor_id] > safety_config.compressor_temp_alarm_limit) {
         return false;
     }
     
     if (safety_system.thermal_overload[compressor_id]) return false;
     
     return true;
 }
 
 // ========================================================================
 // UTILITY AND DATA ACCESS FUNCTIONS
 // ========================================================================
 
 /**
  * @brief Update overall system state based on active alarms
  */
 static void Safety_UpdateSystemState(void)
 {
     SafetyState_t highest_state = SAFETY_STATE_NORMAL;
     
     // Find highest alarm level
     for (uint8_t i = 0; i < safety_system.active_alarm_count; i++) {
         SafetyAlarm_t* alarm = &safety_system.active_alarms[i];
         
         if (alarm->level == SAFETY_LEVEL_EMERGENCY) {
             highest_state = SAFETY_STATE_EMERGENCY;
             break; // Emergency is highest possible
         } else if (alarm->level == SAFETY_LEVEL_CRITICAL && highest_state < SAFETY_STATE_CRITICAL) {
             highest_state = SAFETY_STATE_CRITICAL;
         } else if (alarm->level == SAFETY_LEVEL_ALARM && highest_state < SAFETY_STATE_ALARM) {
             highest_state = SAFETY_STATE_ALARM;
         } else if (alarm->level == SAFETY_LEVEL_WARNING && highest_state < SAFETY_STATE_WARNING) {
             highest_state = SAFETY_STATE_WARNING;
         }
     }
     
     // Update system state if not in lockout
     if (!Safety_IsSystemLocked() || highest_state == SAFETY_STATE_EMERGENCY) {
         safety_system.system_state = highest_state;
     }
 }
 
 /**
  * @brief Get sensor value with validity check
  */
 static float Safety_GetSensorValue(uint8_t sensor_id, bool* is_valid)
 {
     // Use modbus sensor system to read values
     ModbusSensorData_t sensor_data;
     if (ModbusSensor_GetSensorData(sensor_id, &sensor_data)) {
         *is_valid = sensor_data.valid;
         return sensor_data.value;
     }
     
     *is_valid = false;
     return 0.0f;
 }
 
 /**
  * @brief Get current system state
  */
 SafetyState_t Safety_GetSystemState(void)
 {
     return safety_system.system_state;
 }
 
 /**
  * @brief Get active alarm count
  */
 uint8_t Safety_GetActiveAlarmCount(void)
 {
     return safety_system.active_alarm_count;
 }
 
 /**
  * @brief Get status of specific alarm
  */
 bool Safety_GetAlarmStatus(SafetyAlarmType_t alarm_type)
 {
     for (uint8_t i = 0; i < safety_system.active_alarm_count; i++) {
         if (safety_system.active_alarms[i].alarm_id == alarm_type) {
             return true;
         }
     }
     return false;
 }
 
 // ========================================================================
 // HMI INTEGRATION
 // ========================================================================
 
 /**
  * @brief Update HMI with safety data
  */
 void Safety_UpdateHMI(void)
 {
     // Update system status VP registers
     HMI_Write_VP_Register(VP_SAFETY_SYSTEM_STATE, (uint16_t)safety_system.system_state);
     HMI_Write_VP_Register(VP_SAFETY_ACTIVE_ALARMS, safety_system.active_alarm_count);
     HMI_Write_VP_Register(VP_SAFETY_FAULT_COUNT, (uint16_t)safety_system.fault_count);
     
     // Update temperature readings (scaled by 10 for decimal places)
     HMI_Write_VP_Register(VP_SAFETY_RETURN_WATER_TEMP, (uint16_t)(safety_system.return_water_temp * 10));
     HMI_Write_VP_Register(VP_SAFETY_AMBIENT_TEMP, (uint16_t)(safety_system.ambient_temp * 10));
     
     // Update pressure readings
     HMI_Write_VP_Register(VP_SAFETY_HIGH_PRESSURE, (uint16_t)(safety_system.high_pressure * 10));
     HMI_Write_VP_Register(VP_SAFETY_LOW_PRESSURE, (uint16_t)(safety_system.low_pressure * 10));
     
     // Update digital input status (bit-packed)
     uint16_t digital_status = 0;
     if (!safety_system.emergency_stop) digital_status |= 0x01;
     if (safety_system.water_flow_ok) digital_status |= 0x02;
     if (safety_system.phase_monitor_ok) digital_status |= 0x04;
     
     HMI_Write_VP_Register(VP_SAFETY_DIGITAL_INPUTS, digital_status);
 }
 
 // ========================================================================
 // DEBUG AND DIAGNOSTIC FUNCTIONS
 // ========================================================================
 
 /**
  * @brief Print system status via debug interface
  */
 void Safety_PrintSystemStatus(void)
 {
     Safety_SendDebugMessage("=== SAFETY SYSTEM STATUS ===");
     
     snprintf(debug_buffer, sizeof(debug_buffer), "State: %s", 
              Safety_GetStateDescription(safety_system.system_state));
     Safety_SendDebugMessage(debug_buffer);
     
     snprintf(debug_buffer, sizeof(debug_buffer), "Active Alarms: %d", 
              safety_system.active_alarm_count);
     Safety_SendDebugMessage(debug_buffer);
     
     snprintf(debug_buffer, sizeof(debug_buffer), "Uptime: %lu seconds", 
              Safety_GetUptimeSeconds());
     Safety_SendDebugMessage(debug_buffer);
     
     snprintf(debug_buffer, sizeof(debug_buffer), "Return Water: %.1f°C", 
              safety_system.return_water_temp);
     Safety_SendDebugMessage(debug_buffer);
     
     snprintf(debug_buffer, sizeof(debug_buffer), "Ambient: %.1f°C", 
              safety_system.ambient_temp);
     Safety_SendDebugMessage(debug_buffer);
     
     snprintf(debug_buffer, sizeof(debug_buffer), "Pressures: %.1f/%.1f bar", 
              safety_system.high_pressure, safety_system.low_pressure);
     Safety_SendDebugMessage(debug_buffer);
 }
 
 /**
  * @brief Print active alarms
  */
 void Safety_PrintActiveAlarms(void)
 {
     Safety_SendDebugMessage("=== ACTIVE ALARMS ===");
     
     if (safety_system.active_alarm_count == 0) {
         Safety_SendDebugMessage("No active alarms");
         return;
     }
     
     for (uint8_t i = 0; i < safety_system.active_alarm_count; i++) {
         SafetyAlarm_t* alarm = &safety_system.active_alarms[i];
         
         snprintf(debug_buffer, sizeof(debug_buffer), 
                 "%d. [%s] %s", i + 1, 
                 Safety_GetLevelDescription(alarm->level),
                 alarm->description);
         Safety_SendDebugMessage(debug_buffer);
     }
 }
 
 /**
  * @brief Run safety system diagnostics
  */
 void Safety_RunDiagnostics(void)
 {
     Safety_SendDebugMessage("=== SAFETY DIAGNOSTICS ===");
     
     // Test all safety inputs
     Safety_SendDebugMessage("Testing digital inputs...");
     Safety_CheckDigitalInputs();
     
     // Test sensor readings
     Safety_SendDebugMessage("Testing sensors...");
     Safety_CheckTemperatures();
     Safety_CheckPressures();
     
     Safety_SendDebugMessage("Diagnostics complete");
 }
 
 // ========================================================================
 // DEBUG COMMAND HANDLERS
 // ========================================================================
 
 /**
  * @brief Debug command: safety_status
  */
 void Safety_Debug_Status(void)
 {
     Safety_PrintSystemStatus();
 }
 
 /**
  * @brief Debug command: safety_alarms
  */
 void Safety_Debug_Alarms(void)
 {
     Safety_PrintActiveAlarms();
 }
 
 /**
  * @brief Debug command: safety_test
  */
 void Safety_Debug_Test(void)
 {
     Safety_RunDiagnostics();
 }
 
 /**
  * @brief Debug command: safety_reset
  */
 void Safety_Debug_Reset(void)
 {
     if (Safety_SystemReset()) {
         Safety_SendDebugMessage("Safety system reset successful");
     } else {
         Safety_SendDebugMessage("Safety system reset failed");
     }
 }
 
 // ========================================================================
 // UTILITY FUNCTIONS
 // ========================================================================
 
 /**
  * @brief Send debug message
  */
 static void Safety_SendDebugMessage(const char* message)
 {
     Send_Debug_Data((char*)message);
     Send_Debug_Data("\r\n");
 }
 
 /**
  * @brief Log event to system
  */
 static void Safety_LogEvent(const char* event, SafetyLevel_t level)
 {
     // Log to flash storage system
     FlashConfig_LogEvent("SAFETY", event, (uint8_t)level);
 }
 
 /**
  * @brief Log alarm to flash storage
  */
 void Safety_LogAlarmToFlash(SafetyAlarm_t* alarm)
 {
     char log_entry[128];
     snprintf(log_entry, sizeof(log_entry), "ALARM_%d:%s", 
              (int)alarm->alarm_id, alarm->description);
     
     FlashConfig_LogEvent("ALARM", log_entry, (uint8_t)alarm->level);
 }
 
 /**
  * @brief Get system uptime in seconds
  */
 uint32_t Safety_GetUptimeSeconds(void)
 {
     return (HAL_GetTick() - safety_uptime_start) / 1000;
 }
 
 /**
  * @brief Get alarm description string
  */
 const char* Safety_GetAlarmDescription(SafetyAlarmType_t alarm_type)
 {
     switch (alarm_type) {
         case SAFETY_ALARM_EMERGENCY_STOP: return "Emergency Stop";
         case SAFETY_ALARM_SYSTEM_FAULT: return "System Fault";
         case SAFETY_ALARM_HIGH_PRESSURE: return "High Pressure";
         case SAFETY_ALARM_LOW_PRESSURE: return "Low Pressure";
         case SAFETY_ALARM_COMPRESSOR_TEMP_HIGH: return "Compressor Temperature High";
         case SAFETY_ALARM_HIGH_AMBIENT_TEMP: return "High Ambient Temperature";
         default: return "Unknown Alarm";
     }
 }
 
 /**
  * @brief Get safety level description string
  */
 const char* Safety_GetLevelDescription(SafetyLevel_t level)
 {
     switch (level) {
         case SAFETY_LEVEL_INFO: return "INFO";
         case SAFETY_LEVEL_WARNING: return "WARNING";
         case SAFETY_LEVEL_ALARM: return "ALARM";
         case SAFETY_LEVEL_CRITICAL: return "CRITICAL";
         case SAFETY_LEVEL_EMERGENCY: return "EMERGENCY";
         default: return "UNKNOWN";
     }
 }
 
 /**
  * @brief Get safety state description string
  */
 const char* Safety_GetStateDescription(SafetyState_t state)
 {
     switch (state) {
         case SAFETY_STATE_NORMAL: return "NORMAL";
         case SAFETY_STATE_WARNING: return "WARNING";
         case SAFETY_STATE_ALARM: return "ALARM";
         case SAFETY_STATE_CRITICAL: return "CRITICAL";
         case SAFETY_STATE_EMERGENCY: return "EMERGENCY";
         case SAFETY_STATE_LOCKOUT: return "LOCKOUT";
         default: return "UNKNOWN";
     }
 }
 
 /**
  * ========================================================================
  * END OF SAFETY SYSTEM IMPLEMENTATION
  * ========================================================================
  */
