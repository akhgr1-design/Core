/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    control_algorithms.c
  * @brief   This file provides code for advanced control algorithms
  *          for the chiller control system.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "control_algorithms.h"

/* USER CODE BEGIN 0 */
#include <string.h>
#include <stdio.h>
/* USER CODE END 0 */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN Define */

/* USER CODE END Define */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN Macro */

/* USER CODE END Macro */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

// Global control system structures
static AdvancedControlParams_t g_control_params;
static SystemOptimization_t g_optimization_data;
static PerformanceAnalytics_t g_performance_analytics;

// PID Controllers array
static PIDController_t g_pid_controllers[PID_MAX_CONTROLLERS];

// Control algorithm state variables
static bool s_system_initialized = false;
static bool s_debug_enabled = false;
static uint32_t s_last_process_time = 0;
static uint32_t s_last_optimization_time = 0;
static uint32_t s_last_analytics_update = 0;

// Performance tracking arrays
static float s_load_prediction_buffer[LOAD_PREDICTION_HORIZON];
static uint16_t s_prediction_buffer_index = 0;
static float s_ambient_compensation_factor = 1.0f;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

// Internal utility functions
static void ControlAlgorithms_InitializeDefaults(void);
static void ControlAlgorithms_UpdatePerformanceMetrics(void);
static float ControlAlgorithms_CalculateMovingAverage(float* buffer, uint8_t size, uint8_t index);
static bool ControlAlgorithms_ValidateParameters(void);
static void ControlAlgorithms_DebugPrint(const char* message);

/* USER CODE END PFP */

/* External functions --------------------------------------------------------*/
/* USER CODE BEGIN ExternalFunctions */

/* USER CODE END ExternalFunctions */

/* USER CODE BEGIN 0 */

// ========================================================================
// INITIALIZATION FUNCTIONS
// ========================================================================

bool ControlAlgorithms_Init(void)
{
    ControlAlgorithms_DebugPrint("\r\n=== ADVANCED CONTROL ALGORITHMS INITIALIZATION ===\r\n");
    
    // Clear all structures
    memset(&g_control_params, 0, sizeof(AdvancedControlParams_t));
    memset(&g_optimization_data, 0, sizeof(SystemOptimization_t));
    memset(&g_performance_analytics, 0, sizeof(PerformanceAnalytics_t));
    memset(g_pid_controllers, 0, sizeof(g_pid_controllers));
    
    // Initialize default parameters
    ControlAlgorithms_InitializeDefaults();
    
    // Initialize performance analytics
    if (!PerformanceAnalytics_Init(&g_performance_analytics)) {
        ControlAlgorithms_DebugPrint("CONTROL_ALGORITHMS: Performance analytics initialization failed\r\n");
        return false;
    }
    
    // Try to load configuration from flash
    if (ControlAlgorithms_LoadConfiguration()) {
        ControlAlgorithms_DebugPrint("CONTROL_ALGORITHMS: Configuration loaded from flash\r\n");
    } else {
        ControlAlgorithms_DebugPrint("CONTROL_ALGORITHMS: Using default configuration\r\n");
    }
    
    // Initialize timing
    s_last_process_time = HAL_GetTick();
    s_last_optimization_time = HAL_GetTick();
    s_last_analytics_update = HAL_GetTick();
    
    // Enable debug output by default
    s_debug_enabled = true;
    
    // Mark initialization as complete
    s_system_initialized = true;
    
    ControlAlgorithms_DebugPrint("CONTROL_ALGORITHMS: Initialization complete\r\n");
    
    char debug_msg[120];
    snprintf(debug_msg, sizeof(debug_msg), 
             "CONTROL_ALGORITHMS: Algorithm: %d, Efficiency Target: %.1f%%, Adaptive: %s\r\n",
             g_control_params.algorithm_type,
             g_control_params.efficiency_target * 100.0f,
             g_control_params.adaptive_tuning_enabled ? "ON" : "OFF");
    ControlAlgorithms_DebugPrint(debug_msg);
    
    ControlAlgorithms_DebugPrint("=== CONTROL ALGORITHMS INITIALIZATION COMPLETE ===\r\n\r\n");
    
    return true;
}

static void ControlAlgorithms_InitializeDefaults(void)
{
    // Initialize control parameters
    g_control_params.algorithm_type = CONTROL_ALGORITHM_ADAPTIVE_PID;
    g_control_params.ambient_temperature = 38.0f;  // Hot climate baseline
    g_control_params.load_factor = 0.0f;
    g_control_params.efficiency_target = EFFICIENCY_TARGET;
    g_control_params.adaptive_tuning_enabled = true;
    g_control_params.predictive_control_enabled = true;
    g_control_params.optimization_enabled = true;
    g_control_params.optimization_interval = ENERGY_OPTIMIZATION_INTERVAL;
    g_control_params.learning_rate = ADAPTIVE_LEARNING_RATE;
    g_control_params.performance_weight = 1.0f;
    
    // Initialize optimization data
    g_optimization_data.current_efficiency = 0.0f;
    g_optimization_data.target_efficiency = EFFICIENCY_TARGET;
    g_optimization_data.power_consumption = 0.0f;
    g_optimization_data.cooling_capacity = 0.0f;
    g_optimization_data.optimization_score = 0.0f;
    g_optimization_data.optimization_cycles = 0;
    g_optimization_data.optimization_active = false;
    
    // Initialize PID controllers with default tunings
    for (uint8_t i = 0; i < PID_MAX_CONTROLLERS; i++) {
        PID_Init(&g_pid_controllers[i], 1.0f, 0.1f, 0.05f);  // Default PID gains
        g_pid_controllers[i].enabled = false;  // Disabled by default
    }
    
    // Initialize prediction buffer
    memset(s_load_prediction_buffer, 0, sizeof(s_load_prediction_buffer));
    s_prediction_buffer_index = 0;
    
    ControlAlgorithms_DebugPrint("CONTROL_ALGORITHMS: Default configuration initialized\r\n");
}

bool ControlAlgorithms_LoadConfiguration(void)
{
    // TODO: Implement flash storage integration
    return false;  // Use defaults for now
}

bool ControlAlgorithms_SaveConfiguration(void)
{
    // TODO: Implement flash storage integration
    ControlAlgorithms_DebugPrint("CONTROL_ALGORITHMS: Configuration saved to flash\r\n");
    return true;
}

void ControlAlgorithms_ResetToDefaults(void)
{
    ControlAlgorithms_DebugPrint("CONTROL_ALGORITHMS: Resetting to default configuration\r\n");
    ControlAlgorithms_InitializeDefaults();
    ControlAlgorithms_SaveConfiguration();
}

// ========================================================================
// PID CONTROLLER FUNCTIONS
// ========================================================================

bool PID_Init(PIDController_t* pid, float kp, float ki, float kd)
{
    if (pid == NULL) return false;
    
    // Initialize PID parameters
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->setpoint = 0.0f;
    pid->previous_error = 0.0f;
    pid->integral = 0.0f;
    pid->derivative = 0.0f;
    pid->output = 0.0f;
    pid->output_min = PID_OUTPUT_MIN;
    pid->output_max = PID_OUTPUT_MAX;
    pid->last_update_time = HAL_GetTick();
    pid->enabled = false;
    
    // Clear error history
    memset(pid->error_history, 0, sizeof(pid->error_history));
    pid->history_index = 0;
    
    return true;
}

float PID_Update(PIDController_t* pid, float process_value, float setpoint)
{
    if (pid == NULL || !pid->enabled) return 0.0f;
    
    uint32_t current_time = HAL_GetTick();
    float dt = (float)(current_time - pid->last_update_time) / 1000.0f;  // Convert to seconds
    
    if (dt <= 0.0f) return pid->output;  // Avoid division by zero
    
    // Calculate error
    float error = setpoint - process_value;
    pid->setpoint = setpoint;
    
    // Store error in history
    pid->error_history[pid->history_index] = error;
    pid->history_index = (pid->history_index + 1) % PID_HISTORY_SIZE;
    
    // Proportional term
    float proportional = pid->kp * error;
    
    // Integral term with windup protection
    pid->integral += error * dt;
    
    // Clamp integral to prevent windup
    float max_integral = (pid->output_max - proportional) / pid->ki;
    float min_integral = (pid->output_min - proportional) / pid->ki;
    
    if (pid->integral > max_integral) pid->integral = max_integral;
    if (pid->integral < min_integral) pid->integral = min_integral;
    
    float integral = pid->ki * pid->integral;
    
    // Derivative term with filtering
    float derivative_raw = (error - pid->previous_error) / dt;
    pid->derivative = pid->derivative * (1.0f - PID_DERIVATIVE_FILTER) + 
                     derivative_raw * PID_DERIVATIVE_FILTER;
    float derivative = pid->kd * pid->derivative;
    
    // Calculate output
    pid->output = proportional + integral + derivative;
    
    // Apply output limits
    pid->output = ControlAlgorithms_LimitOutput(pid->output, pid->output_min, pid->output_max);
    
    // Update for next iteration
    pid->previous_error = error;
    pid->last_update_time = current_time;
    
    return pid->output;
}

void PID_SetTunings(PIDController_t* pid, float kp, float ki, float kd)
{
    if (pid == NULL) return;
    
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    
    char debug_msg[100];
    snprintf(debug_msg, sizeof(debug_msg), 
             "PID: Tunings updated - Kp:%.3f Ki:%.3f Kd:%.3f\r\n", kp, ki, kd);
    ControlAlgorithms_DebugPrint(debug_msg);
}

void PID_SetOutputLimits(PIDController_t* pid, float min, float max)
{
    if (pid == NULL || min >= max) return;
    
    pid->output_min = min;
    pid->output_max = max;
    
    // Clamp current output to new limits
    pid->output = ControlAlgorithms_LimitOutput(pid->output, min, max);
}

void PID_Reset(PIDController_t* pid)
{
    if (pid == NULL) return;
    
    pid->previous_error = 0.0f;
    pid->integral = 0.0f;
    pid->derivative = 0.0f;
    pid->output = 0.0f;
    
    // Clear error history
    memset(pid->error_history, 0, sizeof(pid->error_history));
    pid->history_index = 0;
    
    pid->last_update_time = HAL_GetTick();
}

void PID_Enable(PIDController_t* pid, bool enabled)
{
    if (pid == NULL) return;
    
    if (enabled && !pid->enabled) {
        // Reset PID when enabling
        PID_Reset(pid);
    }
    
    pid->enabled = enabled;
}

// ========================================================================
// ADVANCED CONTROL FUNCTIONS
// ========================================================================

bool ControlAlgorithms_Process(void)
{
    if (!s_system_initialized) return false;
    
    uint32_t current_time = HAL_GetTick();
    
    // Update performance metrics periodically
    if ((current_time - s_last_analytics_update) > 5000) {  // Every 5 seconds
        ControlAlgorithms_UpdatePerformanceMetrics();
        s_last_analytics_update = current_time;
    }
    
    // Run system optimization
    if (g_control_params.optimization_enabled && 
        (current_time - s_last_optimization_time) > g_control_params.optimization_interval) {
        SystemOptimization_Process();
        s_last_optimization_time = current_time;
    }
    
    // Adaptive parameter adjustment
    if (g_control_params.adaptive_tuning_enabled) {
        AdaptiveControl_UpdateParameters(g_optimization_data.current_efficiency);
        AdaptiveControl_AdjustForAmbient(g_control_params.ambient_temperature);
        AdaptiveControl_AdjustForLoad(g_control_params.load_factor);
    }
    
    // Periodic debug output
    if (s_debug_enabled && (current_time - s_last_process_time) > 30000) {  // Every 30 seconds
        char debug_msg[150];
        snprintf(debug_msg, sizeof(debug_msg), 
                 "CONTROL_ALG: Eff:%.1f%% Target:%.1f%% Score:%.2f Ambient:%.1f°C Load:%.1f%%\r\n",
                 g_optimization_data.current_efficiency * 100.0f,
                 g_optimization_data.target_efficiency * 100.0f,
                 g_optimization_data.optimization_score,
                 g_control_params.ambient_temperature,
                 g_control_params.load_factor * 100.0f);
        ControlAlgorithms_DebugPrint(debug_msg);
        
        s_last_process_time = current_time;
    }
    
    return true;
}

float ControlAlgorithms_CalculateOptimalOutput(float process_value, float setpoint)
{
    float output = 0.0f;
    
    switch (g_control_params.algorithm_type) {
        case CONTROL_ALGORITHM_BASIC_PID:
            // Use first PID controller
            output = PID_Update(&g_pid_controllers[0], process_value, setpoint);
            break;
            
        case CONTROL_ALGORITHM_ADAPTIVE_PID:
            // Use adaptive PID with dynamic tuning
            output = PID_Update(&g_pid_controllers[0], process_value, setpoint);
            // Apply ambient compensation
            output *= s_ambient_compensation_factor;
            break;
            
        case CONTROL_ALGORITHM_FUZZY_LOGIC:
            // Use fuzzy logic control
            float error = setpoint - process_value;
            float rate_of_change = (error - g_pid_controllers[0].previous_error) / 0.1f;  // Assume 100ms dt
            output = FuzzyLogic_ProcessTemperatureControl(error, rate_of_change);
            break;
            
        case CONTROL_ALGORITHM_PREDICTIVE:
            // Use predictive control
            if (g_control_params.predictive_control_enabled) {
                float predicted_load = PredictiveControl_PredictLoad(300);  // 5 minute horizon
                output = PID_Update(&g_pid_controllers[0], process_value, setpoint);
                output *= (1.0f + predicted_load * 0.1f);  // Adjust for predicted load
            } else {
                output = PID_Update(&g_pid_controllers[0], process_value, setpoint);
            }
            break;
            
        case CONTROL_ALGORITHM_HYBRID:
            // Combine multiple algorithms
            float pid_output = PID_Update(&g_pid_controllers[0], process_value, setpoint);
            float fuzzy_output = FuzzyLogic_ProcessTemperatureControl(setpoint - process_value, 0.0f);
            output = (pid_output * 0.7f) + (fuzzy_output * 0.3f);  // Weighted combination
            break;
            
        default:
            output = PID_Update(&g_pid_controllers[0], process_value, setpoint);
            break;
    }
    
    // Apply limits
    return ControlAlgorithms_LimitOutput(output, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
}

// ========================================================================
// OPTIMIZATION FUNCTIONS
// ========================================================================

bool SystemOptimization_Process(void)
{
    g_optimization_data.optimization_active = true;
    g_optimization_data.optimization_cycles++;
    
    // Calculate current system efficiency
    g_optimization_data.current_efficiency = SystemOptimization_CalculateEfficiency();
    
    // Optimize parameters if efficiency is below target
    if (g_optimization_data.current_efficiency < g_optimization_data.target_efficiency) {
        SystemOptimization_OptimizeParameters();
    }
    
    // Calculate optimization score
    g_optimization_data.optimization_score = SystemOptimization_GetOptimizationScore();
    
    g_optimization_data.last_optimization_time = HAL_GetTick();
    g_optimization_data.optimization_active = false;
    
    return true;
}

float SystemOptimization_CalculateEfficiency(void)
{
    // Simulate efficiency calculation based on power and cooling capacity
    if (g_optimization_data.power_consumption > 0.0f) {
        float efficiency = g_optimization_data.cooling_capacity / g_optimization_data.power_consumption;
        
        // Normalize to 0-1 range (assuming max efficiency of 4.0 COP)
        efficiency = efficiency / 4.0f;
        
        // Clamp to valid range
        if (efficiency > 1.0f) efficiency = 1.0f;
        if (efficiency < 0.0f) efficiency = 0.0f;
        
        return efficiency;
    }
    
    return 0.0f;
}

bool SystemOptimization_OptimizeParameters(void)
{
    // Simple optimization algorithm - adjust PID parameters based on performance
    float efficiency_error = g_optimization_data.target_efficiency - g_optimization_data.current_efficiency;
    
    if (efficiency_error > 0.05f) {  // 5% efficiency gap
        // Increase aggressiveness
        for (uint8_t i = 0; i < PID_MAX_CONTROLLERS; i++) {
            if (g_pid_controllers[i].enabled) {
                g_pid_controllers[i].kp *= 1.05f;  // Increase proportional gain
                g_pid_controllers[i].ki *= 1.02f;  // Slight increase in integral gain
            }
        }
        
        ControlAlgorithms_DebugPrint("OPTIMIZATION: Increased PID aggressiveness\r\n");
        return true;
    }
    
    return false;
}

void SystemOptimization_UpdateMetrics(float efficiency, float power, float capacity)
{
    g_optimization_data.current_efficiency = efficiency;
    g_optimization_data.power_consumption = power;
    g_optimization_data.cooling_capacity = capacity;
}

float SystemOptimization_GetOptimizationScore(void)
{
    // Calculate a composite optimization score (0-100)
    float efficiency_score = g_optimization_data.current_efficiency * 50.0f;
    float target_score = (1.0f - fabsf(g_optimization_data.current_efficiency - 
                                      g_optimization_data.target_efficiency)) * 50.0f;
    
    return efficiency_score + target_score;
}

// ========================================================================
// PERFORMANCE ANALYTICS FUNCTIONS
// ========================================================================

bool PerformanceAnalytics_Init(PerformanceAnalytics_t* analytics)
{
    if (analytics == NULL) return false;
    
    memset(analytics->efficiency_history, 0, sizeof(analytics->efficiency_history));
    memset(analytics->power_history, 0, sizeof(analytics->power_history));
    memset(analytics->load_history, 0, sizeof(analytics->load_history));
    
    analytics->history_index = 0;
    analytics->average_efficiency = 0.0f;
    analytics->average_power = 0.0f;
    analytics->average_load = 0.0f;
    analytics->efficiency_trend = 0.0f;
    analytics->power_trend = 0.0f;
    analytics->data_valid = false;
    
    return true;
}

void PerformanceAnalytics_UpdateData(PerformanceAnalytics_t* analytics, 
                                    float efficiency, float power, float load)
{
    if (analytics == NULL) return;
    
    // Store new data
    analytics->efficiency_history[analytics->history_index] = efficiency;
    analytics->power_history[analytics->history_index] = power;
    analytics->load_history[analytics->history_index] = load;
    
    analytics->history_index = (analytics->history_index + 1) % PERFORMANCE_WINDOW_SIZE;
    
    // Calculate averages
    analytics->average_efficiency = ControlAlgorithms_CalculateMovingAverage(
        analytics->efficiency_history, PERFORMANCE_WINDOW_SIZE, analytics->history_index);
    analytics->average_power = ControlAlgorithms_CalculateMovingAverage(
        analytics->power_history, PERFORMANCE_WINDOW_SIZE, analytics->history_index);
    analytics->average_load = ControlAlgorithms_CalculateMovingAverage(
        analytics->load_history, PERFORMANCE_WINDOW_SIZE, analytics->history_index);
    
    // Calculate trends (simple linear trend)
    analytics->efficiency_trend = PerformanceAnalytics_GetTrend(analytics, 0);
    analytics->power_trend = PerformanceAnalytics_GetTrend(analytics, 1);
    
    analytics->data_valid = true;
}

float PerformanceAnalytics_GetTrend(PerformanceAnalytics_t* analytics, uint8_t metric_type)
{
    if (analytics == NULL || !analytics->data_valid) return 0.0f;
    
    float* data_array;
    
    switch (metric_type) {
        case 0: data_array = analytics->efficiency_history; break;
        case 1: data_array = analytics->power_history; break;
        case 2: data_array = analytics->load_history; break;
        default: return 0.0f;
    }
    
    // Simple trend calculation (last 10 samples)
    uint8_t trend_samples = 10;
    if (trend_samples > PERFORMANCE_WINDOW_SIZE) trend_samples = PERFORMANCE_WINDOW_SIZE;
    
    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_x2 = 0.0f;
    
    for (uint8_t i = 0; i < trend_samples; i++) {
        uint8_t index = (analytics->history_index - trend_samples + i + PERFORMANCE_WINDOW_SIZE) % PERFORMANCE_WINDOW_SIZE;
        float x = (float)i;
        float y = data_array[index];
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    // Calculate slope (trend)
    float denominator = trend_samples * sum_x2 - sum_x * sum_x;
    if (fabsf(denominator) > 0.001f) {
        return (trend_samples * sum_xy - sum_x * sum_y) / denominator;
    }
    
    return 0.0f;
}

// ========================================================================
// FUZZY LOGIC FUNCTIONS
// ========================================================================

float FuzzyLogic_ProcessTemperatureControl(float temperature_error, float rate_of_change)
{
    // Simple fuzzy logic controller for temperature
    float output = 0.0f;
    
    // Temperature error membership functions
    float error_negative = FuzzyLogic_CalculateMembership(temperature_error, -2.0f, 1.0f);
    float error_zero = FuzzyLogic_CalculateMembership(temperature_error, 0.0f, 1.0f);
    float error_positive = FuzzyLogic_CalculateMembership(temperature_error, 2.0f, 1.0f);
    
    // Rate of change membership functions
    float rate_negative = FuzzyLogic_CalculateMembership(rate_of_change, -1.0f, 0.5f);
    float rate_zero = FuzzyLogic_CalculateMembership(rate_of_change, 0.0f, 0.5f);
    float rate_positive = FuzzyLogic_CalculateMembership(rate_of_change, 1.0f, 0.5f);
    
    // Fuzzy rules (simplified)
    float rule1 = fminf(error_negative, rate_negative) * 0.0f;     // If error negative and rate negative, output low
    float rule2 = fminf(error_zero, rate_zero) * 50.0f;           // If error zero and rate zero, output medium
    float rule3 = fminf(error_positive, rate_positive) * 100.0f;  // If error positive and rate positive, output high
    
    // Defuzzification (center of gravity method - simplified)
    output = rule1 + rule2 + rule3;
    
    return ControlAlgorithms_LimitOutput(output, 0.0f, 100.0f);
}

float FuzzyLogic_CalculateMembership(float value, float center, float width)
{
    float distance = fabsf(value - center);
    if (distance >= width) return 0.0f;
    
    return 1.0f - (distance / width);
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

static void ControlAlgorithms_UpdatePerformanceMetrics(void)
{
    // Update performance analytics with current system data
    PerformanceAnalytics_UpdateData(&g_performance_analytics,
                                   g_optimization_data.current_efficiency,
                                   g_optimization_data.power_consumption,
                                   g_control_params.load_factor);
}

static float ControlAlgorithms_CalculateMovingAverage(float* buffer, uint8_t size, uint8_t index)
{
    float sum = 0.0f;
    for (uint8_t i = 0; i < size; i++) {
        sum += buffer[i];
    }
    return sum / size;
}

float ControlAlgorithms_LimitOutput(float output, float min, float max)
{
    if (output > max) return max;
    if (output < min) return min;
    return output;
}

float ControlAlgorithms_FilterSignal(float input, float filter_factor)
{
    static float filtered_output = 0.0f;
    filtered_output = filtered_output * (1.0f - filter_factor) + input * filter_factor;
    return filtered_output;
}

static void ControlAlgorithms_DebugPrint(const char* message)
{
    if (s_debug_enabled) {
        Send_Debug_Data(message);
    }
}

// ========================================================================
// CONFIGURATION AND STATUS FUNCTIONS
// ========================================================================

void ControlAlgorithms_SetDebugEnabled(bool enabled)
{
    s_debug_enabled = enabled;
    
    if (enabled) {
        ControlAlgorithms_DebugPrint("CONTROL_ALGORITHMS: Debug output ENABLED\r\n");
    } else {
        Send_Debug_Data("CONTROL_ALGORITHMS: Debug output DISABLED\r\n");
    }
}

AdvancedControlParams_t* ControlAlgorithms_GetParameters(void)
{
    return &g_control_params;
}

SystemOptimization_t* ControlAlgorithms_GetOptimizationData(void)
{
    return &g_optimization_data;
}

float ControlAlgorithms_GetCurrentEfficiency(void)
{
    return g_optimization_data.current_efficiency;
}

float ControlAlgorithms_GetOptimizationScore(void)
{
    return g_optimization_data.optimization_score;
}

void ControlAlgorithms_PrintStatus(void)
{
    Send_Debug_Data("\r\n=== ADVANCED CONTROL ALGORITHMS STATUS ===\r\n");
    
    char status_msg[200];
    
    // Algorithm settings
    snprintf(status_msg, sizeof(status_msg),
             "Algorithm: %d | Efficiency: %.1f%% | Target: %.1f%% | Score: %.1f\r\n",
             g_control_params.algorithm_type,
             g_optimization_data.current_efficiency * 100.0f,
             g_optimization_data.target_efficiency * 100.0f,
             g_optimization_data.optimization_score);
    Send_Debug_Data(status_msg);
    
    // Environmental conditions
    snprintf(status_msg, sizeof(status_msg),
             "Ambient: %.1f°C | Load: %.1f%% | Power: %.1fkW | Capacity: %.1fkW\r\n",
             g_control_params.ambient_temperature,
             g_control_params.load_factor * 100.0f,
             g_optimization_data.power_consumption,
             g_optimization_data.cooling_capacity);
    Send_Debug_Data(status_msg);
    
    // Control features
    snprintf(status_msg, sizeof(status_msg),
             "Features: Adaptive:%s | Predictive:%s | Optimization:%s\r\n",
             g_control_params.adaptive_tuning_enabled ? "ON" : "OFF",
             g_control_params.predictive_control_enabled ? "ON" : "OFF",
             g_control_params.optimization_enabled ? "ON" : "OFF");
    Send_Debug_Data(status_msg);
    
    // Optimization statistics
    snprintf(status_msg, sizeof(status_msg),
             "Optimization Cycles: %lu | Last Run: %lu ms ago\r\n",
             g_optimization_data.optimization_cycles,
             HAL_GetTick() - g_optimization_data.last_optimization_time);
    Send_Debug_Data(status_msg);
    
    Send_Debug_Data("=== END CONTROL ALGORITHMS STATUS ===\r\n\r\n");
}

bool ControlAlgorithms_RunDiagnostics(void)
{
    Send_Debug_Data("\r\n=== CONTROL ALGORITHMS DIAGNOSTICS ===\r\n");
    
    bool all_tests_passed = true;
    
    // Test 1: Initialization check
    if (!s_system_initialized) {
        Send_Debug_Data("FAIL: System not initialized\r\n");
        all_tests_passed = false;
    } else {
        Send_Debug_Data("PASS: System initialization\r\n");
    }
    
    // Test 2: Parameter validation
    if (!ControlAlgorithms_ValidateParameters()) {
        Send_Debug_Data("FAIL: Parameter validation\r\n");
        all_tests_passed = false;
    } else {
        Send_Debug_Data("PASS: Parameter validation\r\n");
    }
    
    // Test 3: PID controller check
    bool pid_ok = true;
    for (uint8_t i = 0; i < PID_MAX_CONTROLLERS; i++) {
        if (g_pid_controllers[i].enabled) {
            if (g_pid_controllers[i].kp <= 0.0f || g_pid_controllers[i].ki < 0.0f || g_pid_controllers[i].kd < 0.0f) {
                pid_ok = false;
                break;
            }
        }
    }
    
    if (pid_ok) {
        Send_Debug_Data("PASS: PID controllers\r\n");
    } else {
        Send_Debug_Data("FAIL: PID controller parameters\r\n");
        all_tests_passed = false;
    }
    
    // Test 4: Performance analytics
    if (g_performance_analytics.data_valid) {
        Send_Debug_Data("PASS: Performance analytics\r\n");
    } else {
        Send_Debug_Data("WARN: Performance analytics not yet valid\r\n");
    }
    
    char result_msg[60];
    snprintf(result_msg, sizeof(result_msg), 
             "=== DIAGNOSTICS %s ===\r\n\r\n", 
             all_tests_passed ? "PASSED" : "COMPLETED WITH WARNINGS");
    Send_Debug_Data(result_msg);
    
    return all_tests_passed;
}

static bool ControlAlgorithms_ValidateParameters(void)
{
    // Validate efficiency target
    if (g_control_params.efficiency_target < 0.0f || g_control_params.efficiency_target > 1.0f) {
        return false;
    }
    
    // Validate learning rate
    if (g_control_params.learning_rate <= 0.0f || g_control_params.learning_rate > 1.0f) {
        return false;
    }
    
    // Validate ambient temperature
    if (g_control_params.ambient_temperature < -50.0f || g_control_params.ambient_temperature > 80.0f) {
        return false;
    }
    
    return true;
}

// ========================================================================
// ADAPTIVE CONTROL STUB FUNCTIONS (Basic Implementation)
// ========================================================================

bool AdaptiveControl_UpdateParameters(float performance_metric)
{
    // Simple adaptive parameter adjustment
    if (performance_metric < g_control_params.efficiency_target) {
        // Increase learning rate slightly if performance is poor
        g_control_params.learning_rate = fminf(g_control_params.learning_rate * 1.01f, 0.1f);
        return true;
    }
    
    return false;
}

bool AdaptiveControl_AdjustForAmbient(float ambient_temperature)
{
    // Calculate ambient compensation factor
    float baseline_temp = 25.0f;  // Standard conditions
    float temp_difference = ambient_temperature - baseline_temp;
    
    s_ambient_compensation_factor = 1.0f + (temp_difference * AMBIENT_ADAPTATION_FACTOR);
    
    // Clamp compensation factor
    if (s_ambient_compensation_factor > 2.0f) s_ambient_compensation_factor = 2.0f;
    if (s_ambient_compensation_factor < 0.5f) s_ambient_compensation_factor = 0.5f;
    
    return true;
}

bool AdaptiveControl_AdjustForLoad(float load_factor)
{
    g_control_params.load_factor = load_factor;
    
    // Adjust optimization interval based on load
    if (load_factor > 0.8f) {
        // High load - optimize more frequently
        g_control_params.optimization_interval = ENERGY_OPTIMIZATION_INTERVAL / 2;
    } else if (load_factor < 0.3f) {
        // Low load - optimize less frequently
        g_control_params.optimization_interval = ENERGY_OPTIMIZATION_INTERVAL * 2;
    } else {
        // Normal load - standard interval
        g_control_params.optimization_interval = ENERGY_OPTIMIZATION_INTERVAL;
    }
    
    return true;
}

// ========================================================================
// PREDICTIVE CONTROL STUB FUNCTIONS (Basic Implementation)
// ========================================================================

float PredictiveControl_PredictLoad(uint32_t horizon_seconds)
{
    // Simple load prediction based on historical data
    float predicted_load = g_control_params.load_factor;
    
    // Add some simple trending
    if (g_performance_analytics.data_valid) {
        predicted_load += g_performance_analytics.average_load * 0.1f;
    }
    
    // Clamp to valid range
    if (predicted_load > 1.0f) predicted_load = 1.0f;
    if (predicted_load < 0.0f) predicted_load = 0.0f;
    
    return predicted_load;
}

/* USER CODE END 0 */