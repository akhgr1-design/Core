/**
  ******************************************************************************
  * @file    ch_control_core.c
  * @brief   Core Control System Implementation
  * @author  Industrial Chiller Control System
  * @version 1.0
  * @date    2025
  ******************************************************************************
  */

#include "ch_control_core.h"
#include <stdio.h>
#include <math.h>

/* Private Constants ---------------------------------------------------------*/
#define LOAD_DEMAND_SMOOTHING_FACTOR    0.1f        // Load demand smoothing factor
#define EFFICIENCY_SMOOTHING_FACTOR     0.05f       // Efficiency calculation smoothing
#define TEMPERATURE_DELTA_TARGET        5.0f        // Target temperature delta (°C)
#define FAULT_RETRY_MAX_ATTEMPTS        3           // Maximum fault retry attempts

/* Private Variables ---------------------------------------------------------*/
static uint32_t system_start_time = 0;
static uint32_t last_state_change_time = 0;
static uint8_t fault_retry_count = 0;
static float load_demand_filtered = 0.0f;
static float efficiency_filtered = 0.0f;

/* Global Core Control System -----------------------------------------------*/
ChillerControlCore_t g_chiller_core = {0};
uint8_t g_chiller_core_initialized = 0;

/* Private Function Prototypes -----------------------------------------------*/
static void ChillerCore_InitializeSystemStatus(void);
static void ChillerCore_CheckSystemHealth(void);
static float ChillerCore_CalculateLoadDemand(void);
static void ChillerCore_ProcessStateTransition(ChillerSystemState_t new_state);
static void ChillerCore_HandleFaultRecovery(void);

/* === INITIALIZATION AND SHUTDOWN === */

/**
 * @brief Initialize Chiller Control Core System
 */
ChillerFaultCode_t ChillerCore_Init(void)
{
    Send_Debug_Data("=== CHILLER CORE INITIALIZATION ===\r\n");
    
    // Initialize core system structure
    memset(&g_chiller_core, 0, sizeof(ChillerControlCore_t));
    
    // Initialize system status
    ChillerCore_InitializeSystemStatus();
    
    // Initialize subsystems
    ChillerFaultCode_t result = ChillerCore_InitializeSubsystems();
    if (result != CH_FAULT_NONE) {
        Send_Debug_Data("Chiller Core: Subsystem initialization failed\r\n");
        return result;
    }
    
    // Synchronize with equipment configuration
    ChillerCore_SynchronizeWithEquipmentConfig();
    
    // Set initial state
    g_chiller_core.status.current_state = CH_STATE_OFF;
    g_chiller_core.status.current_capacity_mode = g_equipment_config.current_mode;
    
    // Mark system as initialized
    g_chiller_core.initialized = 1;
    g_chiller_core_initialized = 1;
    system_start_time = HAL_GetTick();
    
    Send_Debug_Data("Chiller Core: System initialized successfully\r\n");
    ChillerCore_DisplaySystemStatus();
    
    return CH_FAULT_NONE;
}

/**
 * @brief Initialize System Status Structure
 */
static void ChillerCore_InitializeSystemStatus(void)
{
    ChillerSystemStatus_t* status = &g_chiller_core.status;
    
    status->current_state = CH_STATE_OFF;
    status->previous_state = CH_STATE_OFF;
    status->state_enter_time = HAL_GetTick();
    status->current_capacity_mode = CAPACITY_MODE_NORMAL;
    status->auto_mode_enabled = 1;
    status->system_ready = 0;
    status->active_faults = CH_FAULT_NONE;
    status->fault_history = CH_FAULT_NONE;
    
    // Initialize performance tracking
    g_chiller_core.performance_index = 0;
    g_chiller_core.performance_count = 0;
    g_chiller_core.last_performance_sample = HAL_GetTick();
}

/**
 * @brief Initialize and Check Subsystems
 */
ChillerFaultCode_t ChillerCore_InitializeSubsystems(void)
{
    ChillerFaultCode_t faults = CH_FAULT_NONE;
    
    // Check GPIO Manager
    g_chiller_core.gpio_manager_ok = 1; // Assume initialized from main
    
    // Check Modbus System
    g_chiller_core.modbus_system_ok = Modbus_System_IsEnabled();
    
    // Check HMI System
    g_chiller_core.hmi_system_ok = HMI_Is_Initialized();
    
    // Check Flash Configuration
    g_chiller_core.flash_config_ok = g_flash_config_initialized;
    
    // Check Equipment Configuration
    g_chiller_core.equipment_config_ok = g_config_initialized;
    
    // Verify critical subsystems
    if (!g_chiller_core.equipment_config_ok) {
        faults |= CH_FAULT_CONFIGURATION;
    }
    
    if (!g_chiller_core.gpio_manager_ok) {
        faults |= CH_FAULT_CRITICAL_SYSTEM;
    }
    
    return faults;
}

/* === MAIN SYSTEM CONTROL === */

/**
 * @brief Main System Process Function
 */
void ChillerCore_Process(void)
{
    if (!g_chiller_core_initialized) return;
    
    uint32_t current_time = HAL_GetTick();
    g_chiller_core.last_process_time = current_time;
    
    // Update system status
    ChillerCore_UpdateSystemStatus();
    
    // Check for system faults
    ChillerFaultCode_t current_faults = ChillerCore_CheckSystemFaults();
    if (current_faults != CH_FAULT_NONE) {
        ChillerCore_ProcessFaults(current_faults);
    }
    
    // Execute pending commands
    if (g_chiller_core.pending_command != CH_CMD_NONE) {
        ChillerCore_ExecuteCommand(g_chiller_core.pending_command);
        g_chiller_core.pending_command = CH_CMD_NONE;
    }
    
    // Run state machine
    ChillerCore_StateMachine();
    
    // Update performance monitoring
    if ((current_time - g_chiller_core.last_performance_sample) >= CH_PERFORMANCE_SAMPLE_INTERVAL) {
        ChillerCore_UpdatePerformanceData();
        g_chiller_core.last_performance_sample = current_time;
    }
    
    // Auto mode control
    if (g_chiller_core.status.auto_mode_enabled && 
        g_chiller_core.status.current_state == CH_STATE_RUNNING) {
        ChillerCore_AutoModeControl();
    }
    
    // Update subsystem integrations
    ChillerCore_CheckSubsystemStatus();
    ChillerCore_UpdateHMIRegisters();
}

/**
 * @brief Execute System Command
 */
ChillerFaultCode_t ChillerCore_ExecuteCommand(ChillerSystemCommand_t command)
{
    ChillerFaultCode_t result = CH_FAULT_NONE;
    
    switch (command) {
        case CH_CMD_START:
            result = ChillerCore_StartSystem();
            break;
            
        case CH_CMD_STOP:
            result = ChillerCore_StopSystem();
            break;
            
        case CH_CMD_EMERGENCY_STOP:
            result = ChillerCore_EmergencyStop();
            break;
            
        case CH_CMD_RESET_FAULTS:
            result = ChillerCore_ResetFaults();
            break;
            
        case CH_CMD_ENTER_MAINTENANCE:
            result = ChillerCore_EnterMaintenanceMode();
            break;
            
        case CH_CMD_EXIT_MAINTENANCE:
            result = ChillerCore_ExitMaintenanceMode();
            break;
            
        case CH_CMD_AUTO_MODE:
            g_chiller_core.status.auto_mode_enabled = 1;
            g_chiller_core.status.manual_override_active = 0;
            Send_Debug_Data("Chiller Core: Auto mode enabled\r\n");
            break;
            
        case CH_CMD_MANUAL_MODE:
            g_chiller_core.status.auto_mode_enabled = 0;
            g_chiller_core.status.manual_override_active = 1;
            Send_Debug_Data("Chiller Core: Manual mode enabled\r\n");
            break;
            
        default:
            result = CH_FAULT_CONFIGURATION;
            break;
    }
    
    return result;
}

/* === STATE MACHINE MANAGEMENT === */

/**
 * @brief Main State Machine
 */
void ChillerCore_StateMachine(void)
{
    ChillerSystemState_t current_state = g_chiller_core.status.current_state;
    uint32_t state_duration = HAL_GetTick() - g_chiller_core.status.state_enter_time;
    
    switch (current_state) {
        case CH_STATE_OFF:
            // System is off, ready for start command
            g_chiller_core.status.system_ready = (g_chiller_core.status.active_faults == CH_FAULT_NONE);
            break;
            
        case CH_STATE_STARTING:
            // System startup sequence
            if (state_duration >= CH_CONTROL_STARTUP_DELAY) {
                if (g_chiller_core.status.active_faults == CH_FAULT_NONE) {
                    ChillerCore_ChangeState(CH_STATE_RUNNING);
                    g_chiller_core.successful_starts++;
                    Send_Debug_Data("Chiller Core: System start successful\r\n");
                } else {
                    ChillerCore_ChangeState(CH_STATE_FAULT);
                    g_chiller_core.failed_starts++;
                    Send_Debug_Data("Chiller Core: System start failed - faults detected\r\n");
                }
            }
            break;
            
        case CH_STATE_RUNNING:
            // Normal operation - monitor for faults
            if (g_chiller_core.status.active_faults != CH_FAULT_NONE) {
                ChillerCore_ChangeState(CH_STATE_FAULT);
            }
            g_chiller_core.status.total_run_time = state_duration;
            break;
            
        case CH_STATE_STOPPING:
            // System shutdown sequence
            if (state_duration >= CH_CONTROL_SHUTDOWN_DELAY) {
                ChillerCore_ChangeState(CH_STATE_OFF);
                Send_Debug_Data("Chiller Core: System stop complete\r\n");
            }
            break;
            
        case CH_STATE_FAULT:
            // Fault condition - attempt recovery if configured
            ChillerCore_HandleFaultRecovery();
            break;
            
        case CH_STATE_MAINTENANCE:
            // Maintenance mode - limited operation
            break;
            
        case CH_STATE_EMERGENCY_STOP:
            // Emergency stop - manual intervention required
            break;
    }
    
    // Update state duration
    g_chiller_core.status.state_duration = state_duration;
}

/**
 * @brief Change System State
 */
ChillerFaultCode_t ChillerCore_ChangeState(ChillerSystemState_t new_state)
{
    ChillerSystemState_t old_state = g_chiller_core.status.current_state;
    
    if (old_state == new_state) {
        return CH_FAULT_NONE; // No change needed
    }
    
    // Process state transition
    ChillerCore_ProcessStateTransition(new_state);
    
    // Update state information
    g_chiller_core.status.previous_state = old_state;
    g_chiller_core.status.current_state = new_state;
    g_chiller_core.status.state_enter_time = HAL_GetTick();
    g_chiller_core.status.state_change_count++;
    last_state_change_time = HAL_GetTick();
    
    // Log state change
    char msg[100];
    snprintf(msg, sizeof(msg), "Chiller Core: State changed from %s to %s\r\n",
             ChillerCore_GetStateName(old_state), ChillerCore_GetStateName(new_state));
    Send_Debug_Data(msg);
    
    // Log to flash configuration
    FlashConfig_LogAlarm(0x1000 + new_state, 1, 0, 0.0f, "System state change");
    
    return CH_FAULT_NONE;
}

/**
 * @brief Process State Transition Actions
 */
static void ChillerCore_ProcessStateTransition(ChillerSystemState_t new_state)
{
    switch (new_state) {
        case CH_STATE_OFF:
            // Turn off all equipment
            for (int i = 0; i < MAX_COMPRESSORS; i++) {
                Relay_Set(i, 0);  // Turn off compressor relays
            }
            for (int i = 0; i < MAX_CONDENSER_BANKS; i++) {
                Relay_Set(i + 8, 0);  // Turn off condenser relays (Q1.x)
            }
            break;
            
        case CH_STATE_STARTING:
            // Pre-start checks and preparations
            Send_Debug_Data("Chiller Core: Starting system...\r\n");
            break;
            
        case CH_STATE_RUNNING:
            // System is now running
            break;
            
        case CH_STATE_STOPPING:
            // Orderly shutdown sequence
            Send_Debug_Data("Chiller Core: Stopping system...\r\n");
            break;
            
        case CH_STATE_FAULT:
            // Fault handling
            g_chiller_core.status.fault_count++;
            g_chiller_core.status.last_fault_time = HAL_GetTick();
            break;
            
        case CH_STATE_EMERGENCY_STOP:
            // Immediate shutdown
            g_chiller_core.emergency_stops++;
            for (int i = 0; i < 16; i++) {
                Relay_Set(i, 0);  // Turn off all relays immediately
            }
            break;
            
        default:
            break;
    }
}

/**
 * @brief Get State Name String
 */
const char* ChillerCore_GetStateName(ChillerSystemState_t state)
{
    switch (state) {
        case CH_STATE_OFF:           return "OFF";
        case CH_STATE_STARTING:      return "STARTING";
        case CH_STATE_RUNNING:       return "RUNNING";
        case CH_STATE_STOPPING:      return "STOPPING";
        case CH_STATE_FAULT:         return "FAULT";
        case CH_STATE_MAINTENANCE:   return "MAINTENANCE";
        case CH_STATE_EMERGENCY_STOP: return "EMERGENCY_STOP";
        default:                     return "UNKNOWN";
    }
}

/* === SYSTEM CONTROL COMMANDS === */

/**
 * @brief Start System
 */
ChillerFaultCode_t ChillerCore_StartSystem(void)
{
    if (g_chiller_core.status.current_state != CH_STATE_OFF) {
        Send_Debug_Data("Chiller Core: Cannot start - system not in OFF state\r\n");
        return CH_FAULT_CONFIGURATION;
    }
    
    if (g_chiller_core.status.active_faults != CH_FAULT_NONE) {
        Send_Debug_Data("Chiller Core: Cannot start - active faults present\r\n");
        return g_chiller_core.status.active_faults;
    }
    
    // Pre-start checks
    ChillerCore_CheckSystemHealth();
    
    if (g_chiller_core.status.system_ready) {
        ChillerCore_ChangeState(CH_STATE_STARTING);
        return CH_FAULT_NONE;
    } else {
        Send_Debug_Data("Chiller Core: System not ready for start\r\n");
        return CH_FAULT_CRITICAL_SYSTEM;
    }
}

/**
 * @brief Stop System
 */
ChillerFaultCode_t ChillerCore_StopSystem(void)
{
    if (g_chiller_core.status.current_state == CH_STATE_OFF ||
        g_chiller_core.status.current_state == CH_STATE_STOPPING) {
        return CH_FAULT_NONE; // Already stopped or stopping
    }
    
    Send_Debug_Data("Chiller Core: Stop command received\r\n");
    ChillerCore_ChangeState(CH_STATE_STOPPING);
    return CH_FAULT_NONE;
}

/**
 * @brief Emergency Stop System
 */
ChillerFaultCode_t ChillerCore_EmergencyStop(void)
{
    Send_Debug_Data("Chiller Core: EMERGENCY STOP activated!\r\n");
    ChillerCore_ChangeState(CH_STATE_EMERGENCY_STOP);
    
    // Log emergency stop
    FlashConfig_LogAlarm(0xE911, 5, 0, 0.0f, "Emergency stop activated");
    
    return CH_FAULT_EMERGENCY_STOP;
}

/**
 * @brief Reset System Faults
 */
ChillerFaultCode_t ChillerCore_ResetFaults(void)
{
    g_chiller_core.status.active_faults = CH_FAULT_NONE;
    fault_retry_count = 0;
    
    Send_Debug_Data("Chiller Core: Faults reset\r\n");
    
    // If in fault state, return to OFF state
    if (g_chiller_core.status.current_state == CH_STATE_FAULT) {
        ChillerCore_ChangeState(CH_STATE_OFF);
    }
    
    return CH_FAULT_NONE;
}

/* === FAULT DETECTION AND HANDLING === */

/**
 * @brief Check System Faults
 */
ChillerFaultCode_t ChillerCore_CheckSystemFaults(void)
{
    ChillerFaultCode_t faults = CH_FAULT_NONE;
    
    // Check temperature faults
    faults |= ChillerCore_CheckTemperatureFaults();
    
    // Check pressure and flow faults
    faults |= ChillerCore_CheckPressureFlowFaults();
    
    // Check equipment faults
    faults |= ChillerCore_CheckEquipmentFaults();
    
    // Check communication faults
    if (!g_chiller_core.modbus_system_ok) {
        faults |= CH_FAULT_COMMUNICATION;
    }
    
    return faults;
}

/**
 * @brief Check Temperature-Related Faults
 */
ChillerFaultCode_t ChillerCore_CheckTemperatureFaults(void)
{
    ChillerFaultCode_t faults = CH_FAULT_NONE;
    
    // Get current temperature setpoints
    float supply_setpoint, return_setpoint, tolerance;
    EquipmentConfig_GetTemperatureSetpoints(&supply_setpoint, &return_setpoint, &tolerance);
    
    // Check if sensors are available for fault detection
    if (g_equipment_config.sensor_config.supply_sensor_enabled) {
        // Get supply temperature from Modbus (placeholder - implement actual sensor read)
        float supply_temp = 8.0f; // TODO: Get from Modbus system
        
        if (supply_temp > (supply_setpoint + tolerance + 2.0f)) {
            faults |= CH_FAULT_SUPPLY_TEMP_HIGH;
        }
        if (supply_temp < (supply_setpoint - tolerance - 2.0f)) {
            faults |= CH_FAULT_SUPPLY_TEMP_LOW;
        }
    }
    
    // Check ambient temperature if sensor available
    if (g_equipment_config.sensor_config.ambient_sensor_enabled) {
        float ambient_temp = 38.0f; // TODO: Get from Modbus system
        
        if (ambient_temp > g_equipment_config.high_ambient_alarm_limit) {
            faults |= CH_FAULT_AMBIENT_TEMP_HIGH;
        }
    }
    
    return faults;
}

/**
 * @brief Check Pressure and Flow Faults
 */
ChillerFaultCode_t ChillerCore_CheckPressureFlowFaults(void)
{
    ChillerFaultCode_t faults = CH_FAULT_NONE;
    
    // TODO: Implement pressure and flow fault checking
    // This would read from Modbus sensors and check against configured limits
    
    return faults;
}

/**
 * @brief Check Equipment Faults
 */
ChillerFaultCode_t ChillerCore_CheckEquipmentFaults(void)
{
    ChillerFaultCode_t faults = CH_FAULT_NONE;
    
    // TODO: Implement equipment-specific fault checking
    // Check compressor status, condenser status, etc.
    
    return faults;
}

/**
 * @brief Process Detected Faults
 */
void ChillerCore_ProcessFaults(ChillerFaultCode_t faults)
{
    // Update active faults
    g_chiller_core.status.active_faults = faults;
    g_chiller_core.status.fault_history |= faults;
    
    // Log faults to flash
    if (faults & CH_FAULT_SUPPLY_TEMP_HIGH) {
        ChillerCore_LogFault(CH_FAULT_SUPPLY_TEMP_HIGH, "Supply temperature too high");
    }
    if (faults & CH_FAULT_AMBIENT_TEMP_HIGH) {
        ChillerCore_LogFault(CH_FAULT_AMBIENT_TEMP_HIGH, "Ambient temperature too high");
    }
    // Add more fault logging as needed
    
    // If running, transition to fault state
    if (g_chiller_core.status.current_state == CH_STATE_RUNNING) {
        ChillerCore_ChangeState(CH_STATE_FAULT);
    }
}

/**
 * @brief Handle Fault Recovery
 */
static void ChillerCore_HandleFaultRecovery(void)
{
    uint32_t fault_duration = HAL_GetTick() - g_chiller_core.status.state_enter_time;
    
    if (fault_duration >= CH_CONTROL_FAULT_RETRY_DELAY) {
        if (fault_retry_count < FAULT_RETRY_MAX_ATTEMPTS) {
            // Attempt automatic recovery
            ChillerFaultCode_t current_faults = ChillerCore_CheckSystemFaults();
            
            if (current_faults == CH_FAULT_NONE) {
                Send_Debug_Data("Chiller Core: Automatic fault recovery successful\r\n");
                g_chiller_core.automatic_recoveries++;
                ChillerCore_ChangeState(CH_STATE_OFF);
                g_chiller_core.status.active_faults = CH_FAULT_NONE;
            } else {
                fault_retry_count++;
                Send_Debug_Data("Chiller Core: Fault recovery attempt failed\r\n");
            }
        } else {
            // Maximum retries exceeded - require manual intervention
            Send_Debug_Data("Chiller Core: Maximum fault recovery attempts exceeded\r\n");
            g_chiller_core.manual_interventions++;
        }
    }
}

/* === PERFORMANCE MONITORING === */

/**
 * @brief Update Performance Data
 */
void ChillerCore_UpdatePerformanceData(void)
{
    ChillerPerformanceData_t* perf = &g_chiller_core.performance_history[g_chiller_core.performance_index];
    
    // Update performance sample
    perf->timestamp = HAL_GetTick();
    perf->supply_temperature = 7.5f;    // TODO: Get from sensors
    perf->return_temperature = 12.8f;   // TODO: Get from sensors
    perf->ambient_temperature = 38.2f;  // TODO: Get from sensors
    perf->temperature_delta = perf->return_temperature - perf->supply_temperature;
    perf->system_pressure = 125;        // TODO: Get from sensors
    perf->flow_rate = 85;               // TODO: Get from sensors
    perf->current_mode = g_chiller_core.status.current_capacity_mode;
    
    // Count active equipment
    perf->active_compressors = 0;
    perf->active_condensers = 0;
    for (int i = 0; i < MAX_COMPRESSORS; i++) {
        if (Relay_Get(i)) perf->active_compressors++;
    }
    for (int i = 0; i < MAX_CONDENSER_BANKS; i++) {
        if (Relay_Get(i + 8)) perf->active_condensers++;
    }
    
    // Calculate system efficiency
    ChillerCore_CalculateSystemEfficiency();
    perf->system_efficiency = efficiency_filtered;
    
    // Update load demand
    ChillerCore_UpdateLoadDemand();
    
    // Update circular buffer
    g_chiller_core.performance_index++;
    if (g_chiller_core.performance_index >= CH_PERFORMANCE_HISTORY_SIZE) {
        g_chiller_core.performance_index = 0;
    }
    if (g_chiller_core.performance_count < CH_PERFORMANCE_HISTORY_SIZE) {
        g_chiller_core.performance_count++;
    }
}

/**
 * @brief Calculate System Efficiency
 */
void ChillerCore_CalculateSystemEfficiency(void)
{
    // Simple efficiency calculation based on temperature delta and active equipment
    float temp_delta = 5.3f; // Current temperature delta
    float target_delta = TEMPERATURE_DELTA_TARGET;
    
    float raw_efficiency = (temp_delta / target_delta) * 100.0f;
    if (raw_efficiency > 100.0f) raw_efficiency = 100.0f;
    if (raw_efficiency < 0.0f) raw_efficiency = 0.0f;
    
    // Apply smoothing filter
    efficiency_filtered = (EFFICIENCY_SMOOTHING_FACTOR * raw_efficiency) + 
                         ((1.0f - EFFICIENCY_SMOOTHING_FACTOR) * efficiency_filtered);
}

/**
 * @brief Update Load Demand Calculation
 */
void ChillerCore_UpdateLoadDemand(void)
{
    float load_demand = ChillerCore_CalculateLoadDemand();
    
    // Apply smoothing filter
    load_demand_filtered = (LOAD_DEMAND_SMOOTHING_FACTOR * load_demand) + 
                          ((1.0f - LOAD_DEMAND_SMOOTHING_FACTOR) * load_demand_filtered);
    
    // Update system status
    g_chiller_core.status.current_load_demand = load_demand_filtered;
    
    // Update peak demand
    if (load_demand_filtered > g_chiller_core.status.peak_load_demand) {
        g_chiller_core.status.peak_load_demand = load_demand_filtered;
    }
}

/**
 * @brief Calculate Current Load Demand
 */
static float ChillerCore_CalculateLoadDemand(void)
{
    // Simple load demand calculation based on temperature deviation from setpoint
    float supply_setpoint, return_setpoint, tolerance;
    EquipmentConfig_GetTemperatureSetpoints(&supply_setpoint, &return_setpoint, &tolerance);
    
    float current_return = 12.8f; // TODO: Get from sensors
    float deviation = current_return - return_setpoint;
    
    // Convert deviation to load demand percentage
    float load_demand = (deviation / tolerance) * 50.0f + 50.0f; // Base 50% + deviation
    
    if (load_demand > 100.0f) load_demand = 100.0f;
    if (load_demand < 0.0f) load_demand = 0.0f;
    
    return load_demand;
}

/* === SYSTEM INFORMATION FUNCTIONS === */

/**
 * @brief Display System Status
 */
void ChillerCore_DisplaySystemStatus(void)
{
    char msg[120];
    
    Send_Debug_Data("\r\n=== CHILLER CORE SYSTEM STATUS ===\r\n");
    
    snprintf(msg, sizeof(msg), "State: %s, Mode: %s\r\n",
             ChillerCore_GetStateName(g_chiller_core.status.current_state),
             (g_chiller_core.status.current_capacity_mode == CAPACITY_MODE_ECONOMIC) ? "Economic" :
             (g_chiller_core.status.current_capacity_mode == CAPACITY_MODE_NORMAL) ? "Normal" :
             (g_chiller_core.status.current_capacity_mode == CAPACITY_MODE_FULL) ? "Full" : "Custom");
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "Load Demand: %.1f%%, Efficiency: %.1f%%\r\n",
             g_chiller_core.status.current_load_demand, efficiency_filtered);
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "Active Faults: 0x%04X, Runtime: %lu ms\r\n",
             g_chiller_core.status.active_faults, g_chiller_core.status.total_run_time);
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "Subsystems - GPIO:%s Modbus:%s HMI:%s Flash:%s\r\n",
             g_chiller_core.gpio_manager_ok ? "OK" : "FAIL",
             g_chiller_core.modbus_system_ok ? "OK" : "FAIL",
             g_chiller_core.hmi_system_ok ? "OK" : "FAIL",
             g_chiller_core.flash_config_ok ? "OK" : "FAIL");
    Send_Debug_Data(msg);
    
    Send_Debug_Data("=====================================\r\n\r\n");
}

/* === UTILITY FUNCTIONS === */

/**
 * @brief Check System Health
 */
static void ChillerCore_CheckSystemHealth(void)
{
    g_chiller_core.status.system_ready = 1;
    
    // Check critical subsystems
    if (!g_chiller_core.gpio_manager_ok || !g_chiller_core.equipment_config_ok) {
        g_chiller_core.status.system_ready = 0;
    }
    
    // Check for active faults
    if (g_chiller_core.status.active_faults != CH_FAULT_NONE) {
        g_chiller_core.status.system_ready = 0;
    }
}

/**
 * @brief Update System Status
 */
void ChillerCore_UpdateSystemStatus(void)
{
    // Update subsystem status
    ChillerCore_CheckSubsystemStatus();
    
    // Update safety interlocks status
    g_chiller_core.status.safety_interlocks_ok = 1; // TODO: Implement safety checks
    
    // Update sensor system status
    g_chiller_core.status.sensors_ok = g_chiller_core.modbus_system_ok;
    
    // Update communication status
    g_chiller_core.status.communication_ok = g_chiller_core.modbus_system_ok && 
                                            g_chiller_core.hmi_system_ok;
}

/**
 * @brief Check Subsystem Status
 */
void ChillerCore_CheckSubsystemStatus(void)
{
    g_chiller_core.modbus_system_ok = Modbus_System_IsEnabled();
    g_chiller_core.hmi_system_ok = HMI_Is_Initialized();
    g_chiller_core.flash_config_ok = g_flash_config_initialized;
    g_chiller_core.equipment_config_ok = g_config_initialized;
}

/**
 * @brief Update HMI Registers
 */
void ChillerCore_UpdateHMIRegisters(void)
{
    // TODO: Update HMI VP registers with current system status
    // This will be implemented when HMI integration is enhanced
}

/**
 * @brief Synchronize with Equipment Configuration
 */
void ChillerCore_SynchronizeWithEquipmentConfig(void)
{
    g_chiller_core.status.current_capacity_mode = g_equipment_config.current_mode;
    g_chiller_core.status.auto_mode_enabled = g_equipment_config.auto_mode_switching;
}

/* === GETTER FUNCTIONS === */

ChillerSystemState_t ChillerCore_GetSystemState(void)
{
    return g_chiller_core.status.current_state;
}

ChillerFaultCode_t ChillerCore_GetActiveFaults(void)
{
    return g_chiller_core.status.active_faults;
}

CapacityMode_t ChillerCore_GetCurrentMode(void)
{
    return g_chiller_core.status.current_capacity_mode;
}

float ChillerCore_GetCurrentLoadDemand(void)
{
    return g_chiller_core.status.current_load_demand;
}

uint8_t ChillerCore_IsSystemRunning(void)
{
    return (g_chiller_core.status.current_state == CH_STATE_RUNNING) ? 1 : 0;
}

uint8_t ChillerCore_IsSystemReady(void)
{
    return g_chiller_core.status.system_ready;
}

/* === PLACEHOLDER FUNCTIONS === */

/**
 * @brief Auto Mode Control (Placeholder)
 */
void ChillerCore_AutoModeControl(void)
{
    // TODO: Implement automatic mode switching logic
    // This will be enhanced when temperature control and staging systems are implemented
}

/**
 * @brief Enter Maintenance Mode (Placeholder)
 */
ChillerFaultCode_t ChillerCore_EnterMaintenanceMode(void)
{
    Send_Debug_Data("Chiller Core: Entering maintenance mode\r\n");
    ChillerCore_ChangeState(CH_STATE_MAINTENANCE);
    return CH_FAULT_NONE;
}

/**
 * @brief Exit Maintenance Mode (Placeholder)
 */
ChillerFaultCode_t ChillerCore_ExitMaintenanceMode(void)
{
    Send_Debug_Data("Chiller Core: Exiting maintenance mode\r\n");
    ChillerCore_ChangeState(CH_STATE_OFF);
    return CH_FAULT_NONE;
}
