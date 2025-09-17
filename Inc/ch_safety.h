/**
 * ========================================================================
 * CHILLER SAFETY SYSTEM - HEADER FILE
 * ========================================================================
 * 
 * File: ch_safety.h
 * Author: Chiller Control System
 * Version: 1.0
 * Date: September 2025
 * 
 * SAFETY SYSTEM OVERVIEW:
 * This module handles all safety-critical functions for the chiller system
 * including temperature monitoring, pressure interlocks, emergency stops,
 * and comprehensive alarm management.
 * 
 * HOT CLIMATE PROTECTION (38°C Baseline):
 * - Enhanced temperature monitoring for extreme ambient conditions
 * - Adaptive pressure limits based on ambient temperature
 * - Predictive shutdown logic to prevent equipment damage
 * 
 * INTEGRATION:
 * - GPIO Manager: Uses Input_Read() for safety interlocks
 * - Modbus Sensors: Temperature and pressure monitoring
 * - HMI System: Alarm display and user notifications
 * - Flash Storage: Fault history and alarm logging
 * 
 * ========================================================================
 */

#ifndef CH_SAFETY_H
#define CH_SAFETY_H

#include "main.h"
#include "gpio_manager.h"
#include "modbus_sensor.h"
#include "equipment_config.h"
#include "flash_config.h"
#include <stdbool.h>
#include <stdint.h>

// ========================================================================
// SAFETY SYSTEM CONFIGURATION
// ========================================================================

#define SAFETY_MAX_COMPRESSORS          8
#define SAFETY_MAX_CONDENSERS           16
#define SAFETY_MAX_ALARMS               64
#define SAFETY_ALARM_HISTORY_SIZE       100

// Safety check intervals (milliseconds)
#define SAFETY_FAST_CHECK_INTERVAL      100    // Critical checks every 100ms
#define SAFETY_NORMAL_CHECK_INTERVAL    1000   // Normal checks every 1 second
#define SAFETY_SLOW_CHECK_INTERVAL      5000   // Slow checks every 5 seconds

// Temperature limits (°C) - Hot climate optimized for 38°C ambient
#define SAFETY_COMPRESSOR_TEMP_ALARM    80     // Warning at 80°C
#define SAFETY_COMPRESSOR_TEMP_TRIP     85     // Emergency shutdown at 85°C
#define SAFETY_OIL_TEMP_ALARM          65     // Warning at 65°C  
#define SAFETY_OIL_TEMP_TRIP           70     // Emergency shutdown at 70°C
#define SAFETY_RETURN_WATER_MAX        18     // Maximum return water temp
#define SAFETY_AMBIENT_CRITICAL        45     // Critical ambient temp

// Pressure limits (bar) - Adaptive for hot climate
#define SAFETY_HIGH_PRESSURE_ALARM     25     // Base high pressure alarm
#define SAFETY_HIGH_PRESSURE_TRIP      30     // Base high pressure trip
#define SAFETY_LOW_PRESSURE_ALARM      2      // Low pressure alarm
#define SAFETY_LOW_PRESSURE_TRIP       1      // Low pressure trip

// Timing constants
#define SAFETY_ALARM_DELAY_MS          2000   // 2 second delay before alarm
#define SAFETY_TRIP_DELAY_MS           500    // 0.5 second delay before trip
#define SAFETY_RESET_DELAY_MS          5000   // 5 second delay after reset
#define SAFETY_LOCKOUT_TIME_MS         30000  // 30 second lockout after trip

// ========================================================================
// SAFETY ALARM DEFINITIONS
// ========================================================================

typedef enum {
    // System alarms (0-15)
    SAFETY_ALARM_EMERGENCY_STOP = 0,
    SAFETY_ALARM_SYSTEM_FAULT,
    SAFETY_ALARM_COMMUNICATION_FAULT,
    SAFETY_ALARM_POWER_FAULT,
    SAFETY_ALARM_WATER_FLOW_FAULT,
    SAFETY_ALARM_PHASE_LOSS,
    SAFETY_ALARM_OVERLOAD,
    SAFETY_ALARM_GROUND_FAULT,
    
    // Temperature alarms (16-31)
    SAFETY_ALARM_HIGH_AMBIENT_TEMP = 16,
    SAFETY_ALARM_HIGH_RETURN_WATER_TEMP,
    SAFETY_ALARM_LOW_RETURN_WATER_TEMP,
    SAFETY_ALARM_SENSOR_FAULT_TEMP,
    
    // Pressure alarms (32-47)
    SAFETY_ALARM_HIGH_PRESSURE = 32,
    SAFETY_ALARM_LOW_PRESSURE,
    SAFETY_ALARM_SENSOR_FAULT_PRESSURE,
    SAFETY_ALARM_PRESSURE_RATE_CHANGE,
    
    // Compressor specific alarms (48-63) - Can expand per compressor
    SAFETY_ALARM_COMPRESSOR_TEMP_HIGH = 48,
    SAFETY_ALARM_COMPRESSOR_OIL_TEMP_HIGH,
    SAFETY_ALARM_COMPRESSOR_MOTOR_FAULT,
    SAFETY_ALARM_COMPRESSOR_VIBRATION,
    SAFETY_ALARM_COMPRESSOR_CURRENT_HIGH,
    
    SAFETY_ALARM_COUNT = 64
} SafetyAlarmType_t;

typedef enum {
    SAFETY_LEVEL_INFO = 0,      // Information only
    SAFETY_LEVEL_WARNING,       // Warning - log but continue
    SAFETY_LEVEL_ALARM,         // Alarm - reduce capacity but continue
    SAFETY_LEVEL_CRITICAL,      // Critical - immediate shutdown required
    SAFETY_LEVEL_EMERGENCY      // Emergency - immediate system shutdown
} SafetyLevel_t;

typedef enum {
    SAFETY_STATE_NORMAL = 0,
    SAFETY_STATE_WARNING,
    SAFETY_STATE_ALARM,
    SAFETY_STATE_CRITICAL,
    SAFETY_STATE_EMERGENCY,
    SAFETY_STATE_LOCKOUT
} SafetyState_t;

// ========================================================================
// SAFETY DATA STRUCTURES
// ========================================================================

typedef struct {
    SafetyAlarmType_t alarm_id;
    SafetyLevel_t level;
    uint32_t timestamp;
    bool active;
    bool acknowledged;
    uint16_t data;              // Additional alarm data
    char description[64];
} SafetyAlarm_t;

typedef struct {
    // Temperature monitoring
    float compressor_temps[SAFETY_MAX_COMPRESSORS];
    float oil_temps[SAFETY_MAX_COMPRESSORS];
    float return_water_temp;
    float ambient_temp;
    
    // Pressure monitoring  
    float high_pressure;
    float low_pressure;
    
    // Digital input states
    bool emergency_stop;
    bool water_flow_ok;
    bool phase_monitor_ok;
    bool thermal_overload[SAFETY_MAX_COMPRESSORS];
    
    // System status
    SafetyState_t system_state;
    uint32_t fault_count;
    uint32_t trip_count;
    
    // Active alarms
    SafetyAlarm_t active_alarms[SAFETY_MAX_ALARMS];
    uint8_t active_alarm_count;
    
    // Alarm history
    SafetyAlarm_t alarm_history[SAFETY_ALARM_HISTORY_SIZE];
    uint8_t alarm_history_index;
    
    // Timing
    uint32_t last_fast_check;
    uint32_t last_normal_check;
    uint32_t last_slow_check;
    uint32_t lockout_end_time;
    
} Safety_SystemData_t;

typedef struct {
    // Temperature limits - can be adjusted for ambient conditions
    float compressor_temp_alarm_limit;
    float compressor_temp_trip_limit;
    float oil_temp_alarm_limit;
    float oil_temp_trip_limit;
    
    // Pressure limits - adaptive for hot climate
    float high_pressure_alarm_limit;
    float high_pressure_trip_limit;
    float low_pressure_alarm_limit;
    float low_pressure_trip_limit;
    
    // Timing settings
    uint16_t alarm_delay_ms;
    uint16_t trip_delay_ms;
    uint16_t lockout_time_ms;
    
    // Safety features enable/disable
    bool temperature_protection_enable;
    bool pressure_protection_enable;
    bool digital_input_monitoring_enable;
    bool automatic_reset_enable;
    
} Safety_Config_t;

// ========================================================================
// GLOBAL VARIABLES (EXTERN DECLARATIONS)
// ========================================================================

extern Safety_SystemData_t safety_system;
extern Safety_Config_t safety_config;

// ========================================================================
// FUNCTION PROTOTYPES
// ========================================================================

// Initialization functions
bool Safety_Init(void);
bool Safety_LoadConfiguration(void);
void Safety_SetDefaultConfiguration(void);

// Main processing functions
void Safety_Process(void);
void Safety_ProcessFastChecks(void);
void Safety_ProcessNormalChecks(void);
void Safety_ProcessSlowChecks(void);

// Temperature monitoring
void Safety_CheckTemperatures(void);
bool Safety_CheckCompressorTemperature(uint8_t compressor_id);
bool Safety_CheckOilTemperature(uint8_t compressor_id);
bool Safety_CheckReturnWaterTemperature(void);
bool Safety_CheckAmbientTemperature(void);

// Pressure monitoring  
void Safety_CheckPressures(void);
bool Safety_CheckHighPressure(void);
bool Safety_CheckLowPressure(void);
void Safety_AdaptPressureLimitsForAmbient(float ambient_temp);

// Digital input monitoring
void Safety_CheckDigitalInputs(void);
bool Safety_CheckEmergencyStop(void);
bool Safety_CheckWaterFlow(void);
bool Safety_CheckPhaseMonitor(void);
bool Safety_CheckThermalOverloads(void);

// Alarm management
bool Safety_SetAlarm(SafetyAlarmType_t alarm_type, SafetyLevel_t level, const char* description);
bool Safety_ClearAlarm(SafetyAlarmType_t alarm_type);
void Safety_ClearAllAlarms(void);
bool Safety_AcknowledgeAlarm(SafetyAlarmType_t alarm_type);
void Safety_AcknowledgeAllAlarms(void);

// System control
void Safety_EmergencyStop(const char* reason);
void Safety_SystemShutdown(const char* reason);
bool Safety_SystemReset(void);
bool Safety_IsSystemLocked(void);
bool Safety_CanStartCompressor(uint8_t compressor_id);

// Data access functions
SafetyState_t Safety_GetSystemState(void);
uint8_t Safety_GetActiveAlarmCount(void);
SafetyAlarm_t* Safety_GetActiveAlarms(void);
SafetyAlarm_t* Safety_GetAlarmHistory(void);
bool Safety_GetAlarmStatus(SafetyAlarmType_t alarm_type);

// Configuration functions
void Safety_UpdateConfiguration(Safety_Config_t* new_config);
void Safety_SaveConfiguration(void);
void Safety_ResetConfiguration(void);

// Logging functions
void Safety_LogAlarmToFlash(SafetyAlarm_t* alarm);
void Safety_LogSystemEvent(const char* event, SafetyLevel_t level);

// HMI integration functions
void Safety_UpdateHMI(void);
void Safety_ProcessHMICommands(void);

// Debug and diagnostic functions
void Safety_PrintSystemStatus(void);
void Safety_PrintActiveAlarms(void);
void Safety_PrintAlarmHistory(void);
void Safety_PrintConfiguration(void);
void Safety_RunDiagnostics(void);
bool Safety_TestSafetyInputs(void);

// Debug command handlers
void Safety_Debug_Status(void);
void Safety_Debug_Alarms(void);
void Safety_Debug_Test(void);
void Safety_Debug_Reset(void);
void Safety_Debug_Config(void);
void Safety_Debug_History(void);

// Utility functions
const char* Safety_GetAlarmDescription(SafetyAlarmType_t alarm_type);
const char* Safety_GetLevelDescription(SafetyLevel_t level);
const char* Safety_GetStateDescription(SafetyState_t state);
uint32_t Safety_GetUptimeSeconds(void);

// ========================================================================
// SAFETY MACROS
// ========================================================================

#define SAFETY_IS_ALARM_ACTIVE(alarm_type) \
    Safety_GetAlarmStatus(alarm_type)

#define SAFETY_IS_SYSTEM_SAFE() \
    (safety_system.system_state <= SAFETY_STATE_WARNING)

#define SAFETY_IS_EMERGENCY_CONDITION() \
    (safety_system.system_state == SAFETY_STATE_EMERGENCY)

#define SAFETY_CAN_OPERATE() \
    (safety_system.system_state <= SAFETY_STATE_ALARM && !Safety_IsSystemLocked())

// Temperature check macros for hot climate
#define SAFETY_TEMP_WITHIN_LIMITS(temp, limit) \
    ((temp) < (limit))

#define SAFETY_PRESSURE_WITHIN_LIMITS(pressure, low_limit, high_limit) \
    (((pressure) > (low_limit)) && ((pressure) < (high_limit)))

#endif /* CH_SAFETY_H */

/**
 * ========================================================================
 * END OF HEADER FILE
 * ========================================================================
 */
