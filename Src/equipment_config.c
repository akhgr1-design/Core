/**
  ******************************************************************************
  * @file    equipment_config.c
  * @brief   Equipment Configuration Management Implementation
  * @author  Industrial Chiller Control System
  * @version 1.0
  * @date    2025
  ******************************************************************************
  */

#include "equipment_config.h"
#include <stdio.h>

/* Private Constants ---------------------------------------------------------*/
#define CONFIG_MAGIC_NUMBER             0xCH1LL3R   // Magic number for config validation
#define CONFIG_CRC32_POLYNOMIAL         0xEDB88320  // Standard CRC32 polynomial
#define CONFIG_DEFAULT_UPDATE_COUNT     10          // Flash update after 10 changes

/* Private Variables ---------------------------------------------------------*/
static uint32_t config_last_save_time = 0;
static uint8_t config_dirty_flag = 0;
static uint32_t config_change_counter = 0;

/* Global Configuration Structure --------------------------------------------*/
EquipmentConfig_t g_equipment_config = {0};
uint8_t g_config_initialized = 0;

/* Private Function Prototypes -----------------------------------------------*/
static uint32_t Config_CalculateCRC32(const uint8_t *data, uint32_t length);
static void Config_InitializeDefaults(void);
static void Config_InitializeModeConfigs(void);
static EquipmentStatus_t Config_ValidateStructure(void);

/* Public Functions ----------------------------------------------------------*/

/**
 * @brief Initialize Equipment Configuration System
 */
EquipmentStatus_t EquipmentConfig_Init(void)
{
    Send_Debug_Data("=== Equipment Configuration Initialization ===\r\n");
    
    // Clear configuration structure
    memset(&g_equipment_config, 0, sizeof(EquipmentConfig_t));
    
    // Try to load configuration from flash
    EquipmentStatus_t status = EquipmentConfig_LoadFromFlash();
    
    if (status != EQUIPMENT_STATUS_OK) {
        Send_Debug_Data("Flash load failed - Loading 38°C optimized defaults\r\n");
        status = EquipmentConfig_LoadDefaults();
        
        if (status == EQUIPMENT_STATUS_OK) {
            // Save defaults to flash for next boot
            EquipmentConfig_SaveToFlash();
        }
    } else {
        Send_Debug_Data("Configuration loaded successfully from flash\r\n");
    }
    
    // Validate loaded configuration
    status = EquipmentConfig_ValidateConfig();
    if (status != EQUIPMENT_STATUS_OK) {
        Send_Debug_Data("Configuration validation failed - Loading defaults\r\n");
        EquipmentConfig_LoadDefaults();
        EquipmentConfig_SaveToFlash();
    }
    
    g_config_initialized = 1;
    config_last_save_time = HAL_GetTick();
    
    // Display current configuration
    EquipmentConfig_DisplayStatus();
    
    Send_Debug_Data("Equipment Configuration: INITIALIZED\r\n");
    return EQUIPMENT_STATUS_OK;
}

/**
 * @brief Load Default Configuration (38°C Hot Climate Optimized)
 */
EquipmentStatus_t EquipmentConfig_LoadDefaults(void)
{
    Send_Debug_Data("Loading 38°C Hot Climate Optimized Defaults...\r\n");
    
    // Initialize basic configuration
    Config_InitializeDefaults();
    
    // Initialize mode-specific configurations
    Config_InitializeModeConfigs();
    
    // Calculate and store CRC
    g_equipment_config.crc32 = EquipmentConfig_CalculateCRC32();
    
    config_dirty_flag = 1; // Mark as dirty for saving
    
    Send_Debug_Data("38°C Hot Climate Defaults: LOADED\r\n");
    return EQUIPMENT_STATUS_OK;
}

/**
 * @brief Initialize Default Configuration Values
 */
static void Config_InitializeDefaults(void)
{
    // Configuration header
    g_equipment_config.version = EQUIPMENT_CONFIG_VERSION;
    g_equipment_config.timestamp = HAL_GetTick();
    
    // System configuration - Default to 4 compressors, 2 condenser banks for testing
    g_equipment_config.current_mode = CAPACITY_MODE_NORMAL;
    g_equipment_config.total_compressors_installed = 4;
    g_equipment_config.total_condenser_banks = 2;
    
    // 38°C Hot Climate Temperature Configuration
    g_equipment_config.supply_water_setpoint = 7.0f;     // 7°C
    g_equipment_config.return_water_setpoint = 12.0f;    // 12°C  
    g_equipment_config.ambient_temp_baseline = 38.0f;    // 38°C
    g_equipment_config.high_ambient_alarm_limit = 40.0f; // 40°C
    
    // Debug: Verify temperature values are set correctly
    char debug_msg[100];
    snprintf(debug_msg, sizeof(debug_msg), "DEBUG: Supply=%d.%d°C, Return=%d.%d°C, Ambient=%d.%d°C\r\n",
             (int)g_equipment_config.supply_water_setpoint,
             (int)((g_equipment_config.supply_water_setpoint - (int)g_equipment_config.supply_water_setpoint) * 10),
             (int)g_equipment_config.return_water_setpoint,
             (int)((g_equipment_config.return_water_setpoint - (int)g_equipment_config.return_water_setpoint) * 10),
             (int)g_equipment_config.ambient_temp_baseline,
             (int)((g_equipment_config.ambient_temp_baseline - (int)g_equipment_config.ambient_temp_baseline) * 10));
    Send_Debug_Data(debug_msg);
    
    Send_Debug_Data("DEBUG: Temperature values should be: Supply=7.0°C, Return=12.0°C, Ambient=38.0°C\r\n");
    
    // Equipment Configuration - Initialize first 4 compressors and 2 condenser banks
    for (int i = 0; i < MAX_COMPRESSORS; i++) {
        g_equipment_config.compressors[i].installed = (i < 4) ? 1 : 0;
        g_equipment_config.compressors[i].enabled = (i < 4) ? 1 : 0;
        g_equipment_config.compressors[i].runtime_hours = 0;
        g_equipment_config.compressors[i].start_cycles = 0;
        g_equipment_config.compressors[i].maintenance_due = 0;
        g_equipment_config.compressors[i].last_maintenance = 0;
    }
    
    for (int i = 0; i < MAX_CONDENSER_BANKS; i++) {
        g_equipment_config.condenser_banks[i].installed = (i < 2) ? 1 : 0;
        g_equipment_config.condenser_banks[i].enabled = (i < 2) ? 1 : 0;
        g_equipment_config.condenser_banks[i].runtime_hours = 0;
        g_equipment_config.condenser_banks[i].start_cycles = 0;
        g_equipment_config.condenser_banks[i].maintenance_due = 0;
        g_equipment_config.condenser_banks[i].last_maintenance = 0;
    }
    
    // Sensor Configuration - Enable basic sensors, graceful degradation for others
    g_equipment_config.sensor_config.supply_sensor_enabled = 1;        // Essential
    g_equipment_config.sensor_config.return_sensor_enabled = 1;        // Essential  
    g_equipment_config.sensor_config.ambient_sensor_enabled = 1;       // Important
    g_equipment_config.sensor_config.compressor_sensors_enabled = 0;   // Optional
    g_equipment_config.sensor_config.condenser_sensors_enabled = 0;    // Optional
    g_equipment_config.sensor_config.oil_temp_sensors_enabled = 0;     // Optional
    g_equipment_config.sensor_config.sensor_fault_tolerance = 1;       // Continue with failed sensors
    
    // Clear calibration offsets
    for (int i = 0; i < MAX_TEMPERATURE_SENSORS; i++) {
        g_equipment_config.sensor_config.sensor_calibration_offset[i] = 0.0f;
    }
    
    // System Settings
    g_equipment_config.config_update_interval = EQUIPMENT_CONFIG_UPDATE_INTERVAL; // 60 seconds
    g_equipment_config.auto_mode_switching = 0;      // Disabled by default
    g_equipment_config.maintenance_mode = 0;         // Normal operation
    
    // Initialize statistics
    g_equipment_config.total_system_runtime = 0;
    g_equipment_config.configuration_changes = 0;
    g_equipment_config.system_start_count++;
}

/**
 * @brief Initialize Mode-Specific Configurations
 */
static void Config_InitializeModeConfigs(void)
{
    // ECONOMIC Mode (25% Capacity) - Energy focused for hot climate
    g_equipment_config.mode_configs[CAPACITY_MODE_ECONOMIC].max_compressors = 2;
    g_equipment_config.mode_configs[CAPACITY_MODE_ECONOMIC].max_condenser_banks = 2;
    g_equipment_config.mode_configs[CAPACITY_MODE_ECONOMIC].temp_tolerance = DEFAULT_TEMP_TOLERANCE_ECO; // 1.5°C
    g_equipment_config.mode_configs[CAPACITY_MODE_ECONOMIC].staging_delay_ms = 120000; // 2 minutes
    g_equipment_config.mode_configs[CAPACITY_MODE_ECONOMIC].energy_optimization = 1;
    g_equipment_config.mode_configs[CAPACITY_MODE_ECONOMIC].ambient_compensation_factor = 1.2f;
    
    // NORMAL Mode (50% Capacity) - Balanced approach
    g_equipment_config.mode_configs[CAPACITY_MODE_NORMAL].max_compressors = 4;
    g_equipment_config.mode_configs[CAPACITY_MODE_NORMAL].max_condenser_banks = 3;
    g_equipment_config.mode_configs[CAPACITY_MODE_NORMAL].temp_tolerance = DEFAULT_TEMP_TOLERANCE_NORMAL; // 1.0°C
    g_equipment_config.mode_configs[CAPACITY_MODE_NORMAL].staging_delay_ms = 60000; // 1 minute
    g_equipment_config.mode_configs[CAPACITY_MODE_NORMAL].energy_optimization = 1;
    g_equipment_config.mode_configs[CAPACITY_MODE_NORMAL].ambient_compensation_factor = 1.0f;
    
    // FULL Mode (75% Capacity) - Performance focused
    g_equipment_config.mode_configs[CAPACITY_MODE_FULL].max_compressors = 6;
    g_equipment_config.mode_configs[CAPACITY_MODE_FULL].max_condenser_banks = 4;
    g_equipment_config.mode_configs[CAPACITY_MODE_FULL].temp_tolerance = DEFAULT_TEMP_TOLERANCE_FULL; // 0.5°C
    g_equipment_config.mode_configs[CAPACITY_MODE_FULL].staging_delay_ms = 30000; // 30 seconds
    g_equipment_config.mode_configs[CAPACITY_MODE_FULL].energy_optimization = 0;
    g_equipment_config.mode_configs[CAPACITY_MODE_FULL].ambient_compensation_factor = 0.8f;
    
    // CUSTOM Mode (100% Capacity) - User configurable
    g_equipment_config.mode_configs[CAPACITY_MODE_CUSTOM].max_compressors = 8;
    g_equipment_config.mode_configs[CAPACITY_MODE_CUSTOM].max_condenser_banks = 4;
    g_equipment_config.mode_configs[CAPACITY_MODE_CUSTOM].temp_tolerance = 1.0f;
    g_equipment_config.mode_configs[CAPACITY_MODE_CUSTOM].staging_delay_ms = 45000; // 45 seconds
    g_equipment_config.mode_configs[CAPACITY_MODE_CUSTOM].energy_optimization = 0;
    g_equipment_config.mode_configs[CAPACITY_MODE_CUSTOM].ambient_compensation_factor = 1.0f;
}

/**
 * @brief Set Capacity Control Mode
 */
EquipmentStatus_t EquipmentConfig_SetCapacityMode(CapacityMode_t mode)
{
    if (mode > CAPACITY_MODE_CUSTOM) {
        return EQUIPMENT_STATUS_INVALID_CONFIG;
    }
    
    if (g_equipment_config.current_mode != mode) {
        g_equipment_config.current_mode = mode;
        config_dirty_flag = 1;
        g_equipment_config.configuration_changes++;
        
        // Debug output
        const char* mode_names[] = {"ECONOMIC", "NORMAL", "FULL", "CUSTOM"};
        char msg[100];
        snprintf(msg, sizeof(msg), "Capacity Mode changed to: %s\r\n", mode_names[mode]);
        Send_Debug_Data(msg);
        
        // Display new limits
        snprintf(msg, sizeof(msg), "Max Compressors: %d, Max Condenser Banks: %d\r\n",
                g_equipment_config.mode_configs[mode].max_compressors,
                g_equipment_config.mode_configs[mode].max_condenser_banks);
        Send_Debug_Data(msg);
    }
    
    return EQUIPMENT_STATUS_OK;
}

/**
 * @brief Get Current Capacity Control Mode
 */
CapacityMode_t EquipmentConfig_GetCapacityMode(void)
{
    return g_equipment_config.current_mode;
}

/**
 * @brief Get Maximum Compressors for Current Mode
 */
uint8_t EquipmentConfig_GetMaxCompressors(void)
{
    CapacityMode_t mode = g_equipment_config.current_mode;
    uint8_t max_available = g_equipment_config.total_compressors_installed;
    uint8_t max_for_mode = g_equipment_config.mode_configs[mode].max_compressors;
    
    return (max_for_mode < max_available) ? max_for_mode : max_available;
}

/**
 * @brief Get Maximum Condenser Banks for Current Mode
 */
uint8_t EquipmentConfig_GetMaxCondenserBanks(void)
{
    CapacityMode_t mode = g_equipment_config.current_mode;
    uint8_t max_available = g_equipment_config.total_condenser_banks;
    uint8_t max_for_mode = g_equipment_config.mode_configs[mode].max_condenser_banks;
    
    return (max_for_mode < max_available) ? max_for_mode : max_available;
}

/**
 * @brief Get Temperature Setpoints for Current Mode  
 */
void EquipmentConfig_GetTemperatureSetpoints(float *supply_setpoint, 
                                           float *return_setpoint,
                                           float *tolerance)
{
    if (supply_setpoint) {
        *supply_setpoint = g_equipment_config.supply_water_setpoint;
    }
    
    if (return_setpoint) {
        *return_setpoint = g_equipment_config.return_water_setpoint;
    }
    
    if (tolerance) {
        CapacityMode_t mode = g_equipment_config.current_mode;
        *tolerance = g_equipment_config.mode_configs[mode].temp_tolerance;
    }
}

/**
 * @brief Load Configuration from Flash Memory
 */
EquipmentStatus_t EquipmentConfig_LoadFromFlash(void)
{
    uint8_t flash_data[sizeof(EquipmentConfig_t)];
    
    // Read configuration from flash
    if (Flash_ReadData(EQUIPMENT_CONFIG_FLASH_ADDR, flash_data, sizeof(EquipmentConfig_t)) != 0) {
        Send_Debug_Data("Flash read error\r\n");
        return EQUIPMENT_STATUS_FLASH_ERROR;
    }
    
    // Copy data to configuration structure
    memcpy(&g_equipment_config, flash_data, sizeof(EquipmentConfig_t));
    
    // Validate version and CRC
    if (g_equipment_config.version != EQUIPMENT_CONFIG_VERSION) {
        Send_Debug_Data("Configuration version mismatch\r\n");
        return EQUIPMENT_STATUS_INVALID_CONFIG;
    }
    
    // Verify CRC32
    uint32_t calculated_crc = EquipmentConfig_CalculateCRC32();
    if (calculated_crc != g_equipment_config.crc32) {
        Send_Debug_Data("Configuration CRC mismatch\r\n");
        return EQUIPMENT_STATUS_INVALID_CONFIG;
    }
    
    return EQUIPMENT_STATUS_OK;
}

/**
 * @brief Save Configuration to Flash Memory
 */
EquipmentStatus_t EquipmentConfig_SaveToFlash(void)
{
    // Update timestamp and CRC before saving
    g_equipment_config.timestamp = HAL_GetTick();
    g_equipment_config.crc32 = EquipmentConfig_CalculateCRC32();
    
    // Write configuration to flash  
    if (Flash_WritePage(EQUIPMENT_CONFIG_FLASH_ADDR, (uint8_t*)&g_equipment_config, 
                       sizeof(EquipmentConfig_t)) != 0) {
        Send_Debug_Data("Flash write error\r\n");
        return EQUIPMENT_STATUS_FLASH_ERROR;
    }
    
    config_dirty_flag = 0;
    config_last_save_time = HAL_GetTick();
    
    Send_Debug_Data("Configuration saved to flash\r\n");
    return EQUIPMENT_STATUS_OK;
}

/**
 * @brief Process Periodic Configuration Tasks
 */
void EquipmentConfig_ProcessPeriodicTasks(void)
{
    // Check if configuration needs to be saved (only for actual config changes)
    if (config_dirty_flag && 
        (HAL_GetTick() - config_last_save_time) >= g_equipment_config.config_update_interval) {
        EquipmentConfig_SaveToFlash();
    }
    
    // Update system runtime (called every 100ms, so increment every 36000 calls = 1 hour)
    static uint32_t runtime_counter = 0;
    runtime_counter++;
    if (runtime_counter >= 36000) { // 100ms * 36000 = 1 hour
        runtime_counter = 0;
        g_equipment_config.total_system_runtime++;
        // Runtime updates don't trigger configuration saves
    }
}

/**
 * @brief Display Current Configuration Status
 */
void EquipmentConfig_DisplayStatus(void)
{
    char msg[150];
    
    Send_Debug_Data("\r\n=== EQUIPMENT CONFIGURATION STATUS ===\r\n");
    
    // System Information
    snprintf(msg, sizeof(msg), "Version: 0x%04X, System Runtime: %lu hours\r\n", 
             g_equipment_config.version, g_equipment_config.total_system_runtime);
    Send_Debug_Data(msg);
    
    // Current Mode
    const char* mode_names[] = {"ECONOMIC", "NORMAL", "FULL", "CUSTOM"};
    snprintf(msg, sizeof(msg), "Current Mode: %s\r\n", mode_names[g_equipment_config.current_mode]);
    Send_Debug_Data(msg);
    
    // Equipment Counts
    snprintf(msg, sizeof(msg), "Equipment: %d Compressors, %d Condenser Banks\r\n",
             g_equipment_config.total_compressors_installed,
             g_equipment_config.total_condenser_banks);
    Send_Debug_Data(msg);
    
    // Current Mode Limits
    CapacityMode_t mode = g_equipment_config.current_mode;
    snprintf(msg, sizeof(msg), "Mode Limits: %d Compressors, %d Condenser Banks\r\n",
             g_equipment_config.mode_configs[mode].max_compressors,
             g_equipment_config.mode_configs[mode].max_condenser_banks);
    Send_Debug_Data(msg);
    
    // Debug: Verify values before display
    char debug_msg[100];
    snprintf(debug_msg, sizeof(debug_msg), "DEBUG: Before display - Supply=%d.%d°C, Return=%d.%d°C\r\n",
             (int)g_equipment_config.supply_water_setpoint,
             (int)((g_equipment_config.supply_water_setpoint - (int)g_equipment_config.supply_water_setpoint) * 10),
             (int)g_equipment_config.return_water_setpoint,
             (int)((g_equipment_config.return_water_setpoint - (int)g_equipment_config.return_water_setpoint) * 10));
    Send_Debug_Data(debug_msg);
    
    // Temperature Settings (38°C Hot Climate)
    snprintf(msg, sizeof(msg), "Temp Setpoints: Supply=%d.%d°C, Return=%d.%d°C, Tolerance=±%d.%d°C\r\n",
             (int)g_equipment_config.supply_water_setpoint,
             (int)((g_equipment_config.supply_water_setpoint - (int)g_equipment_config.supply_water_setpoint) * 10),
             (int)g_equipment_config.return_water_setpoint,
             (int)((g_equipment_config.return_water_setpoint - (int)g_equipment_config.return_water_setpoint) * 10),
             (int)g_equipment_config.mode_configs[mode].temp_tolerance,
             (int)((g_equipment_config.mode_configs[mode].temp_tolerance - (int)g_equipment_config.mode_configs[mode].temp_tolerance) * 10));
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "Ambient Baseline: %d.%d°C, High Alarm: %d.%d°C\r\n",
             (int)g_equipment_config.ambient_temp_baseline,
             (int)((g_equipment_config.ambient_temp_baseline - (int)g_equipment_config.ambient_temp_baseline) * 10),
             (int)g_equipment_config.high_ambient_alarm_limit,
             (int)((g_equipment_config.high_ambient_alarm_limit - (int)g_equipment_config.high_ambient_alarm_limit) * 10));
    Send_Debug_Data(msg);
    
    // Sensor Configuration
    Send_Debug_Data("Sensors: ");
    Send_Debug_Data(g_equipment_config.sensor_config.supply_sensor_enabled ? "Supply+" : "Supply-");
    Send_Debug_Data(g_equipment_config.sensor_config.return_sensor_enabled ? " Return+" : " Return-");
    Send_Debug_Data(g_equipment_config.sensor_config.ambient_sensor_enabled ? " Ambient+" : " Ambient-");
    Send_Debug_Data("\r\n");
    
    // Statistics
    snprintf(msg, sizeof(msg), "Stats: %lu config changes, %lu system starts\r\n",
             g_equipment_config.configuration_changes,
             g_equipment_config.system_start_count);
    Send_Debug_Data(msg);
    
    Send_Debug_Data("=====================================\r\n\r\n");
}

/**
 * @brief Validate Configuration Integrity
 */
EquipmentStatus_t EquipmentConfig_ValidateConfig(void)
{
    // Check version
    if (g_equipment_config.version != EQUIPMENT_CONFIG_VERSION) {
        return EQUIPMENT_STATUS_INVALID_CONFIG;
    }
    
    // Check capacity mode
    if (g_equipment_config.current_mode > CAPACITY_MODE_CUSTOM) {
        return EQUIPMENT_STATUS_INVALID_CONFIG;
    }
    
    // Check equipment counts
    if (g_equipment_config.total_compressors_installed > MAX_COMPRESSORS ||
        g_equipment_config.total_condenser_banks > MAX_CONDENSER_BANKS) {
        return EQUIPMENT_STATUS_INVALID_CONFIG;
    }
    
    // Check temperature ranges (reasonable for chiller operation)
    if (g_equipment_config.supply_water_setpoint < 4.0f || 
        g_equipment_config.supply_water_setpoint > 15.0f ||
        g_equipment_config.return_water_setpoint < 8.0f ||
        g_equipment_config.return_water_setpoint > 25.0f) {
        return EQUIPMENT_STATUS_INVALID_CONFIG;
    }
    
    return EQUIPMENT_STATUS_OK;
}

/**
 * @brief Calculate Configuration CRC32
 */
uint32_t EquipmentConfig_CalculateCRC32(void)
{
    // Temporarily store current CRC and set to 0 for calculation
    uint32_t stored_crc = g_equipment_config.crc32;
    g_equipment_config.crc32 = 0;
    
    // Calculate CRC32 of entire structure except CRC field
    uint32_t crc = Config_CalculateCRC32((uint8_t*)&g_equipment_config, sizeof(EquipmentConfig_t));
    
    // Restore CRC field
    g_equipment_config.crc32 = stored_crc;
    
    return crc;
}

/**
 * @brief Calculate CRC32 for data block
 */
static uint32_t Config_CalculateCRC32(const uint8_t *data, uint32_t length)
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
 * @brief Set Equipment Installation Status
 */
EquipmentStatus_t EquipmentConfig_SetEquipmentInstalled(uint8_t equipment_type, 
                                                       uint8_t equipment_index, 
                                                       uint8_t installed)
{
    if (equipment_type == 0 && equipment_index < MAX_COMPRESSORS) {
        // Compressor
        g_equipment_config.compressors[equipment_index].installed = installed ? 1 : 0;
        g_equipment_config.compressors[equipment_index].enabled = installed ? 1 : 0;
        config_dirty_flag = 1;
        return EQUIPMENT_STATUS_OK;
    } else if (equipment_type == 1 && equipment_index < MAX_CONDENSER_BANKS) {
        // Condenser Bank
        g_equipment_config.condenser_banks[equipment_index].installed = installed ? 1 : 0;
        g_equipment_config.condenser_banks[equipment_index].enabled = installed ? 1 : 0;
        config_dirty_flag = 1;
        return EQUIPMENT_STATUS_OK;
    }
    
    return EQUIPMENT_STATUS_INVALID_CONFIG;
}

/**
 * @brief Factory Reset Configuration
 */
EquipmentStatus_t EquipmentConfig_FactoryReset(void)
{
    Send_Debug_Data("Performing factory reset to 38°C optimized defaults...\r\n");
    
    // Load defaults
    EquipmentStatus_t status = EquipmentConfig_LoadDefaults();
    
    if (status == EQUIPMENT_STATUS_OK) {
        // Save to flash immediately
        status = EquipmentConfig_SaveToFlash();
        
        if (status == EQUIPMENT_STATUS_OK) {
            Send_Debug_Data("Factory reset completed successfully\r\n");
        }
    }
    
    return status;
}

/**
 * @brief Save Runtime Data Only (without configuration)
 */
void EquipmentConfig_SaveRuntimeDataOnly(void)
{
    // Update timestamp and CRC before saving
    g_equipment_config.timestamp = HAL_GetTick();
    g_equipment_config.crc32 = EquipmentConfig_CalculateCRC32();
    
    // Write configuration to flash (silent save)
    if (Flash_WritePage(EQUIPMENT_CONFIG_FLASH_ADDR, (uint8_t*)&g_equipment_config, 
                       sizeof(EquipmentConfig_t)) == 0) {
        config_last_save_time = HAL_GetTick();
        // No debug message for runtime-only saves
    }
}

/**
 * @brief Check if compressor is installed
 * @param compressor_id: Compressor ID to check
 * @return 1 if installed, 0 if not installed
 */
uint8_t EquipmentConfig_IsCompressorInstalled(uint8_t compressor_id)
{
    if (compressor_id >= MAX_COMPRESSORS) {
        return 0;
    }
    
    return g_equipment_config.compressors[compressor_id].installed;
}
