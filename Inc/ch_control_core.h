/**
  ******************************************************************************
  * @file    ch_control_core.h
  * @brief   Core Control System for Industrial Chiller Management
  * @author  Industrial Chiller Control System
  * @version 1.0
  * @date    2025
  * 
  * @description
  * Main system coordinator for industrial chiller control system.
  * Manages system states, coordinates all subsystems, provides four-tier
  * capacity control, and integrates with all hardware and software components.
  * 
  * Features:
  * - System state management (Off/Starting/Running/Stopping/Fault/Maintenance)
  * - Four-tier capacity control coordination (Economic/Normal/Full/Custom)
  * - Integration with all existing subsystems (GPIO, Modbus, HMI, Flash, etc.)
  * - Performance monitoring and diagnostics
  * - Automatic mode switching based on load conditions
  * - Hot climate optimization (38°C baseline operation)
  * - Comprehensive fault detection and recovery
  ******************************************************************************
  */

#ifndef CH_CONTROL_CORE_H
#define CH_CONTROL_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "equipment_config.h"
#include "flash_config.h"
#include "gpio_manager.h"
#include "modbus_sensor.h"
#include "hmi.h"
#include "uart_comm.h"
#include <stdint.h>
#include <string.h>

/* Core Control Constants ----------------------------------------------------*/
#define CH_CONTROL_VERSION              0x0100      // Version 1.0
#define CH_CONTROL_LOOP_PERIOD          100         // Main loop period (ms)
#define CH_CONTROL_STARTUP_DELAY        5000        // System startup delay (ms)
#define CH_CONTROL_SHUTDOWN_DELAY       3000        // System shutdown delay (ms)
#define CH_CONTROL_FAULT_RETRY_DELAY    30000       // Fault retry delay (ms)

/* System Performance Monitoring --------------------------------------------*/
#define CH_PERFORMANCE_HISTORY_SIZE     60          // Store 60 samples (6 minutes at 6s intervals)
#define CH_PERFORMANCE_SAMPLE_INTERVAL  6000        // Performance sampling interval (ms)
#define CH_EFFICIENCY_CALC_INTERVAL     300000      // Efficiency calculation interval (5 minutes)

/* System State Timeouts -----------------------------------------------------*/
#define CH_STATE_TIMEOUT_STARTING       30000       // Starting state timeout (30 seconds)
#define CH_STATE_TIMEOUT_STOPPING       15000       // Stopping state timeout (15 seconds) 
#define CH_STATE_TIMEOUT_FAULT_CLEAR     60000       // Fault clear timeout (60 seconds)

/* Data Types ----------------------------------------------------------------*/

/**
 * @brief Chiller System States
 */
typedef enum {
    CH_STATE_OFF = 0,                   // System completely off
    CH_STATE_STARTING,                  // System startup sequence
    CH_STATE_RUNNING,                   // Normal operation
    CH_STATE_STOPPING,                  // System shutdown sequence
    CH_STATE_FAULT,                     // System fault condition
    CH_STATE_MAINTENANCE,               // Maintenance mode
    CH_STATE_EMERGENCY_STOP             // Emergency stop condition
} ChillerSystemState_t;

/**
 * @brief System Control Commands
 */
typedef enum {
    CH_CMD_NONE = 0,                    // No command
    CH_CMD_START,                       // Start system
    CH_CMD_STOP,                        // Stop system
    CH_CMD_EMERGENCY_STOP,              // Emergency stop
    CH_CMD_RESET_FAULTS,                // Reset fault conditions
    CH_CMD_ENTER_MAINTENANCE,           // Enter maintenance mode
    CH_CMD_EXIT_MAINTENANCE,            // Exit maintenance mode
    CH_CMD_AUTO_MODE,                   // Enable automatic mode
    CH_CMD_MANUAL_MODE                  // Enable manual mode
} ChillerSystemCommand_t;

/**
 * @brief System Fault Codes
 */
typedef enum {
    CH_FAULT_NONE = 0x0000,            // No faults
    CH_FAULT_SUPPLY_TEMP_HIGH = 0x0001, // Supply temperature too high
    CH_FAULT_SUPPLY_TEMP_LOW = 0x0002,  // Supply temperature too low
    CH_FAULT_RETURN_TEMP_HIGH = 0x0004, // Return temperature too high
    CH_FAULT_AMBIENT_TEMP_HIGH = 0x0008, // Ambient temperature too high
    CH_FAULT_FLOW_LOSS = 0x0010,        // Water flow loss
    CH_FAULT_PRESSURE_HIGH = 0x0020,    // System pressure too high
    CH_FAULT_PRESSURE_LOW = 0x0040,     // System pressure too low
    CH_FAULT_COMPRESSOR_FAIL = 0x0080,  // Compressor failure
    CH_FAULT_CONDENSER_FAIL = 0x0100,   // Condenser failure
    CH_FAULT_SENSOR_FAULT = 0x0200,     // Sensor communication fault
    CH_FAULT_POWER_PHASE_LOSS = 0x0400, // Power phase loss
    CH_FAULT_EMERGENCY_STOP = 0x0800,   // Emergency stop activated
    CH_FAULT_SYSTEM_OVERLOAD = 0x1000,  // System overload condition
    CH_FAULT_COMMUNICATION = 0x2000,    // Communication fault
    CH_FAULT_CONFIGURATION = 0x4000,    // Configuration error
    CH_FAULT_CRITICAL_SYSTEM = 0x8000   // Critical system fault
} ChillerFaultCode_t;

/**
 * @brief System Performance Data
 */
typedef struct {
    uint32_t timestamp;                 // Performance sample timestamp
    float supply_temperature;           // Supply water temperature (°C)
    float return_temperature;           // Return water temperature (°C)
    float ambient_temperature;          // Ambient temperature (°C)
    float temperature_delta;            // Return - Supply temperature delta
    uint16_t system_pressure;           // System pressure
    uint16_t flow_rate;                 // Water flow rate
    uint8_t active_compressors;         // Number of active compressors
    uint8_t active_condensers;          // Number of active condensers
    float system_efficiency;            // System efficiency (%)
    float power_consumption;            // Estimated power consumption
    CapacityMode_t current_mode;        // Current capacity mode
} ChillerPerformanceData_t;

/**
 * @brief System Status Information
 */
typedef struct {
    ChillerSystemState_t current_state;     // Current system state
    ChillerSystemState_t previous_state;    // Previous system state
    uint32_t state_enter_time;              // Time when current state was entered
    uint32_t state_duration;                // Duration in current state
    uint32_t total_run_time;                // Total system runtime
    uint32_t state_change_count;            // Number of state changes
    
    CapacityMode_t current_capacity_mode;   // Current capacity mode
    uint8_t auto_mode_enabled;              // Automatic mode control enabled
    uint8_t manual_override_active;         // Manual override is active
    
    ChillerFaultCode_t active_faults;       // Active fault codes (bit field)
    ChillerFaultCode_t fault_history;       // Fault history (bit field)
    uint32_t fault_count;                   // Total fault occurrences
    uint32_t last_fault_time;               // Time of last fault
    
    uint8_t system_ready;                   // System ready for operation
    uint8_t safety_interlocks_ok;           // Safety interlocks status
    uint8_t sensors_ok;                     // Sensor system status
    uint8_t communication_ok;               // Communication system status
    
    float current_load_demand;              // Current cooling load demand (%)
    float average_load_demand;              // Average load demand over time (%)
    float peak_load_demand;                 // Peak load demand recorded (%)
    
} ChillerSystemStatus_t;

/**
 * @brief Core Control System Structure
 */
typedef struct {
    // System Status
    uint8_t initialized;                    // System initialized flag
    uint8_t enabled;                        // System enabled flag
    ChillerSystemStatus_t status;           // System status information
    
    // Control Parameters
    ChillerSystemCommand_t pending_command; // Pending system command
    uint32_t command_timeout;               // Command execution timeout
    uint32_t last_process_time;             // Last main process execution time
    
    // Performance Monitoring
    ChillerPerformanceData_t performance_history[CH_PERFORMANCE_HISTORY_SIZE];
    uint16_t performance_index;             // Current performance data index
    uint16_t performance_count;             // Number of valid performance samples
    uint32_t last_performance_sample;       // Last performance sample time
    
    // System Statistics
    uint32_t successful_starts;            // Successful system starts
    uint32_t failed_starts;                // Failed system starts
    uint32_t emergency_stops;              // Emergency stop count
    uint32_t automatic_recoveries;         // Automatic fault recoveries
    uint32_t manual_interventions;         // Manual intervention count
    
    // Integration Status
    uint8_t gpio_manager_ok;               // GPIO manager status
    uint8_t modbus_system_ok;              // Modbus system status
    uint8_t hmi_system_ok;                 // HMI system status
    uint8_t flash_config_ok;               // Flash configuration status
    uint8_t equipment_config_ok;           // Equipment configuration status
    
} ChillerControlCore_t;

/* Global Variables ----------------------------------------------------------*/
extern ChillerControlCore_t g_chiller_core;
extern uint8_t g_chiller_core_initialized;

/* Function Prototypes -------------------------------------------------------*/

/* === INITIALIZATION AND SHUTDOWN === */
ChillerFaultCode_t ChillerCore_Init(void);
ChillerFaultCode_t ChillerCore_Shutdown(void);
ChillerFaultCode_t ChillerCore_Reset(void);

/* === MAIN SYSTEM CONTROL === */
void ChillerCore_Process(void);
ChillerFaultCode_t ChillerCore_ExecuteCommand(ChillerSystemCommand_t command);
ChillerFaultCode_t ChillerCore_SetCapacityMode(CapacityMode_t mode);
void ChillerCore_UpdateSystemStatus(void);

/* === STATE MACHINE MANAGEMENT === */
void ChillerCore_StateMachine(void);
ChillerFaultCode_t ChillerCore_ChangeState(ChillerSystemState_t new_state);
void ChillerCore_ProcessStateTimeout(void);
const char* ChillerCore_GetStateName(ChillerSystemState_t state);

/* === SYSTEM CONTROL COMMANDS === */
ChillerFaultCode_t ChillerCore_StartSystem(void);
ChillerFaultCode_t ChillerCore_StopSystem(void);
ChillerFaultCode_t ChillerCore_EmergencyStop(void);
ChillerFaultCode_t ChillerCore_ResetFaults(void);
ChillerFaultCode_t ChillerCore_EnterMaintenanceMode(void);
ChillerFaultCode_t ChillerCore_ExitMaintenanceMode(void);

/* === AUTOMATIC CONTROL === */
void ChillerCore_AutoModeControl(void);
CapacityMode_t ChillerCore_DetermineOptimalMode(void);
void ChillerCore_UpdateLoadDemand(void);
uint8_t ChillerCore_ShouldChangeMode(CapacityMode_t suggested_mode);

/* === FAULT DETECTION AND HANDLING === */
ChillerFaultCode_t ChillerCore_CheckSystemFaults(void);
ChillerFaultCode_t ChillerCore_CheckTemperatureFaults(void);
ChillerFaultCode_t ChillerCore_CheckPressureFlowFaults(void);
ChillerFaultCode_t ChillerCore_CheckEquipmentFaults(void);
void ChillerCore_ProcessFaults(ChillerFaultCode_t faults);
void ChillerCore_LogFault(ChillerFaultCode_t fault_code, const char* description);

/* === PERFORMANCE MONITORING === */
void ChillerCore_UpdatePerformanceData(void);
void ChillerCore_CalculateSystemEfficiency(void);
void ChillerCore_UpdateSystemStatistics(void);
void ChillerCore_GetPerformanceStats(float* avg_efficiency, float* avg_load, float* uptime_percent);

/* === SUBSYSTEM INTEGRATION === */
ChillerFaultCode_t ChillerCore_InitializeSubsystems(void);
void ChillerCore_CheckSubsystemStatus(void);
void ChillerCore_SynchronizeWithEquipmentConfig(void);
void ChillerCore_UpdateFlashConfiguration(void);
void ChillerCore_UpdateHMIRegisters(void);

/* === SYSTEM INFORMATION FUNCTIONS === */
ChillerSystemState_t ChillerCore_GetSystemState(void);
ChillerFaultCode_t ChillerCore_GetActiveFaults(void);
CapacityMode_t ChillerCore_GetCurrentMode(void);
float ChillerCore_GetCurrentLoadDemand(void);
uint32_t ChillerCore_GetSystemRuntime(void);
void ChillerCore_GetSystemStatus(ChillerSystemStatus_t* status);

/* === DIAGNOSTIC AND DEBUG FUNCTIONS === */
void ChillerCore_DisplaySystemStatus(void);
void ChillerCore_DisplayPerformanceData(void);
void ChillerCore_DisplayFaultHistory(void);
void ChillerCore_DisplayStatistics(void);
void ChillerCore_RunSystemDiagnostics(void);

/* === DEBUG COMMAND INTERFACE === */
void ChillerCore_ProcessDebugCommand(const char* command);
void ChillerCore_ShowDebugCommands(void);

/* === UTILITY FUNCTIONS === */
uint8_t ChillerCore_IsSystemRunning(void);
uint8_t ChillerCore_IsSystemReady(void);
uint8_t ChillerCore_IsSystemFaulted(void);
uint32_t ChillerCore_GetSystemUptime(void);
float ChillerCore_GetSystemEfficiency(void);

#ifdef __cplusplus
}
#endif

#endif /* CH_CONTROL_CORE_H */
