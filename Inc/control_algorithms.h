/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    control_algorithms.h
  * @brief   This file contains all the function prototypes for
  *          the control_algorithms.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CONTROL_ALGORITHMS_H__
#define __CONTROL_ALGORITHMS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */

// PID Controller constants
#define PID_MAX_CONTROLLERS         4       // Maximum PID controllers
#define PID_HISTORY_SIZE           10       // History buffer size
#define PID_OUTPUT_MIN             0.0f     // Minimum PID output
#define PID_OUTPUT_MAX             100.0f   // Maximum PID output
#define PID_DERIVATIVE_FILTER      0.1f     // Derivative filtering factor

// Control algorithm types
typedef enum {
    CONTROL_ALGORITHM_BASIC_PID = 0,
    CONTROL_ALGORITHM_ADAPTIVE_PID,
    CONTROL_ALGORITHM_PREDICTIVE,
    CONTROL_ALGORITHM_FUZZY_LOGIC
} ControlAlgorithmType_t;

// PID Controller structure
typedef struct {
    float kp;                           // Proportional gain
    float ki;                           // Integral gain
    float kd;                           // Derivative gain
    float setpoint;                     // Desired setpoint
    float previous_error;               // Previous error value
    float integral;                     // Integral accumulator
    float output;                       // Controller output
    float output_min;                   // Minimum output limit
    float output_max;                   // Maximum output limit
    uint32_t last_update_time;          // Last update timestamp
    bool enabled;                       // Controller enabled flag
} PIDController_t;

// Advanced control parameters
typedef struct {
    ControlAlgorithmType_t algorithm_type;  // Selected algorithm type
    float ambient_temperature;             // Current ambient temperature
    float load_factor;                     // Current system load factor
    float efficiency_target;               // Target efficiency percentage
    bool adaptive_tuning_enabled;          // Adaptive tuning feature
    bool optimization_enabled;             // System optimization feature
    uint32_t optimization_interval;        // Optimization update interval
} AdvancedControlParams_t;

// System optimization data
typedef struct {
    float current_efficiency;              // Current system efficiency
    float target_efficiency;               // Target efficiency goal
    float power_consumption;               // Total power consumption
    float cooling_capacity;                // Total cooling capacity
    float optimization_score;              // Overall optimization score
    uint32_t optimization_cycles;          // Number of optimization cycles
    bool optimization_active;              // Optimization process active
} SystemOptimization_t;

/* USER CODE END Private defines */

/* USER CODE BEGIN Prototypes */

// Initialization functions
bool ControlAlgorithms_Init(void);
bool ControlAlgorithms_LoadConfiguration(void);
bool ControlAlgorithms_SaveConfiguration(void);

// PID Controller functions
bool PID_Init(PIDController_t* pid, float kp, float ki, float kd);
float PID_Update(PIDController_t* pid, float process_value, float setpoint);
void PID_SetTunings(PIDController_t* pid, float kp, float ki, float kd);
void PID_SetOutputLimits(PIDController_t* pid, float min, float max);
void PID_Reset(PIDController_t* pid);
void PID_Enable(PIDController_t* pid, bool enabled);

// Advanced control functions
bool ControlAlgorithms_Process(void);
float ControlAlgorithms_CalculateOptimalOutput(float process_value, float setpoint);
bool ControlAlgorithms_SetAlgorithmType(ControlAlgorithmType_t type);

// Optimization functions
bool SystemOptimization_Process(void);
float SystemOptimization_CalculateEfficiency(void);
void SystemOptimization_UpdateMetrics(float efficiency, float power, float capacity);

// Configuration functions
bool ControlAlgorithms_SetEfficiencyTarget(float target);
void ControlAlgorithms_SetAmbientTemperature(float temperature);
void ControlAlgorithms_SetLoadFactor(float load_factor);

// Status and monitoring functions
AdvancedControlParams_t* ControlAlgorithms_GetParameters(void);
SystemOptimization_t* ControlAlgorithms_GetOptimizationData(void);
float ControlAlgorithms_GetCurrentEfficiency(void);

// Debug functions
void ControlAlgorithms_SetDebugEnabled(bool enabled);
void ControlAlgorithms_PrintStatus(void);
bool ControlAlgorithms_RunDiagnostics(void);

// Utility functions
float ControlAlgorithms_LimitOutput(float output, float min, float max);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __CONTROL_ALGORITHMS_H__ */