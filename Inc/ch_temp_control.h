/**
 * ========================================================================
 * CHILLER TEMPERATURE CONTROL SYSTEM - HEADER FILE
 * ========================================================================
 * 
 * File: ch_temp_control.h
 * Author: Chiller Control System
 * Version: 1.0
 * Date: September 2025
 * 
 * TEMPERATURE CONTROL OVERVIEW:
 * Advanced PID temperature control system optimized for hot climate 
 * operation (38°C baseline). Provides precise control of return water
 * temperature with intelligent sensor fusion and fault tolerance.
 * 
 * SENSOR CONFIGURATION:
 * - A0: Return Water Temperature (Primary Control)
 * - A1: Supply Water Temperature (Efficiency Monitoring)
 * - A2: Ambient Temperature (Climate Compensation)
 * - A3: Condenser Temperature (Safety Integration)
 * 
 * HOT CLIMATE FEATURES:
 * - Adaptive setpoint control based on ambient temperature
 * - Enhanced PID tuning for extreme conditions
 * - Predictive control for load anticipation
 * - Sensor fault tolerance and graceful degradation
 * 
 * INTEGRATION:
 * - Safety System: Temperature protection and fault handling
 * - Equipment Config: Capacity and mode-based control
 * - Staging System: Equipment control coordination
 * - HMI: Real-time monitoring and setpoint adjustment
 * - Flash: Configuration storage and data logging
 * 
 * ========================================================================
 */

 #ifndef CH_TEMP_CONTROL_H
 #define CH_TEMP_CONTROL_H
 
 #include "main.h"
 #include "ch_safety.h"
 #include "equipment_config.h"
 #include "flash_config.h"
 #include "modbus_sensor.h"
 #include <stdbool.h>
 #include <stdint.h>
 #include <math.h>
 #include <stdlib.h>  // For atof function
 
 // ========================================================================
 // TEMPERATURE CONTROL CONFIGURATION
 // ========================================================================
 
 #define TEMP_CONTROL_MAX_SENSORS        4
 #define TEMP_CONTROL_HISTORY_SIZE       24      // 24 hours of hourly data
 #define TEMP_CONTROL_SAMPLE_RATE_MS     1000    // 1 second sampling
 #define TEMP_CONTROL_PID_RATE_MS        5000    // 5 second PID updates
 
 // Sensor channel mappings
 #define TEMP_SENSOR_RETURN_WATER        0       // A0 - Primary control sensor
 #define TEMP_SENSOR_SUPPLY_WATER        1       // A1 - Efficiency monitoring
 #define TEMP_SENSOR_AMBIENT             2       // A2 - Ambient compensation
 #define TEMP_SENSOR_CONDENSER           3       // A3 - Safety integration
 
 // Temperature setpoint limits (°C) - Hot climate optimized
 #define TEMP_SETPOINT_MIN               4.0f    // Minimum chilled water setpoint
 #define TEMP_SETPOINT_MAX               18.0f   // Maximum chilled water setpoint  
 #define TEMP_SETPOINT_DEFAULT           10.0f   // Default setpoint for 38°C ambient
 #define TEMP_DEADBAND_DEFAULT           0.5f    // Default control deadband
 
 // Hot climate compensation parameters
 #define TEMP_AMBIENT_BASELINE           38.0f   // Baseline ambient temperature
 #define TEMP_AMBIENT_COMPENSATION_MAX   2.0f    // Max setpoint adjustment
 #define TEMP_EFFICIENCY_THRESHOLD       0.75f   // Efficiency warning threshold
 
 // PID Controller parameters (hot climate tuned)
 #define PID_KP_DEFAULT                  2.0f    // Proportional gain
 #define PID_KI_DEFAULT                  0.1f    // Integral gain  
 #define PID_KD_DEFAULT                  0.5f    // Derivative gain
 #define PID_OUTPUT_MIN                  0.0f    // Minimum PID output (0%)
 #define PID_OUTPUT_MAX                  100.0f  // Maximum PID output (100%)
 #define PID_INTEGRAL_MAX                50.0f   // Integral windup limit
 
 // Control timing constants
 #define TEMP_SENSOR_TIMEOUT_MS          10000   // 10 second sensor timeout
 #define TEMP_FAULT_RECOVERY_TIME_MS     30000   // 30 second fault recovery
 #define TEMP_SETPOINT_RAMP_RATE         0.1f    // °C/minute setpoint ramp rate
 
 // ========================================================================
 // TEMPERATURE CONTROL ENUMERATIONS
 // ========================================================================
 
 typedef enum {
     TEMP_CONTROL_MODE_OFF = 0,
     TEMP_CONTROL_MODE_MANUAL,
     TEMP_CONTROL_MODE_AUTO,
     TEMP_CONTROL_MODE_SETPOINT_RAMP,
     TEMP_CONTROL_MODE_FAULT_RECOVERY
 } TempControlMode_t;
 
 typedef enum {
     TEMP_CONTROL_STATE_NORMAL = 0,
     TEMP_CONTROL_STATE_WARNING,
     TEMP_CONTROL_STATE_FAULT,
     TEMP_CONTROL_STATE_EMERGENCY
 } TempControlState_t;
 
 typedef enum {
     TEMP_FAULT_NONE = 0,
     TEMP_FAULT_SENSOR_RETURN_WATER,
     TEMP_FAULT_SENSOR_SUPPLY_WATER,
     TEMP_FAULT_SENSOR_AMBIENT,
     TEMP_FAULT_TEMPERATURE_RANGE,
     TEMP_FAULT_COOLING_EFFICIENCY,
     TEMP_FAULT_PID_SATURATED,
     TEMP_FAULT_SETPOINT_DEVIATION,
     TEMP_FAULT_SYSTEM_OVERLOAD
 } TempFaultType_t;
 
 // ========================================================================
 // TEMPERATURE DATA STRUCTURES
 // ========================================================================
 
 typedef struct {
     float value;                        // Current temperature value
     bool valid;                         // Data validity flag
     uint32_t last_update;              // Last update timestamp
     uint32_t fault_count;              // Fault occurrence counter
     float min_value;                   // Minimum recorded value
     float max_value;                   // Maximum recorded value
     float average;                     // Running average
 } TempSensorData_t;
 
 typedef struct {
     // PID Control parameters
     float kp;                          // Proportional gain
     float ki;                          // Integral gain
     float kd;                          // Derivative gain
     float output_min;                  // Minimum output limit
     float output_max;                  // Maximum output limit
     float integral_max;                // Integral windup limit
     
     // PID State variables
     float setpoint;                    // Current setpoint
     float previous_error;              // Previous error for derivative
     float integral;                    // Integral accumulator
     float output;                      // Current PID output (0-100%)
     uint32_t last_update;             // Last PID update time
     
     // Control performance
     float error_current;               // Current error
     float error_average;               // Average error over time
     float error_maximum;               // Maximum error recorded
     uint32_t settling_time;           // Time to reach setpoint
     
 } TempPID_Controller_t;
 
 typedef struct {
     // Operating setpoints
     float return_water_setpoint;       // Primary control setpoint
     float return_water_deadband;       // Control deadband
     float ambient_compensation;        // Ambient temperature compensation
     
     // Hot climate parameters
     float ambient_baseline;            // Baseline ambient temperature
     float compensation_factor;         // Compensation scaling factor
     bool auto_compensation_enable;     // Enable automatic compensation
     
     // Control timing
     uint16_t sample_rate_ms;          // Temperature sampling rate
     uint16_t pid_rate_ms;             // PID update rate
     uint16_t fault_timeout_ms;        // Sensor fault timeout
     
     // Efficiency monitoring
     float efficiency_threshold;        // Minimum acceptable efficiency
     bool efficiency_monitoring_enable; // Enable efficiency monitoring
     
     // Control modes
     TempControlMode_t control_mode;    // Current control mode
     bool manual_override_enable;       // Manual control override
     float manual_output;               // Manual control output
     
 } TempControlConfig_t;
 
 typedef struct {
     // Sensor data
     TempSensorData_t sensors[TEMP_CONTROL_MAX_SENSORS];
     
     // PID Controller
     TempPID_Controller_t pid;
     
     // System status
     TempControlState_t system_state;
     TempControlMode_t control_mode;
     uint32_t uptime_seconds;
     
     // Performance metrics
     float cooling_efficiency;         // Current cooling efficiency %
     float delta_t;                    // Supply-Return temperature difference
     float ambient_compensation_active; // Current ambient compensation
     
     // Fault management
     TempFaultType_t active_fault;
     uint32_t fault_timestamp;
     uint32_t fault_count;
     char fault_description[64];
     
     // Historical data (hourly averages)
     float return_temp_history[TEMP_CONTROL_HISTORY_SIZE];
     float efficiency_history[TEMP_CONTROL_HISTORY_SIZE];
     uint8_t history_index;
     
     // Timing
     uint32_t last_sample_time;
     uint32_t last_pid_update;
     uint32_t last_hmi_update;
     
 } TempControlData_t;
 
 // ========================================================================
 // GLOBAL VARIABLES (EXTERN DECLARATIONS)
 // ========================================================================
 
 extern TempControlData_t temp_control_data;
 extern TempControlConfig_t temp_control_config;
 
 // ========================================================================
 // FUNCTION PROTOTYPES
 // ========================================================================
 
 // Initialization functions
 bool TempControl_Init(void);
 bool TempControl_LoadConfiguration(void);
 void TempControl_SetDefaultConfiguration(void);
 
 // Main processing functions
 void TempControl_Process(void);
 void TempControl_ProcessSensors(void);
 void TempControl_ProcessPID(void);
 void TempControl_ProcessFaultDetection(void);
 
 // Sensor management
 bool TempControl_ReadSensor(uint8_t sensor_id, float* temperature);
 bool TempControl_ValidateSensorReading(uint8_t sensor_id, float temperature);
 void TempControl_UpdateSensorStatistics(uint8_t sensor_id, float temperature);
 bool TempControl_IsSensorValid(uint8_t sensor_id);
 
 // PID Control functions
 void TempControl_PID_Init(void);
 void TempControl_PID_Reset(void);
 float TempControl_PID_Calculate(float setpoint, float process_value);
 void TempControl_PID_SetTuning(float kp, float ki, float kd);
 void TempControl_PID_SetLimits(float min_output, float max_output);
 
 // Setpoint management
 bool TempControl_SetSetpoint(float new_setpoint);
 float TempControl_GetSetpoint(void);
 void TempControl_RampToSetpoint(float target_setpoint, float ramp_rate);
 float TempControl_CalculateAmbientCompensation(float ambient_temp);
 
 // Control mode functions
 bool TempControl_SetControlMode(TempControlMode_t new_mode);
 TempControlMode_t TempControl_GetControlMode(void);
 bool TempControl_SetManualOutput(float manual_output);
 
 // Performance monitoring
 float TempControl_CalculateEfficiency(void);
 float TempControl_GetDeltaT(void);
 void TempControl_UpdatePerformanceMetrics(void);
 
 // Fault management
 void TempControl_CheckFaults(void);
 bool TempControl_SetFault(TempFaultType_t fault_type, const char* description);
 void TempControl_ClearFault(void);
 bool TempControl_IsFaultActive(void);
 
 // Data access functions
 float TempControl_GetReturnWaterTemp(void);
 float TempControl_GetSupplyWaterTemp(void);
 float TempControl_GetAmbientTemp(void);
 float TempControl_GetPIDOutput(void);
 TempControlState_t TempControl_GetSystemState(void);
 
 // Historical data management
 void TempControl_UpdateHistory(void);
 void TempControl_GetHistoricalData(float* return_temp_array, float* efficiency_array, uint8_t* count);
 
 // Configuration functions
 void TempControl_UpdateConfiguration(TempControlConfig_t* new_config);
 void TempControl_SaveConfiguration(void);
 void TempControl_ResetConfiguration(void);
 
 // HMI integration functions
 void TempControl_UpdateHMI(void);
 void TempControl_ProcessHMICommands(void);
 
 // Hot climate optimization
 void TempControl_ApplyHotClimateCompensation(void);
 void TempControl_AdaptPIDForAmbient(float ambient_temp);
 bool TempControl_IsHotClimateCondition(void);
 
 // Debug and diagnostic functions
 void TempControl_PrintStatus(void);
 void TempControl_PrintSensorData(void);
 void TempControl_PrintPIDStatus(void);
 void TempControl_PrintConfiguration(void);
 void TempControl_RunDiagnostics(void);
 
 // Debug command handlers
 void TempControl_Debug_Status(void);
 void TempControl_Debug_Sensors(void);
 void TempControl_Debug_PID(void);
 void TempControl_Debug_SetSetpoint(float new_setpoint);
 void TempControl_Debug_SetMode(TempControlMode_t mode);
 void TempControl_Debug_Efficiency(void);
 void TempControl_Debug_History(void);
 
 // Utility functions
 const char* TempControl_GetModeDescription(TempControlMode_t mode);
 const char* TempControl_GetStateDescription(TempControlState_t state);
 const char* TempControl_GetFaultDescription(TempFaultType_t fault);
 bool TempControl_IsTemperatureInRange(float temperature, float min_temp, float max_temp);
 
 // Safety integration functions
 bool TempControl_IsSafeToOperate(void);
 void TempControl_HandleSafetyShutdown(void);
 bool TempControl_GetTemperatureForSafety(uint8_t sensor_id, float* temperature);
 
 // ========================================================================
 // TEMPERATURE CONTROL MACROS
 // ========================================================================
 
 #define TEMP_CONTROL_IS_VALID_SETPOINT(setpoint) \
     ((setpoint) >= TEMP_SETPOINT_MIN && (setpoint) <= TEMP_SETPOINT_MAX)
 
 #define TEMP_CONTROL_IS_SENSOR_TIMEOUT(sensor_id) \
     ((HAL_GetTick() - temp_control_data.sensors[sensor_id].last_update) > TEMP_SENSOR_TIMEOUT_MS)
 
 #define TEMP_CONTROL_IS_PID_READY() \
     (temp_control_data.control_mode == TEMP_CONTROL_MODE_AUTO && \
      TempControl_IsSensorValid(TEMP_SENSOR_RETURN_WATER))
 
 #define TEMP_CONTROL_CELSIUS_TO_FAHRENHEIT(celsius) \
     (((celsius) * 9.0f / 5.0f) + 32.0f)
 
 #define TEMP_CONTROL_FAHRENHEIT_TO_CELSIUS(fahrenheit) \
     (((fahrenheit) - 32.0f) * 5.0f / 9.0f)
 
 // Hot climate compensation macros
 #define TEMP_CONTROL_NEEDS_COMPENSATION(ambient) \
     ((ambient) > TEMP_AMBIENT_BASELINE)
 
 #define TEMP_CONTROL_COMPENSATION_FACTOR(ambient) \
     (((ambient) - TEMP_AMBIENT_BASELINE) * 0.05f) // 0.05°C per degree above baseline

// HMI VP Register Definitions for Temperature Control System
#define VP_TEMP_RETURN_WATER        0x2000  // Return water temperature (x10)
#define VP_TEMP_SUPPLY_WATER        0x2001  // Supply water temperature (x10)
#define VP_TEMP_AMBIENT             0x2002  // Ambient temperature (x10)
#define VP_TEMP_SETPOINT            0x2003  // Temperature setpoint (x10)
#define VP_TEMP_PID_OUTPUT          0x2004  // PID controller output (0-100%)
#define VP_TEMP_EFFICIENCY          0x2005  // Cooling efficiency (0-100%)
#define VP_TEMP_DELTA_T             0x2006  // Temperature difference (x10)
#define VP_TEMP_CONTROL_STATE       0x2007  // Control system state
#define VP_TEMP_CONTROL_MODE        0x2008  // Control mode
#define VP_TEMP_FAULT_ACTIVE        0x2009  // Fault active flag
#define VP_TEMP_FAULT_TYPE          0x200A  // Active fault type

void TempControl_SetDebugEnabled(bool enabled);

#endif /* CH_TEMP_CONTROL_H */
 
 /**
  * ========================================================================
  * END OF HEADER FILE
  * ========================================================================
  */