/**
 * ========================================================================
 * CHILLER EQUIPMENT STAGING CONTROL SYSTEM
 * ========================================================================
 * File: ch_staging.h
 * Author: Claude (Anthropic)
 * Date: 2025-09-18
 * 
 * DESCRIPTION:
 * Header file for chiller equipment staging control system.
 * Manages compressor and condenser staging with runtime balancing,
 * four-tier capacity control, and intelligent equipment rotation.
 * 
 * HARDWARE INTEGRATION:
 * - 8 Compressors: Relays 0-7 (PE5, PB8, PE2, PB9, PE3, PE4, PE6, PA10)
 * - 4 Condenser Banks: Relays 8-11 (PH0, PH1, PC0, PC1)
 * - Uses GPIO Manager Relay_Set() functions
 * - Integrates with equipment_config.h for installed counts
 * - Uses uart_comm.h for comprehensive debug output
 * 
 * ========================================================================
 */

 #ifndef CH_STAGING_H
 #define CH_STAGING_H
 
 #include <stdint.h>
 #include <stdbool.h>
 #include "equipment_config.h"
 #include "gpio_manager.h"
 #include "uart_comm.h"  // Changed from "debug_uart.h"
 
 // ========================================================================
 // CONSTANTS AND DEFINITIONS
 // ========================================================================
 
 // Maximum equipment counts (hardware limits)
 #define MAX_COMPRESSORS             8
 #define MAX_CONDENSER_BANKS         4
 
 // Equipment relay mappings (matches your GPIO manager)
 #define COMPRESSOR_1_RELAY          0    // PE5 (Relay0)
 #define COMPRESSOR_2_RELAY          1    // PB8 (Relay1)
 #define COMPRESSOR_3_RELAY          2    // PE2 (Relay2)
 #define COMPRESSOR_4_RELAY          3    // PB9 (Relay3)
 #define COMPRESSOR_5_RELAY          4    // PE3 (Relay4)
 #define COMPRESSOR_6_RELAY          5    // PE4 (Relay5)
 #define COMPRESSOR_7_RELAY          6    // PE6 (Relay6)
 #define COMPRESSOR_8_RELAY          7    // PA10(Relay7)
 
 #define CONDENSER_BANK_1_RELAY      8    // PH0 (Relay8)
 #define CONDENSER_BANK_2_RELAY      9    // PH1 (Relay9)
 #define CONDENSER_BANK_3_RELAY      10   // PC0 (Relay10)
 #define CONDENSER_BANK_4_RELAY      11   // PC1 (Relay11)
 
 // Staging timing constants (milliseconds)
 #define COMPRESSOR_START_DELAY      15000   // 15 seconds between compressor starts
 #define COMPRESSOR_STOP_DELAY       10000   // 10 seconds between compressor stops
 #define CONDENSER_START_DELAY       5000    // 5 seconds between condenser starts
 #define CONDENSER_STOP_DELAY        3000    // 3 seconds between condenser stops
 #define MINIMUM_RUN_TIME           300000   // 5 minutes minimum run time
 #define RUNTIME_BALANCE_THRESHOLD   36000   // 10 hours runtime difference threshold
 
 // Four-tier capacity levels
 #define CAPACITY_TIER_1             2       // 2 compressors (25% capacity)
 #define CAPACITY_TIER_2             4       // 4 compressors (50% capacity)
 #define CAPACITY_TIER_3             6       // 6 compressors (75% capacity)
 #define CAPACITY_TIER_4             8       // 8 compressors (100% capacity)
 
 // ========================================================================
 // ENUMERATIONS
 // ========================================================================
 
 // Equipment staging states
 typedef enum {
     STAGING_STATE_OFF = 0,
     STAGING_STATE_STARTING,
     STAGING_STATE_RUNNING,
     STAGING_STATE_STOPPING,
     STAGING_STATE_FAULT,
     STAGING_STATE_DISABLED
 } StagingState_t;
 
 // Compressor control modes
 typedef enum {
     COMPRESSOR_MODE_AUTO = 0,
     COMPRESSOR_MODE_MANUAL_ON,
     COMPRESSOR_MODE_MANUAL_OFF,
     COMPRESSOR_MODE_DISABLED,
     COMPRESSOR_MODE_FAULT
 } CompressorMode_t;
 
 // Condenser control modes
 typedef enum {
     CONDENSER_MODE_AUTO = 0,
     CONDENSER_MODE_MANUAL_ON,
     CONDENSER_MODE_MANUAL_OFF,
     CONDENSER_MODE_DISABLED,
     CONDENSER_MODE_FAULT
 } CondenserMode_t;
 
 // Staging algorithms
 typedef enum {
     STAGING_ALGORITHM_SEQUENTIAL = 0,
     STAGING_ALGORITHM_RUNTIME_BALANCED,
     STAGING_ALGORITHM_PERFORMANCE_BASED,
     STAGING_ALGORITHM_MANUAL
 } StagingAlgorithm_t;
 
 // ========================================================================
 // DATA STRUCTURES
 // ========================================================================
 
 // Individual compressor status
 typedef struct {
     bool is_running;                    // Current running state
     bool relay_state;                   // Physical relay state
     CompressorMode_t mode;              // Control mode
     uint32_t start_time;                // Last start timestamp
     uint32_t stop_time;                 // Last stop timestamp
     uint32_t runtime_hours;             // Total runtime hours
     uint32_t start_cycles;              // Number of start cycles
     uint16_t fault_count;               // Fault occurrence count
     bool enabled;                       // Equipment enabled flag
     bool available;                     // Equipment available flag
     float performance_rating;           // Performance rating (0.0-1.0)
 } CompressorStatus_t;
 
 // Individual condenser bank status
 typedef struct {
     bool is_running;                    // Current running state
     bool relay_state;                   // Physical relay state
     CondenserMode_t mode;               // Control mode
     uint32_t start_time;                // Last start timestamp
     uint32_t stop_time;                 // Last stop timestamp
     uint32_t runtime_hours;             // Total runtime hours
     uint32_t start_cycles;              // Number of start cycles
     uint16_t fault_count;               // Fault occurrence count
     bool enabled;                       // Equipment enabled flag
     bool available;                     // Equipment available flag
     float cooling_efficiency;           // Cooling efficiency rating
 } CondenserStatus_t;
 
 // Staging control parameters
 typedef struct {
     StagingAlgorithm_t algorithm;       // Active staging algorithm
     uint8_t target_compressor_count;    // Target number of compressors
     uint8_t target_condenser_count;     // Target number of condensers
     uint8_t current_capacity_tier;      // Current capacity tier (1-4)
     uint8_t max_capacity_tier;          // Maximum allowed capacity tier
     bool runtime_balancing_enabled;     // Runtime balancing feature
     bool auto_staging_enabled;          // Automatic staging feature
     uint32_t staging_delay_compressor;  // Compressor staging delay
     uint32_t staging_delay_condenser;   // Condenser staging delay
 } StagingControl_t;
 
 // System staging status
 typedef struct {
     StagingState_t system_state;        // Overall staging system state
     uint8_t running_compressor_count;   // Number of running compressors
     uint8_t running_condenser_count;    // Number of running condensers
     uint8_t available_compressor_count; // Number of available compressors
     uint8_t available_condenser_count;  // Number of available condensers
     uint32_t last_compressor_start;     // Last compressor start time
     uint32_t last_compressor_stop;      // Last compressor stop time
     uint32_t last_condenser_start;      // Last condenser start time
     uint32_t last_condenser_stop;       // Last condenser stop time
     float system_capacity_percent;      // Current system capacity %
     bool staging_in_progress;           // Staging operation active
 } StagingStatus_t;
 
 // Complete staging system structure
 typedef struct {
     // Equipment arrays
     CompressorStatus_t compressors[MAX_COMPRESSORS];
     CondenserStatus_t condensers[MAX_CONDENSER_BANKS];
     
     // Control parameters
     StagingControl_t control;
     
     // System status
     StagingStatus_t status;
     
     // Runtime balancing data
     uint8_t next_compressor_to_start;   // Lead compressor index
     uint8_t next_compressor_to_stop;    // Lag compressor index
     uint8_t next_condenser_to_start;    // Lead condenser index
     uint8_t next_condenser_to_stop;     // Lag condenser index
     
     // Timing and delays
     uint32_t last_process_time;         // Last process execution time
     bool debug_enabled;                 // Debug output enabled
     
 } ChillerStaging_t;
 
 // ========================================================================
 // GLOBAL VARIABLES
 // ========================================================================
 
 extern ChillerStaging_t g_staging_system;
 
 // ========================================================================
 // FUNCTION PROTOTYPES
 // ========================================================================
 
 // === INITIALIZATION FUNCTIONS ===
 /**
  * @brief Initialize the chiller staging control system
  * @return true if initialization successful, false otherwise
  */
 bool Staging_Init(void);
 
 /**
  * @brief Load staging configuration from flash memory
  * @return true if configuration loaded successfully
  */
 bool Staging_LoadConfiguration(void);
 
 /**
  * @brief Save staging configuration to flash memory
  * @return true if configuration saved successfully
  */
 bool Staging_SaveConfiguration(void);
 
 // === MAIN PROCESS FUNCTIONS ===
 /**
  * @brief Main staging control process (call every 100ms)
  * @return true if process completed successfully
  */
 bool Staging_Process(void);
 
 /**
  * @brief Update staging based on required capacity percentage
  * @param capacity_percent Required capacity (0-100%)
  * @return true if staging updated successfully
  */
 bool Staging_UpdateCapacity(float capacity_percent);
 
 /**
  * @brief Execute staging algorithm for compressors
  * @return true if staging completed successfully
  */
 bool Staging_ProcessCompressors(void);
 
 /**
  * @brief Execute staging algorithm for condensers
  * @return true if staging completed successfully
  */
 bool Staging_ProcessCondensers(void);
 
 // === COMPRESSOR CONTROL FUNCTIONS ===
 /**
  * @brief Start a specific compressor
  * @param compressor_index Index of compressor (0-7)
  * @return true if start command successful
  */
 bool Staging_StartCompressor(uint8_t compressor_index);
 
 /**
  * @brief Stop a specific compressor
  * @param compressor_index Index of compressor (0-7)
  * @return true if stop command successful
  */
 bool Staging_StopCompressor(uint8_t compressor_index);
 
 /**
  * @brief Start all available compressors
  * @return true if all starts successful
  */
 bool Staging_StartAllCompressors(void);
 
 /**
  * @brief Stop all running compressors
  * @return true if all stops successful
  */
 bool Staging_StopAllCompressors(void);
 
 // === CONDENSER CONTROL FUNCTIONS ===
 /**
  * @brief Start a specific condenser bank
  * @param condenser_index Index of condenser (0-3)
  * @return true if start command successful
  */
 bool Staging_StartCondenser(uint8_t condenser_index);
 
 /**
  * @brief Stop a specific condenser bank
  * @param condenser_index Index of condenser (0-3)
  * @return true if stop command successful
  */
 bool Staging_StopCondenser(uint8_t condenser_index);
 
 /**
  * @brief Start all available condensers
  * @return true if all starts successful
  */
 bool Staging_StartAllCondensers(void);
 
 /**
  * @brief Stop all running condensers
  * @return true if all stops successful
  */
 bool Staging_StopAllCondensers(void);
 
 // === RUNTIME BALANCING FUNCTIONS ===
 /**
  * @brief Update equipment runtime hours
  */
 void Staging_UpdateRuntimeHours(void);
 
 /**
  * @brief Select next compressor to start based on runtime balancing
  * @return Index of compressor to start (0-7)
  */
 uint8_t Staging_SelectNextCompressorToStart(void);
 
 /**
  * @brief Select next compressor to stop based on runtime balancing
  * @return Index of compressor to stop (0-7)
  */
 uint8_t Staging_SelectNextCompressorToStop(void);
 
 /**
  * @brief Select next condenser to start based on runtime balancing
  * @return Index of condenser to start (0-3)
  */
 uint8_t Staging_SelectNextCondenserToStart(void);
 
 /**
  * @brief Select next condenser to stop based on runtime balancing
  * @return Index of condenser to stop (0-3)
  */
 uint8_t Staging_SelectNextCondenserToStop(void);
 
 // === CONFIGURATION FUNCTIONS ===
 /**
  * @brief Set staging algorithm type
  * @param algorithm Staging algorithm to use
  * @return true if algorithm set successfully
  */
 bool Staging_SetAlgorithm(StagingAlgorithm_t algorithm);
 
 /**
  * @brief Set maximum capacity tier
  * @param max_tier Maximum capacity tier (1-4)
  * @return true if tier set successfully
  */
 bool Staging_SetMaxCapacityTier(uint8_t max_tier);
 
 /**
  * @brief Enable or disable runtime balancing
  * @param enabled true to enable, false to disable
  */
 void Staging_SetRuntimeBalancing(bool enabled);
 
 /**
  * @brief Enable or disable automatic staging
  * @param enabled true to enable, false to disable
  */
 void Staging_SetAutoStaging(bool enabled);
 
 // === STATUS AND MONITORING FUNCTIONS ===
 /**
  * @brief Get current staging system status
  * @return Pointer to staging status structure
  */
 StagingStatus_t* Staging_GetStatus(void);
 
 /**
  * @brief Get specific compressor status
  * @param compressor_index Index of compressor (0-7)
  * @return Pointer to compressor status structure
  */
 CompressorStatus_t* Staging_GetCompressorStatus(uint8_t compressor_index);
 
 /**
  * @brief Get specific condenser status
  * @param condenser_index Index of condenser (0-3)
  * @return Pointer to condenser status structure
  */
 CondenserStatus_t* Staging_GetCondenserStatus(uint8_t condenser_index);
 
 /**
  * @brief Get current system capacity percentage
  * @return Current capacity percentage (0-100%)
  */
 float Staging_GetCurrentCapacityPercent(void);
 
 /**
  * @brief Get number of running compressors
  * @return Number of currently running compressors
  */
 uint8_t Staging_GetRunningCompressorCount(void);
 
 /**
  * @brief Get number of running condensers
  * @return Number of currently running condensers
  */
 uint8_t Staging_GetRunningCondenserCount(void);
 
 // === DEBUG AND DIAGNOSTICS FUNCTIONS ===
 /**
  * @brief Enable or disable debug output
  * @param enabled true to enable debug output
  */
 void Staging_SetDebugEnabled(bool enabled);
 
 /**
  * @brief Print complete staging system status to debug port
  */
 void Staging_PrintStatus(void);
 
 /**
  * @brief Print compressor status to debug port
  */
 void Staging_PrintCompressorStatus(void);
 
 /**
  * @brief Print condenser status to debug port
  */
 void Staging_PrintCondenserStatus(void);
 
 /**
  * @brief Print runtime balancing information to debug port
  */
 void Staging_PrintRuntimeBalance(void);
 
 /**
  * @brief Test staging system functionality
  * @return true if all tests pass
  */
 bool Staging_RunDiagnostics(void);
 
 // === MANUAL CONTROL FUNCTIONS ===
 /**
  * @brief Set compressor to manual mode
  * @param compressor_index Index of compressor (0-7)
  * @param mode Manual mode to set
  * @return true if mode set successfully
  */
 bool Staging_SetCompressorManualMode(uint8_t compressor_index, CompressorMode_t mode);
 
 /**
  * @brief Set condenser to manual mode
  * @param condenser_index Index of condenser (0-3)
  * @param mode Manual mode to set
  * @return true if mode set successfully
  */
 bool Staging_SetCondenserManualMode(uint8_t condenser_index, CondenserMode_t mode);
 
 /**
  * @brief Return all equipment to automatic mode
  * @return true if all equipment set to auto mode
  */
 bool Staging_SetAllAutoMode(void);
 
 // === FAULT HANDLING FUNCTIONS ===
 /**
  * @brief Report compressor fault
  * @param compressor_index Index of compressor (0-7)
  * @param fault_description Description of fault
  */
 void Staging_ReportCompressorFault(uint8_t compressor_index, const char* fault_description);
 
 /**
  * @brief Report condenser fault
  * @param condenser_index Index of condenser (0-3)
  * @param fault_description Description of fault
  */
 void Staging_ReportCondenserFault(uint8_t condenser_index, const char* fault_description);
 
 /**
  * @brief Clear all equipment faults
  * @return true if faults cleared successfully
  */
 bool Staging_ClearAllFaults(void);
 
 /**
  * @brief Emergency stop all equipment
  * @return true if emergency stop successful
  */
 bool Staging_EmergencyStop(void);
 
 #endif // CH_STAGING_H
 
 /**
  * ========================================================================
  * END OF FILE: ch_staging.h
  * ========================================================================
  */
