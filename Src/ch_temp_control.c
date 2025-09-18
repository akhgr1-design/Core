/**
 * ========================================================================
 * CHILLER TEMPERATURE CONTROL SYSTEM - IMPLEMENTATION FILE
 * ========================================================================
 * 
 * File: ch_temp_control.c
 * Author: Chiller Control System
 * Version: 1.0
 * Date: September 2025
 * 
 * TEMPERATURE CONTROL IMPLEMENTATION:
 * Advanced PID temperature control system with hot climate optimization
 * for industrial chiller applications. Features sensor fusion, fault
 * tolerance, and adaptive control for 38°C ambient baseline operation.
 * 
 * FEATURES:
 * - PID temperature control with hot climate tuning
 * - Multi-sensor monitoring and validation (A0-A3)
 * - Ambient temperature compensation
 * - Efficiency monitoring and optimization
 * - Comprehensive fault detection and recovery
 * - Historical data logging and analysis
 * - HMI integration for operator interface
 * - Debug commands for system testing
 * 
 * ========================================================================
 */

 #include "ch_temp_control.h"
 #include "hmi.h"
 #include "usart.h"
 #include <stdio.h>
 #include <string.h>
 #include <stdlib.h>
 
 // ========================================================================
 // GLOBAL VARIABLES
 // ========================================================================
 
 TempControlData_t temp_control_data;
 TempControlConfig_t temp_control_config;
 
 // Private variables
 static bool temp_control_initialized = false;
 static uint32_t temp_control_uptime_start = 0;
 static char debug_buffer[256];
 static bool temp_control_debug_enabled = false;
 
 // ========================================================================
 // PRIVATE FUNCTION PROTOTYPES
 // ========================================================================
 
 static void TempControl_InitializeSensors(void);
 static void TempControl_ProcessAmbientCompensation(void);
 static void TempControl_LogEvent(const char* event);
 static void TempControl_SendDebugMessage(const char* message);
 static bool TempControl_ReadAnalogSensor(uint8_t channel, float* temperature);
 
 // ========================================================================
 // INITIALIZATION FUNCTIONS
 // ========================================================================
 
 /**
  * @brief Initialize the temperature control system
  * @return true if successful, false if error
  */
 bool TempControl_Init(void)
 {
     // Clear all data structures
     memset(&temp_control_data, 0, sizeof(TempControlData_t));
     memset(&temp_control_config, 0, sizeof(TempControlConfig_t));
     
     // Set default configuration
     TempControl_SetDefaultConfiguration();
     
     // Try to load configuration from flash
     if (!TempControl_LoadConfiguration()) {
         TempControl_SendDebugMessage("TempControl: Using default configuration");
     }
     
     // Initialize sensors
     TempControl_InitializeSensors();
     
     // Initialize PID controller
     TempControl_PID_Init();
     
     // Set initial state
     temp_control_data.system_state = TEMP_CONTROL_STATE_NORMAL;
     temp_control_data.control_mode = temp_control_config.control_mode;
     temp_control_uptime_start = HAL_GetTick();
     
     // Initialize timing
     temp_control_data.last_sample_time = HAL_GetTick();
     temp_control_data.last_pid_update = HAL_GetTick();
     temp_control_data.last_hmi_update = HAL_GetTick();
     
     temp_control_initialized = true;
     
     TempControl_SendDebugMessage("Temperature Control System: Initialized successfully");
     TempControl_LogEvent("Temperature control system started");
     
     return true;
 }
 
 /**
  * @brief Load temperature control configuration from flash
  */
 bool TempControl_LoadConfiguration(void)
 {
     if (FlashConfig_ReadConfigData("temp_config", &temp_control_config, sizeof(TempControlConfig_t))) {
         TempControl_SendDebugMessage("TempControl: Configuration loaded from flash");
         return true;
     }
     return false;
 }
 
 /**
  * @brief Set default temperature control configuration
  */
 void TempControl_SetDefaultConfiguration(void)
 {
     // Setpoint configuration
     temp_control_config.return_water_setpoint = TEMP_SETPOINT_DEFAULT;
     temp_control_config.return_water_deadband = TEMP_DEADBAND_DEFAULT;
     
     // Hot climate parameters
     temp_control_config.ambient_baseline = TEMP_AMBIENT_BASELINE;
     temp_control_config.compensation_factor = 1.0f;
     temp_control_config.auto_compensation_enable = true;
     
     // Control timing
     temp_control_config.sample_rate_ms = TEMP_CONTROL_SAMPLE_RATE_MS;
     temp_control_config.pid_rate_ms = TEMP_CONTROL_PID_RATE_MS;
     temp_control_config.fault_timeout_ms = TEMP_SENSOR_TIMEOUT_MS;
     
     // Efficiency monitoring
     temp_control_config.efficiency_threshold = TEMP_EFFICIENCY_THRESHOLD;
     temp_control_config.efficiency_monitoring_enable = true;
     
     // Control mode
     temp_control_config.control_mode = TEMP_CONTROL_MODE_AUTO;
     temp_control_config.manual_override_enable = false;
     temp_control_config.manual_output = 0.0f;
 }
 
 /**
  * @brief Initialize sensor data structures
  */
 static void TempControl_InitializeSensors(void)
 {
     for (uint8_t i = 0; i < TEMP_CONTROL_MAX_SENSORS; i++) {
         temp_control_data.sensors[i].value = 0.0f;
         temp_control_data.sensors[i].valid = false;
         temp_control_data.sensors[i].last_update = 0;
         temp_control_data.sensors[i].fault_count = 0;
         temp_control_data.sensors[i].min_value = 999.0f;
         temp_control_data.sensors[i].max_value = -999.0f;
         temp_control_data.sensors[i].average = 0.0f;
     }
     
     // Initialize history arrays
     memset(temp_control_data.return_temp_history, 0, sizeof(temp_control_data.return_temp_history));
     memset(temp_control_data.efficiency_history, 0, sizeof(temp_control_data.efficiency_history));
     temp_control_data.history_index = 0;
 }
 
 // ========================================================================
 // MAIN PROCESSING FUNCTIONS
 // ========================================================================
 
 /**
  * @brief Main temperature control processing function
  * Call this from your main loop every 100ms or faster
  */
 void TempControl_Process(void)
 {
     if (!temp_control_initialized) {
         return;
     }
     
     uint32_t current_time = HAL_GetTick();
     
     // Update uptime
     temp_control_data.uptime_seconds = (current_time - temp_control_uptime_start) / 1000;
     
     // Process sensors at configured sample rate
     if (current_time - temp_control_data.last_sample_time >= temp_control_config.sample_rate_ms) {
         TempControl_ProcessSensors();
         temp_control_data.last_sample_time = current_time;
     }
     
     // Process PID control at configured rate
     if (current_time - temp_control_data.last_pid_update >= temp_control_config.pid_rate_ms) {
         TempControl_ProcessPID();
         temp_control_data.last_pid_update = current_time;
     }
     
     // Process fault detection
     TempControl_ProcessFaultDetection();
     
     // Apply hot climate compensation
     if (temp_control_config.auto_compensation_enable) {
         TempControl_ProcessAmbientCompensation();
     }
     
     // Update performance metrics
     TempControl_UpdatePerformanceMetrics();
     
     // Update HMI every 2 seconds
     if (current_time - temp_control_data.last_hmi_update >= 2000) {
         TempControl_UpdateHMI();
         temp_control_data.last_hmi_update = current_time;
     }
 }
 
 /**
  * @brief Process all temperature sensors
  */
 void TempControl_ProcessSensors(void)
 {
     for (uint8_t i = 0; i < TEMP_CONTROL_MAX_SENSORS; i++) {
         float temperature;
         
         if (TempControl_ReadSensor(i, &temperature)) {
             if (TempControl_ValidateSensorReading(i, temperature)) {
                 temp_control_data.sensors[i].value = temperature;
                 temp_control_data.sensors[i].valid = true;
                 temp_control_data.sensors[i].last_update = HAL_GetTick();
                 
                 // Update statistics
                 TempControl_UpdateSensorStatistics(i, temperature);
             } else {
                 temp_control_data.sensors[i].valid = false;
                 temp_control_data.sensors[i].fault_count++;
             }
         } else {
             temp_control_data.sensors[i].valid = false;
             temp_control_data.sensors[i].fault_count++;
         }
     }
 }
 
 /**
  * @brief Process PID control algorithm
  */
 void TempControl_ProcessPID(void)
 {
     // Only run PID if in automatic mode and return water sensor is valid
     if (!TEMP_CONTROL_IS_PID_READY()) {
         return;
     }
     
     float return_water_temp = temp_control_data.sensors[TEMP_SENSOR_RETURN_WATER].value;
     float current_setpoint = temp_control_data.pid.setpoint;
     
     // Apply ambient compensation to setpoint if enabled
     if (temp_control_config.auto_compensation_enable) {
         current_setpoint += temp_control_data.ambient_compensation_active;
     }
     
     // Calculate PID output
     float pid_output = TempControl_PID_Calculate(current_setpoint, return_water_temp);
     
     // Store PID output
     temp_control_data.pid.output = pid_output;
     
     // Update error statistics
     temp_control_data.pid.error_current = current_setpoint - return_water_temp;
     
     // Check for PID saturation
     if (pid_output >= temp_control_data.pid.output_max - 1.0f) {
         TempControl_SetFault(TEMP_FAULT_PID_SATURATED, "PID output saturated at maximum");
     }
 }
 
 /**
  * @brief Process fault detection for all sensors and systems
  */
 void TempControl_ProcessFaultDetection(void)
 {
     // Check sensor timeouts
     for (uint8_t i = 0; i < TEMP_CONTROL_MAX_SENSORS; i++) {
         if (TEMP_CONTROL_IS_SENSOR_TIMEOUT(i)) {
             char fault_msg[64];
             snprintf(fault_msg, sizeof(fault_msg), "Sensor %d timeout", i);
             
             switch (i) {
                 case TEMP_SENSOR_RETURN_WATER:
                     TempControl_SetFault(TEMP_FAULT_SENSOR_RETURN_WATER, fault_msg);
                     break;
                 case TEMP_SENSOR_SUPPLY_WATER:
                     TempControl_SetFault(TEMP_FAULT_SENSOR_SUPPLY_WATER, fault_msg);
                     break;
                 case TEMP_SENSOR_AMBIENT:
                     TempControl_SetFault(TEMP_FAULT_SENSOR_AMBIENT, fault_msg);
                     break;
             }
         }
     }
     
     // Check setpoint deviation
     if (TempControl_IsSensorValid(TEMP_SENSOR_RETURN_WATER)) {
         float error = fabs(temp_control_data.pid.error_current);
         if (error > 3.0f) { // More than 3°C deviation
             TempControl_SetFault(TEMP_FAULT_SETPOINT_DEVIATION, "Large setpoint deviation detected");
         }
     }
     
     // Check cooling efficiency
     if (temp_control_config.efficiency_monitoring_enable) {
         float efficiency = TempControl_CalculateEfficiency();
         if (efficiency < temp_control_config.efficiency_threshold) {
             TempControl_SetFault(TEMP_FAULT_COOLING_EFFICIENCY, "Low cooling efficiency detected");
         }
     }
 }
 
 // ========================================================================
 // SENSOR MANAGEMENT
 // ========================================================================
 
 /**
  * @brief Read temperature from specified sensor
  * @param sensor_id Sensor channel (0-3 for A0-A3)
  * @param temperature Pointer to store temperature value
  * @return true if successful, false if error
  */
 bool TempControl_ReadSensor(uint8_t sensor_id, float* temperature)
 {
     if (sensor_id >= TEMP_CONTROL_MAX_SENSORS || temperature == NULL) {
         return false;
     }
     
     // Read from analog sensor using modbus system
     return TempControl_ReadAnalogSensor(sensor_id, temperature);
 }
 
 /**
  * @brief Read analog sensor value
  * @param channel Analog channel (0=A0, 1=A1, 2=A2, 3=A3)
  * @param temperature Pointer to store temperature value
  * @return true if successful, false if error
  */
 static bool TempControl_ReadAnalogSensor(uint8_t channel, float* temperature)
 {
     // Use modbus sensor system to read analog values
     ModbusSensorData_t sensor_data;
     
     if (ModbusSensor_GetSensorData(channel, &sensor_data)) {
         if (sensor_data.valid) {
             *temperature = sensor_data.value;
             return true;
         }
     }
     
     return false;
 }
 
 /**
  * @brief Validate sensor reading for reasonableness
  * @param sensor_id Sensor channel
  * @param temperature Temperature value to validate
  * @return true if valid, false if invalid
  */
 bool TempControl_ValidateSensorReading(uint8_t sensor_id, float temperature)
 {
     // Basic range check for all sensors
     if (temperature < -50.0f || temperature > 100.0f) {
         return false;
     }
     
     // Specific validation based on sensor type
     switch (sensor_id) {
         case TEMP_SENSOR_RETURN_WATER:
             // Return water should be between 0°C and 25°C for chiller operation
             return TempControl_IsTemperatureInRange(temperature, 0.0f, 25.0f);
             
         case TEMP_SENSOR_SUPPLY_WATER:
             // Supply water should be between 0°C and 20°C (always cooler than return)
             return TempControl_IsTemperatureInRange(temperature, 0.0f, 20.0f);
             
         case TEMP_SENSOR_AMBIENT:
             // Ambient temperature range for hot climate operation
             return TempControl_IsTemperatureInRange(temperature, 0.0f, 60.0f);
             
         case TEMP_SENSOR_CONDENSER:
             // Condenser temperature can be higher in hot climates
             return TempControl_IsTemperatureInRange(temperature, 10.0f, 80.0f);
             
         default:
             return false;
     }
 }
 
 /**
  * @brief Update sensor statistics
  * @param sensor_id Sensor channel
  * @param temperature Current temperature reading
  */
 void TempControl_UpdateSensorStatistics(uint8_t sensor_id, float temperature)
 {
     if (sensor_id >= TEMP_CONTROL_MAX_SENSORS) return;
     
     TempSensorData_t* sensor = &temp_control_data.sensors[sensor_id];
     
     // Update min/max values
     if (temperature < sensor->min_value) {
         sensor->min_value = temperature;
     }
     if (temperature > sensor->max_value) {
         sensor->max_value = temperature;
     }
     
     // Update running average (simple exponential filter)
     sensor->average = (sensor->average * 0.9f) + (temperature * 0.1f);
 }
 
 /**
  * @brief Check if sensor data is valid
  * @param sensor_id Sensor channel
  * @return true if valid, false if invalid
  */
 bool TempControl_IsSensorValid(uint8_t sensor_id)
 {
     if (sensor_id >= TEMP_CONTROL_MAX_SENSORS) return false;
     
     return temp_control_data.sensors[sensor_id].valid && 
            !TEMP_CONTROL_IS_SENSOR_TIMEOUT(sensor_id);
 }
 
 // ========================================================================
 // PID CONTROL FUNCTIONS
 // ========================================================================
 
 /**
  * @brief Initialize PID controller
  */
 void TempControl_PID_Init(void)
 {
     temp_control_data.pid.kp = PID_KP_DEFAULT;
     temp_control_data.pid.ki = PID_KI_DEFAULT;
     temp_control_data.pid.kd = PID_KD_DEFAULT;
     temp_control_data.pid.output_min = PID_OUTPUT_MIN;
     temp_control_data.pid.output_max = PID_OUTPUT_MAX;
     temp_control_data.pid.integral_max = PID_INTEGRAL_MAX;
     
     temp_control_data.pid.setpoint = temp_control_config.return_water_setpoint;
     temp_control_data.pid.previous_error = 0.0f;
     temp_control_data.pid.integral = 0.0f;
     temp_control_data.pid.output = 0.0f;
     temp_control_data.pid.last_update = HAL_GetTick();
 }
 
 /**
  * @brief Reset PID controller
  */
 void TempControl_PID_Reset(void)
 {
     temp_control_data.pid.previous_error = 0.0f;
     temp_control_data.pid.integral = 0.0f;
     temp_control_data.pid.output = 0.0f;
     temp_control_data.pid.last_update = HAL_GetTick();
 }
 
 /**
  * @brief Calculate PID output
  * @param setpoint Desired temperature setpoint
  * @param process_value Current temperature measurement
  * @return PID output (0-100%)
  */
 float TempControl_PID_Calculate(float setpoint, float process_value)
 {
     uint32_t current_time = HAL_GetTick();
     float dt = (current_time - temp_control_data.pid.last_update) / 1000.0f; // Convert to seconds
     
     if (dt <= 0.0f) dt = 0.001f; // Prevent division by zero
     
     // Calculate error
     float error = setpoint - process_value;
     
     // Proportional term
     float proportional = temp_control_data.pid.kp * error;
     
     // Integral term with windup protection
     temp_control_data.pid.integral += error * dt;
     if (temp_control_data.pid.integral > temp_control_data.pid.integral_max) {
         temp_control_data.pid.integral = temp_control_data.pid.integral_max;
     } else if (temp_control_data.pid.integral < -temp_control_data.pid.integral_max) {
         temp_control_data.pid.integral = -temp_control_data.pid.integral_max;
     }
     float integral = temp_control_data.pid.ki * temp_control_data.pid.integral;
     
     // Derivative term
     float derivative = temp_control_data.pid.kd * (error - temp_control_data.pid.previous_error) / dt;
     
     // Calculate total output
     float output = proportional + integral + derivative;
     
     // Apply output limits
     if (output > temp_control_data.pid.output_max) {
         output = temp_control_data.pid.output_max;
     } else if (output < temp_control_data.pid.output_min) {
         output = temp_control_data.pid.output_min;
     }
     
     // Update for next iteration
     temp_control_data.pid.previous_error = error;
     temp_control_data.pid.last_update = current_time;
     
     return output;
 }
 
 /**
  * @brief Set PID tuning parameters
  * @param kp Proportional gain
  * @param ki Integral gain  
  * @param kd Derivative gain
  */
 void TempControl_PID_SetTuning(float kp, float ki, float kd)
 {
     temp_control_data.pid.kp = kp;
     temp_control_data.pid.ki = ki;
     temp_control_data.pid.kd = kd;
     
     TempControl_SendDebugMessage("PID tuning parameters updated");
 }
 
 /**
  * @brief Set PID output limits
  * @param min_output Minimum output value
  * @param max_output Maximum output value
  */
 void TempControl_PID_SetLimits(float min_output, float max_output)
 {
     temp_control_data.pid.output_min = min_output;
     temp_control_data.pid.output_max = max_output;
 }
 
 // ========================================================================
 // SETPOINT MANAGEMENT
 // ========================================================================
 
 /**
  * @brief Set new temperature setpoint
  * @param new_setpoint New setpoint temperature
  * @return true if successful, false if invalid
  */
 bool TempControl_SetSetpoint(float new_setpoint)
 {
     if (!TEMP_CONTROL_IS_VALID_SETPOINT(new_setpoint)) {
         return false;
     }
     
     temp_control_data.pid.setpoint = new_setpoint;
     temp_control_config.return_water_setpoint = new_setpoint;
     
     snprintf(debug_buffer, sizeof(debug_buffer),
             "Setpoint changed to %.1f°C", new_setpoint);
     TempControl_SendDebugMessage(debug_buffer);
     
     return true;
 }
 
 /**
  * @brief Get current temperature setpoint
  * @return Current setpoint temperature
  */
 float TempControl_GetSetpoint(void)
 {
     return temp_control_data.pid.setpoint;
 }
 
 /**
  * @brief Calculate ambient temperature compensation
  * @param ambient_temp Current ambient temperature
  * @return Compensation value to add to setpoint
  */
 float TempControl_CalculateAmbientCompensation(float ambient_temp)
 {
     if (!temp_control_config.auto_compensation_enable) {
         return 0.0f;
     }
     
     float compensation = 0.0f;
     
     if (ambient_temp > temp_control_config.ambient_baseline) {
         // Hot climate - increase setpoint to reduce load
         compensation = TEMP_CONTROL_COMPENSATION_FACTOR(ambient_temp);
         compensation *= temp_control_config.compensation_factor;
         
         // Limit maximum compensation
         if (compensation > TEMP_AMBIENT_COMPENSATION_MAX) {
             compensation = TEMP_AMBIENT_COMPENSATION_MAX;
         }
     }
     
     return compensation;
 }
 
 // ========================================================================
 // PERFORMANCE MONITORING
 // ========================================================================
 
 /**
  * @brief Calculate cooling efficiency
  * @return Cooling efficiency as percentage (0-100%)
  */
 float TempControl_CalculateEfficiency(void)
 {
     if (!TempControl_IsSensorValid(TEMP_SENSOR_RETURN_WATER) ||
         !TempControl_IsSensorValid(TEMP_SENSOR_SUPPLY_WATER)) {
         return 0.0f;
     }
     
     float return_temp = temp_control_data.sensors[TEMP_SENSOR_RETURN_WATER].value;
     float supply_temp = temp_control_data.sensors[TEMP_SENSOR_SUPPLY_WATER].value;
     float delta_t = return_temp - supply_temp;
     
     // Simple efficiency calculation based on temperature difference
     // Higher delta T = better efficiency
     float efficiency = (delta_t / 8.0f) * 100.0f; // Assume 8°C delta T = 100% efficiency
     
     if (efficiency > 100.0f) efficiency = 100.0f;
     if (efficiency < 0.0f) efficiency = 0.0f;
     
     return efficiency;
 }
 
 /**
  * @brief Get supply-return temperature difference
  * @return Temperature difference (delta T)
  */
 float TempControl_GetDeltaT(void)
 {
     if (!TempControl_IsSensorValid(TEMP_SENSOR_RETURN_WATER) ||
         !TempControl_IsSensorValid(TEMP_SENSOR_SUPPLY_WATER)) {
         return 0.0f;
     }
     
     return temp_control_data.sensors[TEMP_SENSOR_RETURN_WATER].value - 
            temp_control_data.sensors[TEMP_SENSOR_SUPPLY_WATER].value;
 }
 
 /**
  * @brief Update performance metrics
  */
 void TempControl_UpdatePerformanceMetrics(void)
 {
     temp_control_data.cooling_efficiency = TempControl_CalculateEfficiency();
     temp_control_data.delta_t = TempControl_GetDeltaT();
     
     // Update ambient compensation
     if (TempControl_IsSensorValid(TEMP_SENSOR_AMBIENT)) {
         float ambient_temp = temp_control_data.sensors[TEMP_SENSOR_AMBIENT].value;
         temp_control_data.ambient_compensation_active = 
             TempControl_CalculateAmbientCompensation(ambient_temp);
     }
 }
 
 // ========================================================================
 // HOT CLIMATE OPTIMIZATION
 // ========================================================================
 
 /**
  * @brief Process ambient temperature compensation
  */
 static void TempControl_ProcessAmbientCompensation(void)
 {
     if (!TempControl_IsSensorValid(TEMP_SENSOR_AMBIENT)) {
         return;
     }
     
     float ambient_temp = temp_control_data.sensors[TEMP_SENSOR_AMBIENT].value;
     
     // Apply hot climate compensation
     if (TempControl_IsHotClimateCondition()) {
         TempControl_AdaptPIDForAmbient(ambient_temp);
     }
 }
 
 /**
  * @brief Adapt PID parameters for ambient temperature
  * @param ambient_temp Current ambient temperature
  */
 void TempControl_AdaptPIDForAmbient(float ambient_temp)
 {
     if (ambient_temp > 40.0f) {
         // Very hot conditions - more conservative PID
         temp_control_data.pid.kp = PID_KP_DEFAULT * 0.8f;
         temp_control_data.pid.ki = PID_KI_DEFAULT * 0.6f;
     } else if (ambient_temp > 35.0f) {
         // Hot conditions - slightly conservative PID
         temp_control_data.pid.kp = PID_KP_DEFAULT * 0.9f;
         temp_control_data.pid.ki = PID_KI_DEFAULT * 0.8f;
     } else {
         // Normal conditions - standard PID
         temp_control_data.pid.kp = PID_KP_DEFAULT;
         temp_control_data.pid.ki = PID_KI_DEFAULT;
     }
 }
 
 /**
  * @brief Check if hot climate conditions exist
  * @return true if hot climate conditions detected
  */
 bool TempControl_IsHotClimateCondition(void)
 {
     if (!TempControl_IsSensorValid(TEMP_SENSOR_AMBIENT)) {
         return false;
     }
     
     return temp_control_data.sensors[TEMP_SENSOR_AMBIENT].value > 35.0f;
 }
 
 // ========================================================================
 // FAULT MANAGEMENT
 // ========================================================================
 
 /**
  * @brief Set a temperature control fault
  * @param fault_type Type of fault
  * @param description Fault description
  * @return true if fault was set
  */
 bool TempControl_SetFault(TempFaultType_t fault_type, const char* description)
 {
     temp_control_data.active_fault = fault_type;
     temp_control_data.fault_timestamp = HAL_GetTick();
     temp_control_data.fault_count++;
     
     strncpy(temp_control_data.fault_description, description, 63);
     temp_control_data.fault_description[63] = '\0';
     
     // Update system state based on fault severity
     switch (fault_type) {
         case TEMP_FAULT_SENSOR_RETURN_WATER:
             temp_control_data.system_state = TEMP_CONTROL_STATE_FAULT;
             break;
         case TEMP_FAULT_TEMPERATURE_RANGE:
         case TEMP_FAULT_SETPOINT_DEVIATION:
             temp_control_data.system_state = TEMP_CONTROL_STATE_WARNING;
             break;
         default:
             temp_control_data.system_state = TEMP_CONTROL_STATE_WARNING;
             break;
     }
     
     snprintf(debug_buffer, sizeof(debug_buffer),
             "TEMP FAULT: %s", description);
     TempControl_SendDebugMessage(debug_buffer);
     
     return true;
 }
 
 /**
  * @brief Clear active fault
  */
 void TempControl_ClearFault(void)
 {
     temp_control_data.active_fault = TEMP_FAULT_NONE;
     temp_control_data.system_state = TEMP_CONTROL_STATE_NORMAL;
     memset(temp_control_data.fault_description, 0, sizeof(temp_control_data.fault_description));
     
     TempControl_SendDebugMessage("Temperature control fault cleared");
 }
 
 /**
  * @brief Check if any fault is active
  * @return true if fault is active
  */
 bool TempControl_IsFaultActive(void)
 {
     return temp_control_data.active_fault != TEMP_FAULT_NONE;
 }
 
 // ========================================================================
 // DATA ACCESS FUNCTIONS
 // ========================================================================
 
 /**
  * @brief Get return water temperature
  * @return Return water temperature in °C
  */
 float TempControl_GetReturnWaterTemp(void)
 {
     if (TempControl_IsSensorValid(TEMP_SENSOR_RETURN_WATER)) {
         return temp_control_data.sensors[TEMP_SENSOR_RETURN_WATER].value;
     }
     return 0.0f;
 }
 
 /**
  * @brief Get supply water temperature
  * @return Supply water temperature in °C
  */
 float TempControl_GetSupplyWaterTemp(void)
 {
     if (TempControl_IsSensorValid(TEMP_SENSOR_SUPPLY_WATER)) {
         return temp_control_data.sensors[TEMP_SENSOR_SUPPLY_WATER].value;
     }
     return 0.0f;
 }
 
 /**
  * @brief Get ambient temperature
  * @return Ambient temperature in °C
  */
 float TempControl_GetAmbientTemp(void)
 {
     if (TempControl_IsSensorValid(TEMP_SENSOR_AMBIENT)) {
         return temp_control_data.sensors[TEMP_SENSOR_AMBIENT].value;
     }
     return 0.0f;
 }
 
 /**
  * @brief Get current PID output
  * @return PID output percentage (0-100%)
  */
 float TempControl_GetPIDOutput(void)
 {
     return temp_control_data.pid.output;
 }
 
 /**
  * @brief Get current system state
  * @return Current temperature control state
  */
 TempControlState_t TempControl_GetSystemState(void)
 {
     return temp_control_data.system_state;
 }
 
 // ========================================================================
 // HMI INTEGRATION
 // ========================================================================
 
 /**
  * @brief Update HMI with temperature control data
  */
 void TempControl_UpdateHMI(void)
 {
     // Update temperature readings (scaled by 10 for decimal places)
     HMI_Write_VP_Register(VP_TEMP_RETURN_WATER, (uint16_t)(TempControl_GetReturnWaterTemp() * 10));
     HMI_Write_VP_Register(VP_TEMP_SUPPLY_WATER, (uint16_t)(TempControl_GetSupplyWaterTemp() * 10));
     HMI_Write_VP_Register(VP_TEMP_AMBIENT, (uint16_t)(TempControl_GetAmbientTemp() * 10));
     
     // Update setpoint and PID data
     HMI_Write_VP_Register(VP_TEMP_SETPOINT, (uint16_t)(temp_control_data.pid.setpoint * 10));
     HMI_Write_VP_Register(VP_TEMP_PID_OUTPUT, (uint16_t)temp_control_data.pid.output);
     
     // Update performance metrics
     HMI_Write_VP_Register(VP_TEMP_EFFICIENCY, (uint16_t)temp_control_data.cooling_efficiency);
     HMI_Write_VP_Register(VP_TEMP_DELTA_T, (uint16_t)(temp_control_data.delta_t * 10));
     
     // Update system status
     HMI_Write_VP_Register(VP_TEMP_CONTROL_STATE, (uint16_t)temp_control_data.system_state);
     HMI_Write_VP_Register(VP_TEMP_CONTROL_MODE, (uint16_t)temp_control_data.control_mode);
     
     // Update fault information
     HMI_Write_VP_Register(VP_TEMP_FAULT_ACTIVE, TempControl_IsFaultActive() ? 1 : 0);
     HMI_Write_VP_Register(VP_TEMP_FAULT_TYPE, (uint16_t)temp_control_data.active_fault);
 }
 
 // ========================================================================
 // DEBUG AND DIAGNOSTIC FUNCTIONS
 // ========================================================================
 
 /**
  * @brief Print temperature control status
  */
 void TempControl_PrintStatus(void)
 {
     TempControl_SendDebugMessage("=== TEMPERATURE CONTROL STATUS ===");
     
     snprintf(debug_buffer, sizeof(debug_buffer), "State: %s", 
              TempControl_GetStateDescription(temp_control_data.system_state));
     TempControl_SendDebugMessage(debug_buffer);
     
     snprintf(debug_buffer, sizeof(debug_buffer), "Mode: %s", 
              TempControl_GetModeDescription(temp_control_data.control_mode));
     TempControl_SendDebugMessage(debug_buffer);
     
     snprintf(debug_buffer, sizeof(debug_buffer), "Uptime: %lu seconds", 
              temp_control_data.uptime_seconds);
     TempControl_SendDebugMessage(debug_buffer);
     
     if (TempControl_IsFaultActive()) {
         snprintf(debug_buffer, sizeof(debug_buffer), "FAULT: %s", 
                  temp_control_data.fault_description);
         TempControl_SendDebugMessage(debug_buffer);
     }
 }
 
 /**
  * @brief Print sensor data
  */
 void TempControl_PrintSensorData(void)
 {
     TempControl_SendDebugMessage("=== TEMPERATURE SENSORS ===");
     
     const char* sensor_names[] = {"Return Water", "Supply Water", "Ambient", "Condenser"};
     
     for (uint8_t i = 0; i < TEMP_CONTROL_MAX_SENSORS; i++) {
         if (TempControl_IsSensorValid(i)) {
             snprintf(debug_buffer, sizeof(debug_buffer), 
                     "%s (A%d): %.1f°C [Valid]", 
                     sensor_names[i], i, temp_control_data.sensors[i].value);
         } else {
             snprintf(debug_buffer, sizeof(debug_buffer), 
                     "%s (A%d): FAULT [Invalid]", 
                     sensor_names[i], i);
         }
         TempControl_SendDebugMessage(debug_buffer);
     }
     
     snprintf(debug_buffer, sizeof(debug_buffer), "Delta T: %.1f°C", 
              temp_control_data.delta_t);
     TempControl_SendDebugMessage(debug_buffer);
     
     snprintf(debug_buffer, sizeof(debug_buffer), "Efficiency: %.1f%%", 
              temp_control_data.cooling_efficiency);
     TempControl_SendDebugMessage(debug_buffer);
 }
 
 /**
  * @brief Print PID controller status
  */
 void TempControl_PrintPIDStatus(void)
 {
     TempControl_SendDebugMessage("=== PID CONTROLLER STATUS ===");
     
     snprintf(debug_buffer, sizeof(debug_buffer), "Setpoint: %.1f°C", 
              temp_control_data.pid.setpoint);
     TempControl_SendDebugMessage(debug_buffer);
     
     snprintf(debug_buffer, sizeof(debug_buffer), "Output: %.1f%%", 
              temp_control_data.pid.output);
     TempControl_SendDebugMessage(debug_buffer);
     
     snprintf(debug_buffer, sizeof(debug_buffer), "Error: %.2f°C", 
              temp_control_data.pid.error_current);
     TempControl_SendDebugMessage(debug_buffer);
     
     snprintf(debug_buffer, sizeof(debug_buffer), "PID Gains: Kp=%.2f Ki=%.2f Kd=%.2f", 
              temp_control_data.pid.kp, temp_control_data.pid.ki, temp_control_data.pid.kd);
     TempControl_SendDebugMessage(debug_buffer);
     
     if (temp_control_config.auto_compensation_enable) {
         snprintf(debug_buffer, sizeof(debug_buffer), "Ambient Compensation: +%.2f°C", 
                  temp_control_data.ambient_compensation_active);
         TempControl_SendDebugMessage(debug_buffer);
     }
 }
 
 // ========================================================================
 // DEBUG COMMAND HANDLERS
 // ========================================================================
 
 /**
  * @brief Debug command: temp_status
  */
 void TempControl_Debug_Status(void)
 {
     TempControl_PrintStatus();
 }
 
 /**
  * @brief Debug command: temp_sensors
  */
 void TempControl_Debug_Sensors(void)
 {
     TempControl_PrintSensorData();
 }
 
 /**
  * @brief Debug command: temp_pid
  */
 void TempControl_Debug_PID(void)
 {
     TempControl_PrintPIDStatus();
 }
 
 /**
  * @brief Debug command: temp_setpoint [value]
  */
 void TempControl_Debug_SetSetpoint(float new_setpoint)
 {
     if (TempControl_SetSetpoint(new_setpoint)) {
         snprintf(debug_buffer, sizeof(debug_buffer), 
                 "Setpoint set to %.1f°C", new_setpoint);
         TempControl_SendDebugMessage(debug_buffer);
     } else {
         TempControl_SendDebugMessage("Invalid setpoint - must be between 4.0°C and 18.0°C");
     }
 }
 
 /**
  * @brief Debug command: temp_efficiency
  */
 void TempControl_Debug_Efficiency(void)
 {
     TempControl_SendDebugMessage("=== COOLING EFFICIENCY ===");
     
     snprintf(debug_buffer, sizeof(debug_buffer), "Current Efficiency: %.1f%%", 
              temp_control_data.cooling_efficiency);
     TempControl_SendDebugMessage(debug_buffer);
     
     snprintf(debug_buffer, sizeof(debug_buffer), "Temperature Delta: %.1f°C", 
              temp_control_data.delta_t);
     TempControl_SendDebugMessage(debug_buffer);
     
     snprintf(debug_buffer, sizeof(debug_buffer), "Return Water: %.1f°C", 
              TempControl_GetReturnWaterTemp());
     TempControl_SendDebugMessage(debug_buffer);
     
     snprintf(debug_buffer, sizeof(debug_buffer), "Supply Water: %.1f°C", 
              TempControl_GetSupplyWaterTemp());
     TempControl_SendDebugMessage(debug_buffer);
 }
 
 // ========================================================================
 // UTILITY FUNCTIONS
 // ========================================================================
 
 /**
  * @brief Send debug message
  */
 static void TempControl_SendDebugMessage(const char* message)
 {
     Send_Debug_Data((char*)message);
     Send_Debug_Data("\r\n");
 }
 
 /**
  * @brief Log event to system
  */
 static void TempControl_LogEvent(const char* event)
 {
     FlashConfig_LogEvent("TEMP_CTRL", event, 0);
 }
 
 /**
  * @brief Check if temperature is within specified range
  */
 bool TempControl_IsTemperatureInRange(float temperature, float min_temp, float max_temp)
 {
     return (temperature >= min_temp && temperature <= max_temp);
 }
 
 /**
  * @brief Get control mode description string
  */
 const char* TempControl_GetModeDescription(TempControlMode_t mode)
 {
     switch (mode) {
         case TEMP_CONTROL_MODE_OFF: return "OFF";
         case TEMP_CONTROL_MODE_MANUAL: return "MANUAL";
         case TEMP_CONTROL_MODE_AUTO: return "AUTO";
         case TEMP_CONTROL_MODE_SETPOINT_RAMP: return "RAMP";
         case TEMP_CONTROL_MODE_FAULT_RECOVERY: return "RECOVERY";
         default: return "UNKNOWN";
     }
 }
 
 /**
  * @brief Get control state description string
  */
 const char* TempControl_GetStateDescription(TempControlState_t state)
 {
     switch (state) {
         case TEMP_CONTROL_STATE_NORMAL: return "NORMAL";
         case TEMP_CONTROL_STATE_WARNING: return "WARNING";
         case TEMP_CONTROL_STATE_FAULT: return "FAULT";
         case TEMP_CONTROL_STATE_EMERGENCY: return "EMERGENCY";
         default: return "UNKNOWN";
     }
 }
 
 /**
  * @brief Get fault description string
  */
 const char* TempControl_GetFaultDescription(TempFaultType_t fault)
 {
     switch (fault) {
         case TEMP_FAULT_NONE: return "No Fault";
         case TEMP_FAULT_SENSOR_RETURN_WATER: return "Return Water Sensor Fault";
         case TEMP_FAULT_SENSOR_SUPPLY_WATER: return "Supply Water Sensor Fault";
         case TEMP_FAULT_SENSOR_AMBIENT: return "Ambient Sensor Fault";
         case TEMP_FAULT_TEMPERATURE_RANGE: return "Temperature Out of Range";
         case TEMP_FAULT_COOLING_EFFICIENCY: return "Low Cooling Efficiency";
         case TEMP_FAULT_PID_SATURATED: return "PID Output Saturated";
         case TEMP_FAULT_SETPOINT_DEVIATION: return "Large Setpoint Deviation";
         case TEMP_FAULT_SYSTEM_OVERLOAD: return "System Overload";
         default: return "Unknown Fault";
     }
 }
 
 /**
  * ========================================================================
  * END OF TEMPERATURE CONTROL IMPLEMENTATION
  * ========================================================================
  */

 void TempControl_SetDebugEnabled(bool enabled)
 {
     // Simple implementation using static variable
     temp_control_debug_enabled = enabled;
 }
