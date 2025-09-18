/**
  ******************************************************************************
  * @file    equipment_config.h
  * @brief   Equipment Configuration Management for Chiller Control System
  * @author  Industrial Chiller Control System
  * @version 1.0
  * @date    2025
  * 
  * @description
  * This module manages equipment configuration, capacity modes, sensor availability,
  * and provides hot climate optimization (38°C baseline) for industrial chiller
  * control systems.
  * 
  * Features:
  * - Four-tier capacity control (Economic/Normal/Full/Custom)  
  * - Adaptive sensor configuration with graceful degradation
  * - Flash storage integration with 1-minute update intervals
  * - 38°C hot climate optimization as default baseline
  * - Equipment count management (2/4/6/8 compressors + condenser banks)
  * - Runtime hours tracking and equipment balancing support
  ******************************************************************************
  */

#ifndef EQUIPMENT_CONFIG_H
#define EQUIPMENT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "flash_25q16.h"
#include "uart_comm.h"
#include <stdint.h>
#include <string.h>

/* Equipment Configuration Constants -----------------------------------------*/
#define EQUIPMENT_CONFIG_VERSION        0x0100      // Version 1.0
#define EQUIPMENT_CONFIG_FLASH_ADDR     0x1000      // Flash storage address
#define EQUIPMENT_CONFIG_UPDATE_INTERVAL 60000      // Default 60s (1 minute)

/* Equipment Limits ----------------------------------------------------------*/
#define MAX_COMPRESSORS                 8           // Maximum compressors supported
#define MAX_CONDENSER_BANKS             4           // Maximum condenser banks  
#define MAX_TEMPERATURE_SENSORS         8           // Maximum temperature sensors
#define MAX_PRESSURE_SENSORS            4           // Maximum pressure sensors

/* Hot Climate Configuration (38°C Baseline) --------------------------------*/
#define DEFAULT_AMBIENT_TEMP            38.0f       // 38°C baseline ambient
#define DEFAULT_SUPPLY_SETPOINT         7.0f        // 7°C supply water (hot climate)
#define DEFAULT_RETURN_SETPOINT         12.0f       // 12°C return water (hot climate)
#define DEFAULT_TEMP_TOLERANCE_ECO      1.5f        // Economic mode tolerance
#define DEFAULT_TEMP_TOLERANCE_NORMAL   1.0f        // Normal mode tolerance  
#define DEFAULT_TEMP_TOLERANCE_FULL     0.5f        // Full mode tolerance
#define DEFAULT_HIGH_AMBIENT_LIMIT      40.0f       // High ambient alarm limit

/* Data Types ----------------------------------------------------------------*/

/**
 * @brief Equipment Capacity Control Modes (Four-Tier System)
 */
typedef enum {
    CAPACITY_MODE_ECONOMIC = 0,     // 25% - Max 2 compressors, energy focused
    CAPACITY_MODE_NORMAL,           // 50% - Max 4 compressors, balanced approach
    CAPACITY_MODE_FULL,             // 75% - Max 6 compressors, performance focused  
    CAPACITY_MODE_CUSTOM            // 100% - Max 8 compressors, user configurable
} CapacityMode_t;

/**
 * @brief Equipment Configuration Status
 */
typedef enum {
    EQUIPMENT_STATUS_OK = 0,
    EQUIPMENT_STATUS_ERROR,
    EQUIPMENT_STATUS_FLASH_ERROR,
    EQUIPMENT_STATUS_INVALID_CONFIG,
    EQUIPMENT_STATUS_SENSOR_FAULT
} EquipmentStatus_t;

/**
 * @brief Individual Equipment Configuration
 */
typedef struct {
    uint8_t installed;              // Equipment installed (1=yes, 0=no)
    uint8_t enabled;                // Equipment enabled (1=yes, 0=no) 
    uint32_t runtime_hours;         // Total runtime hours
    uint32_t start_cycles;          // Total start cycles
    uint8_t maintenance_due;        // Maintenance required flag
    uint32_t last_maintenance;      // Last maintenance timestamp
} EquipmentItem_t;

/**
 * @brief Temperature Sensor Configuration  
 */
typedef struct {
    uint8_t supply_sensor_enabled;      // Supply water temp sensor
    uint8_t return_sensor_enabled;      // Return water temp sensor
    uint8_t ambient_sensor_enabled;     // Ambient air temp sensor
    uint8_t compressor_sensors_enabled; // Individual compressor temp sensors
    uint8_t condenser_sensors_enabled;  // Individual condenser temp sensors
    uint8_t oil_temp_sensors_enabled;   // Compressor oil temp sensors
    uint8_t sensor_fault_tolerance;     // Continue operation with failed sensors
    float sensor_calibration_offset[MAX_TEMPERATURE_SENSORS]; // Calibration offsets
} SensorConfig_t;

/**
 * @brief Capacity Mode Configuration
 */
typedef struct {
    uint8_t max_compressors;            // Maximum compressors for this mode
    uint8_t max_condenser_banks;        // Maximum condenser banks for this mode
    float temp_tolerance;               // Temperature control tolerance (°C)
    uint32_t staging_delay_ms;          // Delay between equipment staging (ms)
    uint8_t energy_optimization;        // Energy optimization enabled
    float ambient_compensation_factor;  // Ambient temperature compensation factor
} CapacityModeConfig_t;

/**
 * @brief Complete Equipment Configuration Structure
 */
typedef struct {
    // Configuration Header
    uint16_t version;                   // Configuration version
    uint32_t timestamp;                 // Configuration timestamp
    uint32_t crc32;                     // Configuration CRC32 checksum
    
    // System Configuration
    CapacityMode_t current_mode;        // Current capacity mode
    uint8_t total_compressors_installed; // Total compressors installed (2/4/6/8)
    uint8_t total_condenser_banks;      // Total condenser banks installed (1-4)
    
    // Equipment Arrays
    EquipmentItem_t compressors[MAX_COMPRESSORS];       // Compressor configurations
    EquipmentItem_t condenser_banks[MAX_CONDENSER_BANKS]; // Condenser configurations
    
    // Temperature Configuration (38°C Hot Climate Baseline)
    float supply_water_setpoint;       // Supply water temperature setpoint (°C)
    float return_water_setpoint;       // Return water temperature setpoint (°C)
    float ambient_temp_baseline;       // Ambient temperature baseline (38°C)
    float high_ambient_alarm_limit;    // High ambient temperature alarm (°C)
    
    // Mode-Specific Configurations  
    CapacityModeConfig_t mode_configs[4]; // Configurations for each capacity mode
    
    // Sensor Configuration
    SensorConfig_t sensor_config;      // Temperature sensor configuration
    
    // System Settings
    uint32_t config_update_interval;   // Flash update interval (ms)
    uint8_t auto_mode_switching;       // Automatic mode switching based on load
    uint8_t maintenance_mode;          // System in maintenance mode
    
    // Statistics
    uint32_t total_system_runtime;     // Total system runtime hours
    uint32_t configuration_changes;    // Number of configuration changes
    uint32_t system_start_count;       // System start counter
} EquipmentConfig_t;

/* Global Variables ----------------------------------------------------------*/
extern EquipmentConfig_t g_equipment_config;
extern uint8_t g_config_initialized;

/* Function Prototypes -------------------------------------------------------*/

/**
 * @brief Initialize Equipment Configuration System
 * @return EquipmentStatus_t - Status of initialization
 */
EquipmentStatus_t EquipmentConfig_Init(void);

/**
 * @brief Load Configuration from Flash Memory
 * @return EquipmentStatus_t - Status of load operation
 */
EquipmentStatus_t EquipmentConfig_LoadFromFlash(void);

/**
 * @brief Save Configuration to Flash Memory  
 * @return EquipmentStatus_t - Status of save operation
 */
EquipmentStatus_t EquipmentConfig_SaveToFlash(void);

/**
 * @brief Load Default Configuration (38°C Hot Climate Optimized)
 * @return EquipmentStatus_t - Status of operation
 */
EquipmentStatus_t EquipmentConfig_LoadDefaults(void);

/**
 * @brief Set Capacity Control Mode
 * @param mode - Desired capacity mode
 * @return EquipmentStatus_t - Status of operation
 */
EquipmentStatus_t EquipmentConfig_SetCapacityMode(CapacityMode_t mode);

/**
 * @brief Get Current Capacity Control Mode
 * @return CapacityMode_t - Current capacity mode
 */
CapacityMode_t EquipmentConfig_GetCapacityMode(void);

/**
 * @brief Get Maximum Compressors for Current Mode
 * @return uint8_t - Maximum compressors allowed
 */
uint8_t EquipmentConfig_GetMaxCompressors(void);

/**
 * @brief Get Maximum Condenser Banks for Current Mode
 * @return uint8_t - Maximum condenser banks allowed
 */
uint8_t EquipmentConfig_GetMaxCondenserBanks(void);

EquipmentConfig_t* EquipmentConfig_GetConfig(void);

/**
 * @brief Set Equipment Installation Status
 * @param equipment_type - 0=compressor, 1=condenser_bank
 * @param equipment_index - Equipment index (0-7 for compressors, 0-3 for condensers)
 * @param installed - Installation status (1=installed, 0=not installed)
 * @return EquipmentStatus_t - Status of operation
 */
EquipmentStatus_t EquipmentConfig_SetEquipmentInstalled(uint8_t equipment_type, 
                                                       uint8_t equipment_index, 
                                                       uint8_t installed);

/**
 * @brief Update Equipment Runtime Hours
 * @param equipment_type - 0=compressor, 1=condenser_bank
 * @param equipment_index - Equipment index
 * @param runtime_hours - New runtime hours
 * @return EquipmentStatus_t - Status of operation
 */
EquipmentStatus_t EquipmentConfig_UpdateRuntimeHours(uint8_t equipment_type,
                                                    uint8_t equipment_index,
                                                    uint32_t runtime_hours);

/**
 * @brief Set Temperature Sensor Availability
 * @param sensor_type - Sensor type (see SensorConfig_t structure)
 * @param enabled - Sensor enabled status
 * @return EquipmentStatus_t - Status of operation
 */
EquipmentStatus_t EquipmentConfig_SetSensorEnabled(uint8_t sensor_type, uint8_t enabled);

/**
 * @brief Get Temperature Setpoints for Current Mode
 * @param supply_setpoint - Pointer to store supply setpoint
 * @param return_setpoint - Pointer to store return setpoint
 * @param tolerance - Pointer to store temperature tolerance
 */
void EquipmentConfig_GetTemperatureSetpoints(float *supply_setpoint, 
                                           float *return_setpoint,
                                           float *tolerance);

/**
 * @brief Validate Configuration Integrity
 * @return EquipmentStatus_t - Validation status
 */
EquipmentStatus_t EquipmentConfig_ValidateConfig(void);

/**
 * @brief Process Periodic Configuration Tasks
 * @note Call this function periodically to handle flash updates
 */
void EquipmentConfig_ProcessPeriodicTasks(void);

/**
 * @brief Display Current Configuration Status
 * @note Sends configuration info to debug UART
 */
void EquipmentConfig_DisplayStatus(void);

/**
 * @brief Get Configuration Statistics  
 * @param total_runtime - Pointer to store total system runtime
 * @param config_changes - Pointer to store configuration change count
 * @param start_count - Pointer to store system start count
 */
void EquipmentConfig_GetStatistics(uint32_t *total_runtime,
                                  uint32_t *config_changes, 
                                  uint32_t *start_count);

/**
 * @brief Reset Configuration to Factory Defaults
 * @return EquipmentStatus_t - Status of reset operation
 */
EquipmentStatus_t EquipmentConfig_FactoryReset(void);

/**
 * @brief Calculate Configuration CRC32
 * @return uint32_t - CRC32 checksum of configuration
 */
uint32_t EquipmentConfig_CalculateCRC32(void);

/**
 * @brief Check if compressor is installed
 * @param compressor_id: Compressor ID to check
 * @return 1 if installed, 0 if not installed
 */
uint8_t EquipmentConfig_IsCompressorInstalled(uint8_t compressor_id);

#ifdef __cplusplus
}
#endif

#endif /* EQUIPMENT_CONFIG_H */
