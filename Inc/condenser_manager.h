/**
 * ========================================================================
 * SMART CONDENSER MANAGEMENT SYSTEM
 * ========================================================================
 * File: condenser_manager.h
 * Author: Claude (Anthropic)
 * Date: 2025-09-18
 * 
 * DESCRIPTION:
 * Advanced condenser management system with intelligent motor rotation,
 * performance tracking, maintenance scheduling, and adaptive control
 * for maximum efficiency and equipment longevity.
 * 
 * FEATURES:
 * - Individual condenser runtime tracking and balancing
 * - Performance-based selection algorithms
 * - Motor rotation for even wear distribution
 * - Predictive maintenance scheduling
 * - Seasonal efficiency optimization
 * - Hot climate adaptation (38°C baseline)
 * - Integration with ch_staging.c system
 * 
 * ========================================================================
 */

 #ifndef CONDENSER_MANAGER_H
 #define CONDENSER_MANAGER_H
 
 #include <stdint.h>
 #include <stdbool.h>
 #include "ch_staging.h"
 #include "equipment_config.h"
 #include "uart_comm.h"  // Changed from "debug_uart.h"
 
 // ========================================================================
 // CONSTANTS AND DEFINITIONS
 // ========================================================================
 
 // Performance tracking constants
 #define CONDENSER_PERFORMANCE_SAMPLES   24      // 24 hours of performance data
 #define CONDENSER_EFFICIENCY_THRESHOLD  0.75f   // Minimum efficiency rating
 #define CONDENSER_MAINTENANCE_HOURS     8760    // Annual maintenance (hours)
 #define CONDENSER_DEGRADATION_FACTOR    0.99f   // Annual efficiency degradation
 #define CONDENSER_TEMPERATURE_RATING    38.0f   // Hot climate baseline (°C)
 
 // Motor rotation parameters
 #define ROTATION_BALANCE_THRESHOLD      50      // Hours difference for rotation
 #define ROTATION_COOLDOWN_TIME         3600     // 1 hour between rotations (seconds)
 #define MOTOR_STARTUP_CURRENT_PEAK     1.5f     // Peak current during startup
 #define MOTOR_STEADY_STATE_CURRENT     1.0f     // Normal running current
 
 // Performance monitoring intervals
 #define PERFORMANCE_UPDATE_INTERVAL    3600000  // 1 hour (milliseconds)
 #define EFFICIENCY_CALC_INTERVAL       21600000 // 6 hours (milliseconds)
 #define MAINTENANCE_CHECK_INTERVAL     86400000 // 24 hours (milliseconds)
 
 // Ambient temperature zones for adaptive control
 #define AMBIENT_ZONE_COOL              25.0f    // Below 25°C
 #define AMBIENT_ZONE_MILD              35.0f    // 25-35°C
 #define AMBIENT_ZONE_HOT               45.0f    // 35-45°C
 #define AMBIENT_ZONE_EXTREME           50.0f    // Above 45°C
 
 // ========================================================================
 // ENUMERATIONS
 // ========================================================================
 
 // Condenser performance states
 typedef enum {
     CONDENSER_PERF_EXCELLENT = 0,   // >95% efficiency
     CONDENSER_PERF_GOOD,            // 85-95% efficiency
     CONDENSER_PERF_FAIR,            // 75-85% efficiency
     CONDENSER_PERF_POOR,            // 60-75% efficiency
     CONDENSER_PERF_FAILED,          // <60% efficiency
     CONDENSER_PERF_UNKNOWN          // No data available
 } CondenserPerformance_t;
 
 // Maintenance states
 typedef enum {
     MAINTENANCE_OK = 0,             // No maintenance required
     MAINTENANCE_DUE_SOON,           // Maintenance due within 30 days
     MAINTENANCE_DUE_NOW,            // Maintenance overdue
     MAINTENANCE_CRITICAL,           // Critical maintenance required
     MAINTENANCE_IN_PROGRESS         // Maintenance being performed
 } MaintenanceState_t;
 
 // Selection algorithms
 typedef enum {
     SELECTION_ALGORITHM_RUNTIME = 0,    // Runtime balancing priority
     SELECTION_ALGORITHM_PERFORMANCE,    // Performance-based priority
     SELECTION_ALGORITHM_HYBRID,         // Combined runtime + performance
     SELECTION_ALGORITHM_MAINTENANCE,    // Maintenance schedule priority
     SELECTION_ALGORITHM_ADAPTIVE        // Adaptive based on conditions
 } SelectionAlgorithm_t;
 
 // Ambient adaptation modes
 typedef enum {
     AMBIENT_MODE_STANDARD = 0,      // Standard operation
     AMBIENT_MODE_HOT_CLIMATE,       // Hot climate optimization
     AMBIENT_MODE_VARIABLE,          // Variable ambient adaptation
     AMBIENT_MODE_EXTREME_HEAT       // Extreme heat protection
 } AmbientMode_t;
 
 // ========================================================================
 // DATA STRUCTURES
 // ========================================================================
 
 // Performance metrics for individual condenser
 typedef struct {
     float efficiency_rating;            // Current efficiency (0.0-1.0)
     float power_consumption;            // Power consumption (kW)
     float cooling_capacity;             // Cooling capacity (tons)
     float temperature_delta;            // Inlet-outlet temperature difference
     uint32_t performance_samples;       // Number of performance samples
     float efficiency_trend;             // Efficiency trend (positive/negative)
     uint32_t last_performance_update;   // Last performance calculation time
     bool performance_valid;             // Performance data validity
 } CondenserPerformanceData_t;
 
 // Motor-specific data
 typedef struct {
     float motor_current;                // Current motor current (A)
     float motor_voltage;                // Motor voltage (V)
     float motor_power_factor;           // Power factor
     uint32_t motor_starts;              // Number of motor starts
     uint32_t motor_runtime_hours;       // Motor runtime hours
     float motor_temperature;            // Motor temperature (°C)
     bool motor_fault_detected;          // Motor fault flag
     uint32_t last_motor_start;          // Last motor start time
 } CondenserMotorData_t;
 
 // Maintenance tracking data
 typedef struct {
     uint32_t last_maintenance_date;     // Last maintenance timestamp
     uint32_t next_maintenance_due;      // Next maintenance due date
     MaintenanceState_t maintenance_state; // Current maintenance state
     uint16_t maintenance_cycles;        // Number of maintenance cycles
     float maintenance_cost;             // Cumulative maintenance cost
     char maintenance_notes[100];        // Maintenance notes
     bool maintenance_override;          // Manual maintenance override
 } CondenserMaintenanceData_t;
 
 // Individual condenser management data
 typedef struct {
     uint8_t condenser_id;               // Condenser ID (0-3)
     bool is_managed;                    // Under smart management
     bool available;                     // Condenser is available for operation
     float priority_score;               // Selection priority score
     uint32_t total_runtime_hours;       // Total accumulated runtime
     uint32_t total_start_cycles;        // Total start cycles
     
     // Performance tracking
     CondenserPerformanceData_t performance;
     
     // Motor management
     CondenserMotorData_t motor;
     
     // Maintenance tracking
     CondenserMaintenanceData_t maintenance;
     
     // Selection weighting factors
     float runtime_weight;               // Runtime balancing weight
     float performance_weight;           // Performance priority weight
     float maintenance_weight;           // Maintenance priority weight
     
     // Environmental adaptation
     float ambient_compensation;         // Ambient temperature compensation
     float seasonal_factor;              // Seasonal efficiency factor
     
 } CondenserManagedData_t;
 
 // System-wide condenser management
 typedef struct {
     // Configuration
     SelectionAlgorithm_t selection_algorithm;
     AmbientMode_t ambient_mode;
     bool rotation_enabled;
     bool performance_tracking_enabled;
     bool maintenance_tracking_enabled;
     
     // Individual condenser data
     CondenserManagedData_t condensers[MAX_CONDENSER_BANKS];
     
     // System metrics
     float system_efficiency;            // Overall system efficiency
     float system_power_consumption;     // Total power consumption
     uint8_t active_condenser_count;     // Currently active condensers
     uint8_t available_condenser_count;  // Available condensers
     
     // Environmental data
     float ambient_temperature;          // Current ambient temperature
     float ambient_humidity;             // Current ambient humidity
     uint8_t ambient_zone;               // Current ambient zone
     
     // Rotation management
     uint8_t lead_condenser_index;       // Current lead condenser
     uint8_t lag_condenser_index;        // Current lag condenser
     uint32_t last_rotation_time;        // Last rotation timestamp
     bool rotation_in_progress;          // Rotation operation active
     
     // Performance analytics
     float daily_efficiency_avg;         // Daily average efficiency
     float weekly_efficiency_avg;        // Weekly average efficiency
     float monthly_efficiency_avg;       // Monthly average efficiency
     uint32_t performance_data_points;   // Total performance data points
     
     // Maintenance scheduling
     uint8_t next_maintenance_condenser; // Next condenser for maintenance
     uint32_t next_maintenance_time;     // Next scheduled maintenance
     bool maintenance_mode_active;       // Maintenance mode flag
     
     // Debug and diagnostics
     bool debug_enabled;                 // Debug output enabled
     uint32_t last_debug_output;         // Last debug output time
     uint32_t diagnostic_error_count;    // Diagnostic error counter
     
 } CondenserManager_t;
 
 // ========================================================================
 // GLOBAL VARIABLES
 // ========================================================================
 
 extern CondenserManager_t g_condenser_manager;
 
 // ========================================================================
 // FUNCTION PROTOTYPES
 // ========================================================================
 
 // === INITIALIZATION FUNCTIONS ===
 /**
  * @brief Initialize the smart condenser management system
  * @return true if initialization successful, false otherwise
  */
 bool CondenserManager_Init(void);
 
 /**
  * @brief Load condenser management configuration from flash
  * @return true if configuration loaded successfully
  */
 bool CondenserManager_LoadConfiguration(void);
 
 /**
  * @brief Save condenser management configuration to flash
  * @return true if configuration saved successfully
  */
 bool CondenserManager_SaveConfiguration(void);
 
 /**
  * @brief Reset all condenser management data to defaults
  */
 void CondenserManager_ResetToDefaults(void);
 
 // === MAIN PROCESS FUNCTIONS ===
 /**
  * @brief Main condenser management process (call every 100ms)
  * @return true if process completed successfully
  */
 bool CondenserManager_Process(void);
 
 /**
  * @brief Update condenser performance metrics
  */
 void CondenserManager_UpdatePerformanceMetrics(void);
 
 /**
  * @brief Process condenser rotation logic
  */
 void CondenserManager_ProcessRotation(void);
 
 /**
  * @brief Update maintenance schedules
  */
 void CondenserManager_UpdateMaintenanceSchedules(void);
 
 // === SELECTION ALGORITHM FUNCTIONS ===
 /**
  * @brief Select best condenser to start based on current algorithm
  * @param required_count Number of condensers required
  * @param selected_indices Array to store selected condenser indices
  * @return Number of condensers selected
  */
 uint8_t CondenserManager_SelectCondensersToStart(uint8_t required_count, uint8_t* selected_indices);
 
 /**
  * @brief Select best condenser to stop based on current algorithm
  * @param stop_count Number of condensers to stop
  * @param selected_indices Array to store selected condenser indices
  * @return Number of condensers selected for stopping
  */
 uint8_t CondenserManager_SelectCondensersToStop(uint8_t stop_count, uint8_t* selected_indices);
 
 /**
  * @brief Calculate priority score for condenser selection
  * @param condenser_index Index of condenser (0-3)
  * @return Priority score (higher = better candidate)
  */
 float CondenserManager_CalculatePriorityScore(uint8_t condenser_index);
 
 /**
  * @brief Update selection weights based on operating conditions
  */
 void CondenserManager_UpdateSelectionWeights(void);
 
 // === PERFORMANCE TRACKING FUNCTIONS ===
 /**
  * @brief Update performance data for specific condenser
  * @param condenser_index Index of condenser (0-3)
  * @param efficiency Current efficiency reading
  * @param power_consumption Current power consumption
  * @param cooling_capacity Current cooling capacity
 */
void CondenserManager_UpdateCondenserPerformance(uint8_t condenser_index, 
                                                 float efficiency, 
                                                 float power_consumption, 
                                                 float cooling_capacity);
 
 /**
  * @brief Calculate efficiency trend for condenser
  * @param condenser_index Index of condenser (0-3)
  * @return Efficiency trend (positive/negative slope)
  */
 float CondenserManager_CalculateEfficiencyTrend(uint8_t condenser_index);
 
 /**
  * @brief Get condenser performance rating
  * @param condenser_index Index of condenser (0-3)
  * @return Performance rating enumeration
  */
 CondenserPerformance_t CondenserManager_GetPerformanceRating(uint8_t condenser_index);
 
 /**
  * @brief Validate performance data quality
  * @param condenser_index Index of condenser (0-3)
  * @return true if performance data is valid
  */
 bool CondenserManager_ValidatePerformanceData(uint8_t condenser_index);
 
 // === MOTOR MANAGEMENT FUNCTIONS ===
 /**
  * @brief Update motor runtime tracking
  * @param condenser_index Index of condenser (0-3)
  * @param is_running true if motor is currently running
  */
 void CondenserManager_UpdateMotorRuntime(uint8_t condenser_index, bool is_running);
 
 /**
  * @brief Monitor motor health and detect faults
  * @param condenser_index Index of condenser (0-3)
  * @return true if motor is healthy, false if fault detected
  */
 bool CondenserManager_MonitorMotorHealth(uint8_t condenser_index);
 
 /**
  * @brief Calculate motor efficiency
  * @param condenser_index Index of condenser (0-3)
  * @return Motor efficiency (0.0-1.0)
  */
 float CondenserManager_CalculateMotorEfficiency(uint8_t condenser_index);
 
 // === MAINTENANCE FUNCTIONS ===
 /**
  * @brief Check maintenance schedules for all condensers
  */
 void CondenserManager_CheckMaintenanceSchedules(void);
 
 /**
  * @brief Schedule maintenance for specific condenser
  * @param condenser_index Index of condenser (0-3)
  * @param maintenance_type Type of maintenance required
  * @param days_from_now Days from now to schedule maintenance
  */
 void CondenserManager_ScheduleMaintenance(uint8_t condenser_index, 
                                          MaintenanceState_t maintenance_type, 
                                          uint16_t days_from_now);
 
 /**
  * @brief Mark maintenance as completed for condenser
  * @param condenser_index Index of condenser (0-3)
  * @param maintenance_notes Notes from maintenance
  */
 void CondenserManager_CompleteMaintenance(uint8_t condenser_index, const char* maintenance_notes);
 
 /**
  * @brief Get days until next maintenance for condenser
  * @param condenser_index Index of condenser (0-3)
  * @return Days until maintenance (-1 if overdue)
  */
 int16_t CondenserManager_GetDaysUntilMaintenance(uint8_t condenser_index);
 
 // === ENVIRONMENTAL ADAPTATION FUNCTIONS ===
 /**
  * @brief Update ambient conditions and adapt control
  * @param ambient_temp Current ambient temperature (°C)
  * @param ambient_humidity Current ambient humidity (%)
  */
 void CondenserManager_UpdateAmbientConditions(float ambient_temp, float ambient_humidity);
 
 /**
  * @brief Calculate ambient compensation factor
  * @param condenser_index Index of condenser (0-3)
  * @return Compensation factor for ambient conditions
  */
 float CondenserManager_CalculateAmbientCompensation(uint8_t condenser_index);
 
 /**
  * @brief Determine optimal condenser count for ambient conditions
  * @param base_requirement Base condenser requirement
  * @return Adjusted condenser count for ambient conditions
  */
 uint8_t CondenserManager_GetAmbientAdjustedCount(uint8_t base_requirement);
 
 // === CONFIGURATION FUNCTIONS ===
 /**
  * @brief Set selection algorithm
  * @param algorithm Selection algorithm to use
  * @return true if algorithm set successfully
  */
 bool CondenserManager_SetSelectionAlgorithm(SelectionAlgorithm_t algorithm);
 
 /**
  * @brief Set ambient adaptation mode
  * @param mode Ambient adaptation mode to use
  * @return true if mode set successfully
  */
 bool CondenserManager_SetAmbientMode(AmbientMode_t mode);
 
 /**
  * @brief Enable or disable rotation feature
  * @param enabled true to enable rotation, false to disable
  */
 void CondenserManager_SetRotationEnabled(bool enabled);
 
 /**
  * @brief Enable or disable performance tracking
  * @param enabled true to enable tracking, false to disable
  */
 void CondenserManager_SetPerformanceTrackingEnabled(bool enabled);
 
 /**
  * @brief Set runtime balancing weight
  * @param condenser_index Index of condenser (0-3)
  * @param weight Runtime weight (0.0-1.0)
  */
 void CondenserManager_SetRuntimeWeight(uint8_t condenser_index, float weight);
 
 // === STATUS AND MONITORING FUNCTIONS ===
 /**
  * @brief Get system-wide efficiency rating
  * @return Overall system efficiency (0.0-1.0)
  */
 float CondenserManager_GetSystemEfficiency(void);
 
 /**
  * @brief Get total system power consumption
  * @return Total power consumption (kW)
  */
 float CondenserManager_GetSystemPowerConsumption(void);
 
 /**
  * @brief Get condenser management data for specific condenser
  * @param condenser_index Index of condenser (0-3)
  * @return Pointer to condenser management data
  */
 CondenserManagedData_t* CondenserManager_GetCondenserData(uint8_t condenser_index);
 
 /**
  * @brief Get current lead condenser index
  * @return Index of lead condenser
  */
 uint8_t CondenserManager_GetLeadCondenserIndex(void);
 
 /**
  * @brief Get runtime balance status
  * @param max_runtime Pointer to store maximum runtime hours
  * @param min_runtime Pointer to store minimum runtime hours
  * @return Runtime spread (max - min) in hours
  */
 uint32_t CondenserManager_GetRuntimeBalance(uint32_t* max_runtime, uint32_t* min_runtime);
 
 // === DEBUG AND DIAGNOSTICS FUNCTIONS ===
 /**
  * @brief Enable or disable debug output
  * @param enabled true to enable debug output
  */
 void CondenserManager_SetDebugEnabled(bool enabled);
 
 /**
  * @brief Print complete condenser management status
  */
 void CondenserManager_PrintStatus(void);
 
 /**
  * @brief Print performance analytics
  */
 void CondenserManager_PrintPerformanceAnalytics(void);
 
 /**
  * @brief Print maintenance schedule
  */
 void CondenserManager_PrintMaintenanceSchedule(void);
 
 /**
  * @brief Print rotation status and history
  */
 void CondenserManager_PrintRotationStatus(void);
 
 /**
  * @brief Run comprehensive diagnostics
  * @return true if all diagnostics pass
  */
 bool CondenserManager_RunDiagnostics(void);
 
 /**
  * @brief Export performance data to debug port
  * @param condenser_index Index of condenser (0-3, or 255 for all)
  */
 void CondenserManager_ExportPerformanceData(uint8_t condenser_index);
 
 // === INTEGRATION FUNCTIONS ===
 /**
  * @brief Register with staging system for condenser selection
  * @return true if registration successful
  */
 bool CondenserManager_RegisterWithStaging(void);
 
 /**
  * @brief Notify condenser manager of staging changes
  * @param condenser_index Index of condenser (0-3)
  * @param is_starting true if condenser is starting, false if stopping
  */
 void CondenserManager_NotifyCondenserStateChange(uint8_t condenser_index, bool is_starting);
 
 /**
  * @brief Get recommended condenser selection for staging system
  * @param required_count Number of condensers required
  * @param selected_indices Array to store recommended indices
  * @return Number of condensers recommended
  */
 uint8_t CondenserManager_GetStagingRecommendation(uint8_t required_count, uint8_t* selected_indices);
 
 #endif // CONDENSER_MANAGER_H
 
 /**
  * ========================================================================
  * END OF FILE: condenser_manager.h
  * ========================================================================
  */