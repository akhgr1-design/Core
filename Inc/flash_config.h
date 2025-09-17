/**
  ******************************************************************************
  * @file    flash_config.h
  * @brief   Flash Configuration Management System for Chiller Control
  * @author  Industrial Chiller Control System
  * @version 1.0
  * @date    2025
  * 
  * @description
  * High-level flash configuration management system that provides:
  * - User settings storage with 1-minute configurable updates
  * - Sensor data logging every 8 hours to flash 
  * - Runtime hours tracking for all equipment
  * - Configuration backup/restore functionality
  * - SD Card integration for extended logging (if available)
  * - Data integrity with CRC verification
  ******************************************************************************
  */

#ifndef FLASH_CONFIG_H
#define FLASH_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "flash_25q16.h"
#include "equipment_config.h"
#include "uart_comm.h"
#include "sd_card.h"
#include <stdint.h>
#include <string.h>

/* Flash Configuration Constants ---------------------------------------------*/
#define FLASH_CONFIG_VERSION            0x0100      // Version 1.0
#define FLASH_CONFIG_SECTOR_SIZE        4096        // 4KB sector size
#define FLASH_CONFIG_PAGE_SIZE          256         // 256 byte page size

/* Flash Memory Map (25Q16 = 2MB total) -------------------------------------*/
#define FLASH_USER_CONFIG_ADDR          0x001000    // User settings (4KB)
#define FLASH_SYSTEM_CONFIG_ADDR        0x002000    // System configuration (4KB) 
#define FLASH_RUNTIME_DATA_ADDR         0x003000    // Equipment runtime data (4KB)
#define FLASH_SENSOR_LOG_ADDR           0x004000    // Sensor data logs (16KB)
#define FLASH_ALARM_LOG_ADDR            0x008000    // Alarm history logs (8KB)
#define FLASH_BACKUP_CONFIG_ADDR        0x00A000    // Configuration backup (4KB)

/* Data Storage Intervals ----------------------------------------------------*/
#define DEFAULT_USER_SAVE_INTERVAL      60000       // 1 minute (configurable)
#define DEFAULT_SENSOR_LOG_INTERVAL     28800000    // 8 hours
#define DEFAULT_RUNTIME_SAVE_INTERVAL   900000      // 15 minutes
#define DEFAULT_ALARM_SAVE_INTERVAL     5000        // 5 seconds (immediate)

/* Configuration Limits ------------------------------------------------------*/
#define MAX_SENSOR_LOG_ENTRIES          100         // Maximum sensor log entries
#define MAX_ALARM_LOG_ENTRIES           50          // Maximum alarm log entries  
#define MAX_CONFIG_CHANGES              1000        // Configuration change tracking

/* Data Types ----------------------------------------------------------------*/

/**
 * @brief Flash Configuration Status
 */
typedef enum {
    FLASH_CONFIG_OK = 0,
    FLASH_CONFIG_ERROR,
    FLASH_CONFIG_FULL,
    FLASH_CONFIG_CORRUPTED,
    FLASH_CONFIG_NOT_FOUND,
    FLASH_CONFIG_WRITE_PROTECTED,
    FLASH_CONFIG_SD_ERROR
} FlashConfig_Status_t;

/**
 * @brief User Configuration Data Structure
 */
typedef struct {
    uint16_t version;                   // Configuration version
    uint32_t timestamp;                 // Last update timestamp
    uint32_t crc32;                     // Data integrity checksum
    
    // User Setpoints
    float supply_setpoint;              // Supply water temperature (°C)
    float return_setpoint;              // Return water temperature (°C)
    float ambient_threshold;            // High ambient alarm threshold (°C)
    
    // Operating Mode Settings
    CapacityMode_t default_mode;        // Default capacity mode
    uint8_t auto_mode_enabled;          // Auto mode switching enabled
    uint32_t mode_switch_delay;         // Mode switch delay (seconds)
    
    // Update Intervals (configurable by user)
    uint32_t config_save_interval;      // Configuration save interval (ms)
    uint32_t sensor_log_interval;       // Sensor logging interval (ms)
    uint32_t runtime_save_interval;     // Runtime hours save interval (ms)
    
    // System Preferences
    uint8_t debug_enabled;              // Debug output enabled
    uint8_t sd_logging_enabled;         // SD card logging enabled
    uint8_t network_enabled;            // Network functionality enabled
    uint8_t hmi_enabled;                // HMI interface enabled
    
    // Safety Settings
    float max_supply_temp;              // Maximum supply temperature limit
    float min_supply_temp;              // Minimum supply temperature limit
    uint32_t safety_check_interval;     // Safety check interval (ms)
    
} UserConfig_t;

/**
 * @brief Sensor Data Log Entry
 */
typedef struct {
    uint32_t timestamp;                 // Log entry timestamp
    float supply_temp;                  // Supply water temperature (°C)
    float return_temp;                  // Return water temperature (°C)
    float ambient_temp;                 // Ambient temperature (°C)
    uint16_t pressure;                  // System pressure
    uint16_t flow_rate;                 // Flow rate
    uint8_t active_compressors;         // Number of active compressors
    uint8_t active_condensers;          // Number of active condensers
    CapacityMode_t current_mode;        // Current capacity mode
    uint16_t system_status;             // System status bits
} SensorLogEntry_t;

/**
 * @brief Runtime Data Structure
 */
typedef struct {
    uint16_t version;                   // Data structure version
    uint32_t timestamp;                 // Last update timestamp
    uint32_t crc32;                     // Data integrity checksum
    
    // Equipment Runtime Hours
    uint32_t compressor_hours[MAX_COMPRESSORS];     // Individual compressor hours
    uint32_t condenser_hours[MAX_CONDENSER_BANKS];  // Individual condenser hours
    uint32_t system_total_hours;        // Total system runtime hours
    
    // Equipment Start Counts
    uint32_t compressor_starts[MAX_COMPRESSORS];    // Compressor start counts
    uint32_t condenser_starts[MAX_CONDENSER_BANKS]; // Condenser start counts
    uint32_t system_start_count;        // Total system starts
    
    // Performance Statistics
    float total_cooling_hours;          // Total cooling hours delivered
    uint32_t mode_hours[4];             // Hours in each capacity mode
    uint32_t fault_count;               // Total fault occurrences
    uint32_t alarm_count;               // Total alarm occurrences
    
} RuntimeData_t;

/**
 * @brief Alarm Log Entry
 */
typedef struct {
    uint32_t timestamp;                 // Alarm timestamp
    uint16_t alarm_code;                // Alarm identification code
    uint8_t alarm_severity;             // Alarm severity (1-5)
    uint8_t alarm_source;               // Alarm source (equipment ID)
    float trigger_value;                // Value that triggered alarm
    uint8_t system_state;               // System state when alarm occurred
    char alarm_description[32];         // Alarm description text
} AlarmLogEntry_t;

/**
 * @brief Flash Configuration System Structure
 */
typedef struct {
    // System Status
    uint8_t initialized;                // System initialized flag
    uint8_t sd_available;               // SD card available flag
    uint32_t last_save_time;            // Last configuration save time
    uint32_t save_counter;              // Total saves performed
    
    // Data Structures  
    UserConfig_t user_config;           // User configuration data
    RuntimeData_t runtime_data;         // Equipment runtime data
    SensorLogEntry_t sensor_logs[MAX_SENSOR_LOG_ENTRIES];  // Sensor data logs
    AlarmLogEntry_t alarm_logs[MAX_ALARM_LOG_ENTRIES];     // Alarm history logs
    
    // Counters and Indexes
    uint16_t sensor_log_count;          // Current sensor log count
    uint16_t sensor_log_index;          // Current sensor log write index
    uint16_t alarm_log_count;           // Current alarm log count
    uint16_t alarm_log_index;           // Current alarm log write index
    
} FlashConfigSystem_t;

/* Global Variables ----------------------------------------------------------*/
extern FlashConfigSystem_t g_flash_config;
extern uint8_t g_flash_config_initialized;

/* Function Prototypes -------------------------------------------------------*/

/* === INITIALIZATION FUNCTIONS === */
FlashConfig_Status_t FlashConfig_Init(void);
FlashConfig_Status_t FlashConfig_LoadFromFlash(void);
FlashConfig_Status_t FlashConfig_InitializeDefaults(void);

/* === USER CONFIGURATION FUNCTIONS === */
FlashConfig_Status_t FlashConfig_SaveUserConfig(void);
FlashConfig_Status_t FlashConfig_LoadUserConfig(void);
FlashConfig_Status_t FlashConfig_SetUserSetpoint(float supply_temp, float return_temp);
FlashConfig_Status_t FlashConfig_SetCapacityMode(CapacityMode_t mode);
FlashConfig_Status_t FlashConfig_SetUpdateIntervals(uint32_t config_ms, uint32_t sensor_ms, uint32_t runtime_ms);

/* === RUNTIME DATA FUNCTIONS === */
FlashConfig_Status_t FlashConfig_SaveRuntimeData(void);
FlashConfig_Status_t FlashConfig_LoadRuntimeData(void);
FlashConfig_Status_t FlashConfig_UpdateEquipmentHours(uint8_t equipment_type, uint8_t equipment_id, uint32_t additional_hours);
FlashConfig_Status_t FlashConfig_IncrementStartCount(uint8_t equipment_type, uint8_t equipment_id);
void FlashConfig_GetEquipmentHours(uint8_t equipment_type, uint8_t equipment_id, uint32_t *hours, uint32_t *starts);

/* === SENSOR LOGGING FUNCTIONS === */
FlashConfig_Status_t FlashConfig_LogSensorData(float supply_temp, float return_temp, float ambient_temp,
                                              uint16_t pressure, uint16_t flow_rate,
                                              uint8_t active_compressors, uint8_t active_condensers,
                                              CapacityMode_t mode, uint16_t status);
FlashConfig_Status_t FlashConfig_SaveSensorLogs(void);
FlashConfig_Status_t FlashConfig_LoadSensorLogs(void);
void FlashConfig_GetLatestSensorData(SensorLogEntry_t *latest_entry);

/* === ALARM LOGGING FUNCTIONS === */
FlashConfig_Status_t FlashConfig_LogAlarm(uint16_t alarm_code, uint8_t severity, uint8_t source,
                                         float trigger_value, const char *description);
FlashConfig_Status_t FlashConfig_SaveAlarmLogs(void);
FlashConfig_Status_t FlashConfig_LoadAlarmLogs(void);
void FlashConfig_GetLatestAlarm(AlarmLogEntry_t *latest_alarm);

/* === SD CARD INTEGRATION FUNCTIONS === */
FlashConfig_Status_t FlashConfig_WriteToSDCard(void);
FlashConfig_Status_t FlashConfig_BackupToSDCard(void);
FlashConfig_Status_t FlashConfig_RestoreFromSDCard(void);

/* === PERIODIC PROCESSING FUNCTIONS === */
void FlashConfig_ProcessPeriodicTasks(void);
void FlashConfig_CheckSaveIntervals(void);
void FlashConfig_PerformScheduledSaves(void);

/* === UTILITY FUNCTIONS === */
FlashConfig_Status_t FlashConfig_ValidateIntegrity(void);
uint32_t FlashConfig_CalculateCRC32(const uint8_t *data, uint32_t length);
void FlashConfig_DisplayStatus(void);
void FlashConfig_DisplayUserConfig(void);
void FlashConfig_DisplayRuntimeData(void);
void FlashConfig_DisplaySensorLogs(uint16_t count);
void FlashConfig_DisplayAlarmLogs(uint16_t count);

/* === BACKUP AND RECOVERY FUNCTIONS === */
FlashConfig_Status_t FlashConfig_CreateBackup(void);
FlashConfig_Status_t FlashConfig_RestoreBackup(void);
FlashConfig_Status_t FlashConfig_FactoryReset(void);

/* === DEBUG COMMAND FUNCTIONS === */
void FlashConfig_ProcessDebugCommand(const char *command);
void FlashConfig_ShowDebugCommands(void);
void FlashConfig_LogEvent(const char* system, const char* event, uint8_t level);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_CONFIG_H */
