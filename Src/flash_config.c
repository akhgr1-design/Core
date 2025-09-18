/**
  ******************************************************************************
  * @file    flash_config.c
  * @brief   Flash Configuration Management System Implementation
  * @author  Industrial Chiller Control System
  * @version 1.0
  * @date    2025
  ******************************************************************************
  */

#include "flash_config.h"
#include <stdio.h>
#include <time.h>

/* Private Constants ---------------------------------------------------------*/
#define CONFIG_CRC32_POLYNOMIAL         0xEDB88320  // Standard CRC32 polynomial
#define CONFIG_MAGIC_NUMBER             0xFC0NFIG   // Flash config magic number

/* Private Variables ---------------------------------------------------------*/
static uint32_t last_user_save_time = 0;
static uint32_t last_sensor_log_time = 0;
static uint32_t last_runtime_save_time = 0;
static uint8_t config_dirty_flag = 0;

/* Global Flash Configuration System -----------------------------------------*/
FlashConfigSystem_t g_flash_config = {0};
uint8_t g_flash_config_initialized = 0;

/* Private Function Prototypes -----------------------------------------------*/
static uint32_t FlashConfig_CRC32(const uint8_t *data, uint32_t length);
static void FlashConfig_SetDefaults(void);
static FlashConfig_Status_t FlashConfig_ValidateData(void);

/* === INITIALIZATION FUNCTIONS === */

/**
 * @brief Initialize Flash Configuration System
 */
FlashConfig_Status_t FlashConfig_Init(void)
{
    Send_Debug_Data("=== FLASH CONFIG INITIALIZATION ===\r\n");
    
    // Initialize flash configuration system
    memset(&g_flash_config, 0, sizeof(FlashConfigSystem_t));
    
    // Try to load existing configuration from flash
    FlashConfig_Status_t status = FlashConfig_LoadFromFlash();
    
    if (status != FLASH_CONFIG_OK) {
        Send_Debug_Data("Flash Config: No valid config found, initializing defaults\r\n");
        
        // Initialize with default values
        status = FlashConfig_InitializeDefaults();
        if (status == FLASH_CONFIG_OK) {
            // Save default configuration to flash
            FlashConfig_SaveUserConfig();
            FlashConfig_SaveRuntimeData();
        }
    }
    
    // Check SD card availability
    g_flash_config.sd_available = (SD_Card_Get_Status() == SD_CARD_OK) ? 1 : 0;
    
    // Mark system as initialized
    g_flash_config.initialized = 1;
    g_flash_config_initialized = 1;
    
    Send_Debug_Data("Flash Config: System initialized successfully\r\n");
    FlashConfig_DisplayStatus();
    
    return FLASH_CONFIG_OK;
}

/**
 * @brief Load Configuration from Flash Memory
 */
FlashConfig_Status_t FlashConfig_LoadFromFlash(void)
{
    uint8_t flash_data[sizeof(UserConfig_t)];
    
    // Load user configuration
    if (Flash_ReadData(FLASH_USER_CONFIG_ADDR, flash_data, sizeof(UserConfig_t)) != 0) {
        return FLASH_CONFIG_ERROR;
    }
    
    // Copy and validate user config
    memcpy(&g_flash_config.user_config, flash_data, sizeof(UserConfig_t));
    
    if (g_flash_config.user_config.version != FLASH_CONFIG_VERSION) {
        return FLASH_CONFIG_CORRUPTED;
    }
    
    // Validate CRC
    uint32_t calculated_crc = FlashConfig_CRC32((uint8_t*)&g_flash_config.user_config, 
                                              sizeof(UserConfig_t) - sizeof(uint32_t));
    if (calculated_crc != g_flash_config.user_config.crc32) {
        return FLASH_CONFIG_CORRUPTED;
    }
    
    // Load runtime data
    FlashConfig_LoadRuntimeData();
    
    // Load sensor logs
    FlashConfig_LoadSensorLogs();
    
    // Load alarm logs
    FlashConfig_LoadAlarmLogs();
    
    Send_Debug_Data("Flash Config: Configuration loaded from flash\r\n");
    return FLASH_CONFIG_OK;
}

/**
 * @brief Initialize Default Configuration
 */
FlashConfig_Status_t FlashConfig_InitializeDefaults(void)
{
    FlashConfig_SetDefaults();
    
    Send_Debug_Data("Flash Config: Default configuration initialized\r\n");
    return FLASH_CONFIG_OK;
}

/* === USER CONFIGURATION FUNCTIONS === */

/**
 * @brief Save User Configuration to Flash
 */
FlashConfig_Status_t FlashConfig_SaveUserConfig(void)
{
    // Update timestamp and CRC
    g_flash_config.user_config.timestamp = HAL_GetTick();
    g_flash_config.user_config.crc32 = FlashConfig_CRC32((uint8_t*)&g_flash_config.user_config,
                                                        sizeof(UserConfig_t) - sizeof(uint32_t));
    
    // Write to flash
    if (Flash_WritePage(FLASH_USER_CONFIG_ADDR, (uint8_t*)&g_flash_config.user_config,
                       sizeof(UserConfig_t)) != 0) {
        return FLASH_CONFIG_ERROR;
    }
    
    last_user_save_time = HAL_GetTick();
    g_flash_config.save_counter++;
    
    Send_Debug_Data("Flash Config: User configuration saved\r\n");
    return FLASH_CONFIG_OK;
}

/**
 * @brief Set User Temperature Setpoints
 */
FlashConfig_Status_t FlashConfig_SetUserSetpoint(float supply_temp, float return_temp)
{
    // Validate temperature ranges
    if (supply_temp < 4.0f || supply_temp > 15.0f ||
        return_temp < 8.0f || return_temp > 25.0f ||
        return_temp <= supply_temp) {
        return FLASH_CONFIG_ERROR;
    }
    
    g_flash_config.user_config.supply_setpoint = supply_temp;
    g_flash_config.user_config.return_setpoint = return_temp;
    config_dirty_flag = 1;
    
    char msg[100];
    snprintf(msg, sizeof(msg), "Flash Config: Setpoints updated - Supply: %.1f°C, Return: %.1f°C\r\n",
             supply_temp, return_temp);
    Send_Debug_Data(msg);
    
    return FLASH_CONFIG_OK;
}

/**
 * @brief Set Capacity Mode
 */
FlashConfig_Status_t FlashConfig_SetCapacityMode(CapacityMode_t mode)
{
    if (mode > CAPACITY_MODE_CUSTOM) {
        return FLASH_CONFIG_ERROR;
    }
    
    g_flash_config.user_config.default_mode = mode;
    config_dirty_flag = 1;
    
    const char *mode_names[] = {"Economic", "Normal", "Full", "Custom"};
    char msg[80];
    snprintf(msg, sizeof(msg), "Flash Config: Default mode set to %s\r\n", mode_names[mode]);
    Send_Debug_Data(msg);
    
    return FLASH_CONFIG_OK;
}

/**
 * @brief Set Update Intervals
 */
FlashConfig_Status_t FlashConfig_SetUpdateIntervals(uint32_t config_ms, uint32_t sensor_ms, uint32_t runtime_ms)
{
    // Validate intervals (minimum 10 seconds, maximum 24 hours)
    if (config_ms < 10000 || config_ms > 86400000 ||
        sensor_ms < 10000 || sensor_ms > 86400000 ||
        runtime_ms < 10000 || runtime_ms > 86400000) {
        return FLASH_CONFIG_ERROR;
    }
    
    g_flash_config.user_config.config_save_interval = config_ms;
    g_flash_config.user_config.sensor_log_interval = sensor_ms;
    g_flash_config.user_config.runtime_save_interval = runtime_ms;
    config_dirty_flag = 1;
    
    Send_Debug_Data("Flash Config: Update intervals configured\r\n");
    return FLASH_CONFIG_OK;
}

/* === RUNTIME DATA FUNCTIONS === */

/**
 * @brief Save Runtime Data to Flash
 */
FlashConfig_Status_t FlashConfig_SaveRuntimeData(void)
{
    // Update timestamp and CRC
    g_flash_config.runtime_data.timestamp = HAL_GetTick();
    g_flash_config.runtime_data.crc32 = FlashConfig_CRC32((uint8_t*)&g_flash_config.runtime_data,
                                                         sizeof(RuntimeData_t) - sizeof(uint32_t));
    
    // Write to flash
    if (Flash_WritePage(FLASH_RUNTIME_DATA_ADDR, (uint8_t*)&g_flash_config.runtime_data,
                       sizeof(RuntimeData_t)) != 0) {
        return FLASH_CONFIG_ERROR;
    }
    
    last_runtime_save_time = HAL_GetTick();
    
    Send_Debug_Data("Flash Config: Runtime data saved\r\n");
    return FLASH_CONFIG_OK;
}

/**
 * @brief Load Runtime Data from Flash
 */
FlashConfig_Status_t FlashConfig_LoadRuntimeData(void)
{
    uint8_t flash_data[sizeof(RuntimeData_t)];
    
    if (Flash_ReadData(FLASH_RUNTIME_DATA_ADDR, flash_data, sizeof(RuntimeData_t)) != 0) {
        return FLASH_CONFIG_ERROR;
    }
    
    memcpy(&g_flash_config.runtime_data, flash_data, sizeof(RuntimeData_t));
    
    // Validate version and CRC
    if (g_flash_config.runtime_data.version != FLASH_CONFIG_VERSION) {
        return FLASH_CONFIG_CORRUPTED;
    }
    
    uint32_t calculated_crc = FlashConfig_CRC32((uint8_t*)&g_flash_config.runtime_data,
                                              sizeof(RuntimeData_t) - sizeof(uint32_t));
    if (calculated_crc != g_flash_config.runtime_data.crc32) {
        return FLASH_CONFIG_CORRUPTED;
    }
    
    return FLASH_CONFIG_OK;
}

/**
 * @brief Update Equipment Runtime Hours
 */
FlashConfig_Status_t FlashConfig_UpdateEquipmentHours(uint8_t equipment_type, uint8_t equipment_id, uint32_t additional_hours)
{
    if (equipment_type == 0 && equipment_id < MAX_COMPRESSORS) {
        // Compressor
        g_flash_config.runtime_data.compressor_hours[equipment_id] += additional_hours;
    } else if (equipment_type == 1 && equipment_id < MAX_CONDENSER_BANKS) {
        // Condenser
        g_flash_config.runtime_data.condenser_hours[equipment_id] += additional_hours;
    } else {
        return FLASH_CONFIG_ERROR;
    }
    
    g_flash_config.runtime_data.system_total_hours += additional_hours;
    return FLASH_CONFIG_OK;
}

/**
 * @brief Increment Equipment Start Count
 */
FlashConfig_Status_t FlashConfig_IncrementStartCount(uint8_t equipment_type, uint8_t equipment_id)
{
    if (equipment_type == 0 && equipment_id < MAX_COMPRESSORS) {
        // Compressor
        g_flash_config.runtime_data.compressor_starts[equipment_id]++;
    } else if (equipment_type == 1 && equipment_id < MAX_CONDENSER_BANKS) {
        // Condenser
        g_flash_config.runtime_data.condenser_starts[equipment_id]++;
    } else {
        return FLASH_CONFIG_ERROR;
    }
    
    g_flash_config.runtime_data.system_start_count++;
    return FLASH_CONFIG_OK;
}

/**
 * @brief Get Equipment Hours and Start Count
 */
void FlashConfig_GetEquipmentHours(uint8_t equipment_type, uint8_t equipment_id, uint32_t *hours, uint32_t *starts)
{
    if (equipment_type == 0 && equipment_id < MAX_COMPRESSORS) {
        // Compressor
        if (hours) *hours = g_flash_config.runtime_data.compressor_hours[equipment_id];
        if (starts) *starts = g_flash_config.runtime_data.compressor_starts[equipment_id];
    } else if (equipment_type == 1 && equipment_id < MAX_CONDENSER_BANKS) {
        // Condenser
        if (hours) *hours = g_flash_config.runtime_data.condenser_hours[equipment_id];
        if (starts) *starts = g_flash_config.runtime_data.condenser_starts[equipment_id];
    } else {
        if (hours) *hours = 0;
        if (starts) *starts = 0;
    }
}

/* === SENSOR LOGGING FUNCTIONS === */

/**
 * @brief Log Sensor Data
 */
FlashConfig_Status_t FlashConfig_LogSensorData(float supply_temp, float return_temp, float ambient_temp,
                                              uint16_t pressure, uint16_t flow_rate,
                                              uint8_t active_compressors, uint8_t active_condensers,
                                              CapacityMode_t mode, uint16_t status)
{
    // Check if buffer is full
    if (g_flash_config.sensor_log_count >= MAX_SENSOR_LOG_ENTRIES) {
        // Overwrite oldest entry (circular buffer)
        g_flash_config.sensor_log_index = 0;
        g_flash_config.sensor_log_count = MAX_SENSOR_LOG_ENTRIES;
    }
    
    // Create new log entry
    SensorLogEntry_t *entry = &g_flash_config.sensor_logs[g_flash_config.sensor_log_index];
    entry->timestamp = HAL_GetTick();
    entry->supply_temp = supply_temp;
    entry->return_temp = return_temp;
    entry->ambient_temp = ambient_temp;
    entry->pressure = pressure;
    entry->flow_rate = flow_rate;
    entry->active_compressors = active_compressors;
    entry->active_condensers = active_condensers;
    entry->current_mode = mode;
    entry->system_status = status;
    
    // Update counters
    g_flash_config.sensor_log_index++;
    if (g_flash_config.sensor_log_count < MAX_SENSOR_LOG_ENTRIES) {
        g_flash_config.sensor_log_count++;
    }
    
    return FLASH_CONFIG_OK;
}

/**
 * @brief Log Alarm Event
 */
FlashConfig_Status_t FlashConfig_LogAlarm(uint16_t alarm_code, uint8_t severity, uint8_t source,
                                         float trigger_value, const char *description)
{
    // Check if buffer is full
    if (g_flash_config.alarm_log_count >= MAX_ALARM_LOG_ENTRIES) {
        // Overwrite oldest entry (circular buffer)
        g_flash_config.alarm_log_index = 0;
        g_flash_config.alarm_log_count = MAX_ALARM_LOG_ENTRIES;
    }
    
    // Create new alarm entry
    AlarmLogEntry_t *entry = &g_flash_config.alarm_logs[g_flash_config.alarm_log_index];
    entry->timestamp = HAL_GetTick();
    entry->alarm_code = alarm_code;
    entry->alarm_severity = severity;
    entry->alarm_source = source;
    entry->trigger_value = trigger_value;
    
    // Copy description safely
    strncpy(entry->alarm_description, description, sizeof(entry->alarm_description) - 1);
    entry->alarm_description[sizeof(entry->alarm_description) - 1] = '\0';
    
    // Update counters
    g_flash_config.alarm_log_index++;
    if (g_flash_config.alarm_log_count < MAX_ALARM_LOG_ENTRIES) {
        g_flash_config.alarm_log_count++;
    }
    
    // Increment alarm counter in runtime data
    g_flash_config.runtime_data.alarm_count++;
    
    // Save alarm log immediately (critical data)
    FlashConfig_SaveAlarmLogs();
    
    char msg[100];
    snprintf(msg, sizeof(msg), "Flash Config: Alarm logged - Code: %u, Severity: %u\r\n",
             alarm_code, severity);
    Send_Debug_Data(msg);
    
    return FLASH_CONFIG_OK;
}

/* === PERIODIC PROCESSING FUNCTIONS === */

/**
 * @brief Process Periodic Flash Configuration Tasks
 */
void FlashConfig_ProcessPeriodicTasks(void)
{
    if (!g_flash_config_initialized) return;
    
    FlashConfig_CheckSaveIntervals();
    
    // Update system runtime every hour (3600000 ms)
    static uint32_t last_hour_update = 0;
    if (HAL_GetTick() - last_hour_update >= 3600000) {
        last_hour_update = HAL_GetTick();
        g_flash_config.runtime_data.system_total_hours++;
    }
}

/**
 * @brief Check and Perform Scheduled Saves
 */
void FlashConfig_CheckSaveIntervals(void)
{
    uint32_t current_time = HAL_GetTick();
    
    // Check user configuration save interval
    if (config_dirty_flag && 
        (current_time - last_user_save_time) >= g_flash_config.user_config.config_save_interval) {
        FlashConfig_SaveUserConfig();
        config_dirty_flag = 0;
    }
    
    // Check sensor log save interval
    if ((current_time - last_sensor_log_time) >= g_flash_config.user_config.sensor_log_interval) {
        last_sensor_log_time = current_time;
        FlashConfig_SaveSensorLogs();
    }
    
    // Check runtime data save interval
    if ((current_time - last_runtime_save_time) >= g_flash_config.user_config.runtime_save_interval) {
        FlashConfig_SaveRuntimeData();
    }
}

/* === UTILITY FUNCTIONS === */

/**
 * @brief Display Flash Configuration Status
 */
void FlashConfig_DisplayStatus(void)
{
    char msg[120];
    
    Send_Debug_Data("\r\n=== FLASH CONFIGURATION STATUS ===\r\n");
    
    snprintf(msg, sizeof(msg), "System: %s, SD Card: %s\r\n",
             g_flash_config.initialized ? "READY" : "NOT READY",
             g_flash_config.sd_available ? "AVAILABLE" : "NOT AVAILABLE");
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "Save Counter: %lu, Sensor Logs: %u, Alarm Logs: %u\r\n",
             g_flash_config.save_counter,
             g_flash_config.sensor_log_count,
             g_flash_config.alarm_log_count);
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "User Config: Supply %.1f°C, Return %.1f°C\r\n",
             g_flash_config.user_config.supply_setpoint,
             g_flash_config.user_config.return_setpoint);
    Send_Debug_Data(msg);
    
    Send_Debug_Data("=====================================\r\n\r\n");
}

/* === PRIVATE FUNCTIONS === */

/**
 * @brief Calculate CRC32 for Data Integrity
 */
static uint32_t FlashConfig_CRC32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ CONFIG_CRC32_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return ~crc;
}

/**
 * @brief Set Default Configuration Values
 */
static void FlashConfig_SetDefaults(void)
{
    // Initialize user configuration with defaults
    g_flash_config.user_config.version = FLASH_CONFIG_VERSION;
    g_flash_config.user_config.supply_setpoint = DEFAULT_SUPPLY_SETPOINT;
    g_flash_config.user_config.return_setpoint = DEFAULT_RETURN_SETPOINT;
    g_flash_config.user_config.ambient_threshold = DEFAULT_HIGH_AMBIENT_LIMIT;
    g_flash_config.user_config.default_mode = CAPACITY_MODE_NORMAL;
    g_flash_config.user_config.auto_mode_enabled = 1;
    g_flash_config.user_config.mode_switch_delay = 300; // 5 minutes
    g_flash_config.user_config.config_save_interval = DEFAULT_USER_SAVE_INTERVAL;
    g_flash_config.user_config.sensor_log_interval = DEFAULT_SENSOR_LOG_INTERVAL;
    g_flash_config.user_config.runtime_save_interval = DEFAULT_RUNTIME_SAVE_INTERVAL;
    g_flash_config.user_config.debug_enabled = 1;
    g_flash_config.user_config.sd_logging_enabled = 1;
    g_flash_config.user_config.network_enabled = 1;
    g_flash_config.user_config.hmi_enabled = 1;
    g_flash_config.user_config.max_supply_temp = 15.0f;
    g_flash_config.user_config.min_supply_temp = 4.0f;
    g_flash_config.user_config.safety_check_interval = 1000; // 1 second
    
    // Initialize runtime data
    g_flash_config.runtime_data.version = FLASH_CONFIG_VERSION;
    memset(&g_flash_config.runtime_data.compressor_hours, 0, sizeof(g_flash_config.runtime_data.compressor_hours));
    memset(&g_flash_config.runtime_data.condenser_hours, 0, sizeof(g_flash_config.runtime_data.condenser_hours));
    memset(&g_flash_config.runtime_data.compressor_starts, 0, sizeof(g_flash_config.runtime_data.compressor_starts));
    memset(&g_flash_config.runtime_data.condenser_starts, 0, sizeof(g_flash_config.runtime_data.condenser_starts));
    g_flash_config.runtime_data.system_total_hours = 0;
    g_flash_config.runtime_data.system_start_count = 1;
    g_flash_config.runtime_data.total_cooling_hours = 0.0f;
    memset(&g_flash_config.runtime_data.mode_hours, 0, sizeof(g_flash_config.runtime_data.mode_hours));
    g_flash_config.runtime_data.fault_count = 0;
    g_flash_config.runtime_data.alarm_count = 0;
    
    // Initialize counters
    g_flash_config.sensor_log_count = 0;
    g_flash_config.sensor_log_index = 0;
    g_flash_config.alarm_log_count = 0;
    g_flash_config.alarm_log_index = 0;
}

/* === SAVE FUNCTIONS FOR LOGS === */

/**
 * @brief Save Sensor Logs to Flash
 */
FlashConfig_Status_t FlashConfig_SaveSensorLogs(void)
{
    if (Flash_WritePage(FLASH_SENSOR_LOG_ADDR, (uint8_t*)&g_flash_config.sensor_logs,
                       sizeof(g_flash_config.sensor_logs)) != 0) {
        return FLASH_CONFIG_ERROR;
    }
    
    return FLASH_CONFIG_OK;
}

/**
 * @brief Load Sensor Logs from Flash
 */
FlashConfig_Status_t FlashConfig_LoadSensorLogs(void)
{
    if (Flash_ReadData(FLASH_SENSOR_LOG_ADDR, (uint8_t*)&g_flash_config.sensor_logs,
                      sizeof(g_flash_config.sensor_logs)) != 0) {
        return FLASH_CONFIG_ERROR;
    }
    
    return FLASH_CONFIG_OK;
}

/**
 * @brief Save Alarm Logs to Flash
 */
FlashConfig_Status_t FlashConfig_SaveAlarmLogs(void)
{
    if (Flash_WritePage(FLASH_ALARM_LOG_ADDR, (uint8_t*)&g_flash_config.alarm_logs,
                       sizeof(g_flash_config.alarm_logs)) != 0) {
        return FLASH_CONFIG_ERROR;
    }
    
    return FLASH_CONFIG_OK;
}

/**
 * @brief Load Alarm Logs from Flash
 */
FlashConfig_Status_t FlashConfig_LoadAlarmLogs(void)
{
    if (Flash_ReadData(FLASH_ALARM_LOG_ADDR, (uint8_t*)&g_flash_config.alarm_logs,
                      sizeof(g_flash_config.alarm_logs)) != 0) {
        return FLASH_CONFIG_ERROR;
    }
    
    return FLASH_CONFIG_OK;
}

/**
 * @brief Read configuration data from flash
 * @param config_name: Configuration name/key
 * @param data: Pointer to data buffer
 * @param size: Size of data
 * @return 1 if successful, 0 if failed
 */
uint8_t FlashConfig_ReadConfigData(const char* config_name, void* data, uint16_t size)
{
    // Placeholder implementation
    // For now, just return 0 (not found)
    return 0;
}

/**
 * @brief Log event to flash
 * @param system: System name
 * @param event: Event description
 * @param level: Event level
 */
void FlashConfig_LogEvent(const char* system, const char* event, uint8_t level)
{
    // Use existing alarm logging system
    FlashConfig_LogAlarm(0x3000 + level, level, 0, 0.0f, event);
}

SD_Card_Status_t SD_Card_Get_Status(void)
{
    // Placeholder implementation for SD card status
    // Return SD_CARD_OK if SD card is available, SD_CARD_ERROR if not
    return SD_CARD_ERROR; // Assume no SD card for now
}
