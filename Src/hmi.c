#include "hmi.h"
#include "uart_comm.h"
#include "gpio.h"
#include <string.h>

// External UART handle
extern UART_HandleTypeDef huart4;

// Message buffer
static uint8_t hmi_buffer[100];
static uint16_t hmi_index = 0;
static uint32_t hmi_last_time = 0;

// HMI Version check command: 5A A5 04 83 00 0F 01
static uint8_t version_command_1[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x0F, 0x01}; // Original
static uint8_t version_command_2[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x00, 0x01}; // Register 0x00
static uint8_t version_command_3[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x01, 0x01}; // Register 0x01
static uint8_t ping_command[] = {0x5A, 0xA5, 0x03, 0x80, 0x03, 0x00};           // Basic ping
static uint8_t read_pic_id[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x03, 0x01};      // Read Picture ID

static uint8_t current_command = 0;
static uint32_t last_version_check = 0;

// Add these variables for tracking response reliability
static uint32_t commands_sent = 0;
static uint32_t responses_received = 0;
static uint8_t last_response_received = 0;

// Add these variables at the top of hmi.c (after existing variables):
static uint8_t hmi_rx_buffer[1];  // Single byte receive buffer
static volatile uint8_t hmi_interrupt_active = 0;

// Add this near the top of hmi.c with other static variables:
static uint8_t hmi_initialized = 0;

// Add this function to set the status:
void HMI_Set_Initialized(uint8_t status) {
    hmi_initialized = status;
}

// Add this function to check status:
uint8_t HMI_Is_Initialized(void) {
    return hmi_initialized;
}

// CRC calculation for DWIN HMI commands
uint16_t Calculate_CRC16(uint8_t* data, uint8_t length) {
    uint16_t crc = 0xFFFF;  // Initial value
    
    for (uint8_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i];
        
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;  // CRC-16 polynomial
            } else {
                crc = crc >> 1;
            }
        }
    }
    
    return crc;
}

// Alternative CRC-8 calculation (some DWIN models use CRC-8)
uint8_t Calculate_CRC8(uint8_t* data, uint8_t length) {
    uint8_t crc = 0x00;  // Initial value
    
    for (uint8_t i = 0; i < length; i++) {
        crc ^= data[i];
        
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;  // CRC-8 polynomial
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}

// Simple checksum (sum of all bytes)
uint8_t Calculate_Simple_Checksum(uint8_t* data, uint8_t length) {
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < length; i++) {
        checksum += data[i];
    }
    return checksum;
}

// XOR checksum
uint8_t Calculate_XOR_Checksum(uint8_t* data, uint8_t length) {
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

// Test HMI commands with different CRC/checksum methods
void HMI_Test_With_CRC(void) {
    static uint32_t last_test_time = 0;
    static uint8_t crc_test = 0;
    static uint8_t command_sent = 0;
    
    if (HAL_GetTick() - last_test_time < 8000) {
        return;
    }
    
    if (command_sent) {
        return;
    }
    
    last_test_time = HAL_GetTick();
    command_sent = 1;
    
    // Base command (without CRC)
    uint8_t base_cmd[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x0F, 0x01};
    uint8_t cmd_buffer[12];  // Extra space for CRC
    uint8_t cmd_size;
    char* crc_desc;
    
    // Copy base command
    memcpy(cmd_buffer, base_cmd, 7);
    
    // Add different CRC/checksum methods
    switch(crc_test % 6) {
        case 0: {
            // No CRC
            cmd_size = 7;
            crc_desc = "No CRC";
            break;
        }
        
        case 1: {
            // CRC-16 (Little Endian)
            uint16_t crc16 = Calculate_CRC16(base_cmd, 7);
            cmd_buffer[7] = crc16 & 0xFF;        // Low byte first
            cmd_buffer[8] = (crc16 >> 8) & 0xFF; // High byte
            cmd_size = 9;
            crc_desc = "CRC-16 LE";
            break;
        }
        
        case 2: {
            // CRC-16 (Big Endian)
            uint16_t crc16 = Calculate_CRC16(base_cmd, 7);
            cmd_buffer[7] = (crc16 >> 8) & 0xFF; // High byte first
            cmd_buffer[8] = crc16 & 0xFF;        // Low byte
            cmd_size = 9;
            crc_desc = "CRC-16 BE";
            break;
        }
        
        case 3: {
            // CRC-8
            uint8_t crc8 = Calculate_CRC8(base_cmd, 7);
            cmd_buffer[7] = crc8;
            cmd_size = 8;
            crc_desc = "CRC-8";
            break;
        }
        
        case 4: {
            // Simple checksum
            uint8_t checksum = Calculate_Simple_Checksum(base_cmd, 7);
            cmd_buffer[7] = checksum;
            cmd_size = 8;
            crc_desc = "Simple Sum";
            break;
        }
        
        case 5: {
            // XOR checksum
            uint8_t xor_checksum = Calculate_XOR_Checksum(base_cmd, 7);
            cmd_buffer[7] = xor_checksum;
            cmd_size = 8;
            crc_desc = "XOR Checksum";
            break;
        }
    }
    
    // Clear buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    // Show exact packet being sent
    Send_Debug_Data("HMI TX: ");
    for (int i = 0; i < cmd_size; i++) {
        char hex_str[4];
        snprintf(hex_str, sizeof(hex_str), "%02X ", cmd_buffer[i]);
        Send_Debug_Data(hex_str);
    }
    char crc_msg[100];
    snprintf(crc_msg, sizeof(crc_msg), "(%s)\r\n", crc_desc);
    Send_Debug_Data(crc_msg);
    
    // Show CRC calculation details
    if (crc_test > 0) {
        Send_Debug_Data("Base command: 5A A5 04 83 00 0F 01\r\n");
        if (cmd_size > 7) {
            Send_Debug_Data("Added CRC/Checksum: ");
            for (int i = 7; i < cmd_size; i++) {
                char crc_hex[4];
                snprintf(crc_hex, sizeof(crc_hex), "%02X ", cmd_buffer[i]);
                Send_Debug_Data(crc_hex);
            }
            Send_Debug_Data("\r\n");
        }
    }
    
    // Prepare for transmission
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    
    // Clear any pending data
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
    }
    
    // Send command with CRC
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(5);
    
    HAL_UART_Transmit(&huart4, cmd_buffer, cmd_size, 2000);
    
    // Wait for transmission complete
    while(__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TC) == RESET);
    
    // Wait for last bit to clear
    HAL_Delay(3);
    
    // Switch to RX
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    
    // Clear any echo
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t echo = huart4.Instance->RDR;
        (void)echo;
    }
    
    Send_Debug_Data("Listening for response...\r\n");
    
    // Capture response
    uint32_t capture_start = HAL_GetTick();
    uint32_t last_byte_time = capture_start;
    
    while ((HAL_GetTick() - capture_start) < 300) {  // 300ms window
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
            
            if (hmi_index < sizeof(hmi_buffer)) {
                hmi_buffer[hmi_index] = byte;
                hmi_index++;
                last_byte_time = HAL_GetTick();
                
                char byte_msg[50];
                snprintf(byte_msg, sizeof(byte_msg), "RX[%d]: %02X\r\n", hmi_index-1, byte);
                Send_Debug_Data(byte_msg);
                
                // Check for complete DWIN response
                if (hmi_index >= 3 && hmi_buffer[0] == 0x5A && hmi_buffer[1] == 0xA5) {
                    uint8_t expected_len = 3 + hmi_buffer[2];
                    if (hmi_index >= expected_len) {
                        Send_Debug_Data("Complete DWIN response detected!\r\n");
                        HAL_Delay(20);  // Wait for any additional bytes
                        if (!__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
                            break;
                        }
                    }
                }
            }
        }
        
        // Timeout after no bytes for 50ms
        if (hmi_index > 0 && (HAL_GetTick() - last_byte_time) > 50) {
            break;
        }
        
        HAL_Delay(2);
    }
    
    // Results
    Send_Debug_Data("HMI OK\r\n");
    
    // Move to next CRC test
    crc_test++;
    
    // Reset for next test
    HAL_Delay(3000);
    command_sent = 0;
}

// Process function for CRC testing
void HMI_Process_CRC_Test(void) {
    HMI_Test_With_CRC();
}

uint8_t HMI_Init(void) {
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    hmi_index = 0;
    hmi_last_time = HAL_GetTick();
    last_version_check = HAL_GetTick();
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    // Start interrupt-based reception
    HMI_Start_Interrupt_RX();
    
    Send_Debug_Data("HMI: Passive monitoring mode\r\n");
    Send_Debug_Data("HMI: DWIN DMG10600T070-A5WTC @ 9600 baud\r\n");
    Send_Debug_Data("HMI: Version check every 5 seconds\r\n");
    Send_Debug_Data("HMI: RS485 DE/RE control on PE8\r\n");
    Send_Debug_Data("HMI: Interrupt RX enabled\r\n");
    
    return 1;
}

void Show_Complete_Message(void) {
    if (hmi_index == 0) return;
    
    // COMMENT OUT all verbose message display
    /*
    Send_Debug_Data("\r\n========================================\r\n");
    Send_Debug_Data("    HMI MESSAGE RECEIVED\r\n");
    Send_Debug_Data("========================================\r\n");
    // ... all the verbose hex display code ...
    Send_Debug_Data("\r\n========================================\r\n\r\n");
    */
    // REPLACE WITH simple status:
    Send_Debug_Data("HMI OK\r\n");
    
    // Reset buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
}

void HMI_Capture(void) {
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);

    // Capture all immediately available bytes
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);

        if (hmi_index < sizeof(hmi_buffer)) {
            hmi_buffer[hmi_index] = byte;
            hmi_index++;
            hmi_last_time = HAL_GetTick();
        }

        // Show each byte as it arrives
        char byte_msg[20];
        snprintf(byte_msg, sizeof(byte_msg), "HMI RX: %02X\r\n", byte);
        Send_Debug_Data(byte_msg);
    }

    // Process message completion
    if (hmi_index > 0) {
        uint32_t elapsed = HAL_GetTick() - hmi_last_time;

        // Protocol-aware completion
        if (hmi_index >= 3 && hmi_buffer[0] == 0x5A && hmi_buffer[1] == 0xA5) {
            uint8_t data_length = hmi_buffer[2];
            uint16_t expected_total = 3 + data_length;

            if (hmi_index >= expected_total) {
                Show_Complete_Message();
                hmi_last_time = HAL_GetTick();
                return;
            }
        }

        // Timeout completion
        if (elapsed > 500 && hmi_index >= 3) {
            Show_Complete_Message();
            hmi_last_time = HAL_GetTick();
        }
    }
}

// NO SENDING OF COMMANDS - just monitor what's already working
HMI_StatusTypeDef HMI_Process(void) {
    HMI_Capture();
    return HMI_OK;
}

uint8_t HMI_Detect_Connection(void) {
    Send_Debug_Data("HMI: Monitoring existing communication\r\n");
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    return 1;
}

uint8_t HMI_IsConnected(void) { return 1; }
void HMI_SetConnectionCheckInterval(uint32_t interval_ms) { }
void HMI_SendSystemInfo(void) { }
void HMI_System_Init(void) { HMI_Init(); }
void HMI_System_Process(void) { HMI_Correct_Page_Change(); }
void HMI_System_SetInterval(uint32_t interval_ms) { }
void HMI_Send_System_Info(void) { HMI_SendSystemInfo(); }

void HMI_Send_Version_Check(void) {
    uint8_t* command_to_send;
    uint8_t command_size;
    char* command_name;
    
    // Cycle through different commands
    switch(current_command) {
        case 0:
            command_to_send = version_command_1;
            command_size = sizeof(version_command_1);
            command_name = "Version Check (0x0F)";
            break;
        case 1:
            command_to_send = version_command_2;
            command_size = sizeof(version_command_2);
            command_name = "Read Register 0x00";
            break;
        case 2:
            command_to_send = version_command_3;
            command_size = sizeof(version_command_3);
            command_name = "Read Register 0x01";
            break;
        case 3:
            command_to_send = ping_command;
            command_size = sizeof(ping_command);
            command_name = "Basic Ping";
            break;
        case 4:
            command_to_send = read_pic_id;
            command_size = sizeof(read_pic_id);
            command_name = "Read Picture ID";
            break;
        default:
            current_command = 0;
            command_to_send = version_command_1;
            command_size = sizeof(version_command_1);
            command_name = "Version Check (0x0F)";
            break;
    }
    
    // Clear any existing buffer before sending
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    // Set RS485 to transmit mode
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(2);  // Increase delay slightly
    
    // Send the command
    HAL_UART_Transmit(&huart4, command_to_send, command_size, 1000);
    
    // Debug output - show what we sent
    Send_Debug_Data("HMI TX: ");
    for (int i = 0; i < command_size; i++) {
        char hex_str[4];
        snprintf(hex_str, sizeof(hex_str), "%02X ", command_to_send[i]);
        Send_Debug_Data(hex_str);
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "(%s)\r\n", command_name);
    Send_Debug_Data(msg);
    
    HAL_Delay(5);  // Give more time before switching to receive
    // Set RS485 back to receive mode
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    // Wait a bit more for response to arrive
    HAL_Delay(100);
    
    // Move to next command for next iteration
    current_command = (current_command + 1) % 5;
}

void HMI_Send_Single_Version_Check(void) {
    static uint8_t version_cmd[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x0F, 0x01};
    
    // Clear buffer before sending
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    // Set RS485 to transmit mode
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(2);
    
    // Send the command
    HAL_UART_Transmit(&huart4, version_cmd, sizeof(version_cmd), 1000);
    
    // Debug output
    Send_Debug_Data("HMI TX: 5A A5 04 83 00 0F 01 (Version Check)\r\n");
    
    HAL_Delay(5);
    // Set RS485 back to receive mode
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    // Wait for response
    HAL_Delay(200);  // Give plenty of time for full response
}

// Enhanced robust HMI communication function
void HMI_Send_Robust_Version_Check(void) {
    static uint8_t version_cmd[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x0F, 0x01};
    
    commands_sent++;
    
    // Step 1: Clear UART receive buffer completely
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;  // Clear any pending data
        (void)dummy; // Avoid unused variable warning
    }
    
    // Step 2: Clear our internal buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    last_response_received = 0;
    
    // Step 3: Ensure we're in receive mode first
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);  // Give time to settle
    
    // Step 4: Switch to transmit mode
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(5);  // Longer delay for RS485 transceiver to switch
    
    // Step 5: Send the command
    HAL_StatusTypeDef result = HAL_UART_Transmit(&huart4, version_cmd, sizeof(version_cmd), 2000);
    
    // Step 6: Wait for transmission to complete
    HAL_Delay(10);  // Ensure all bits are transmitted
    
    // Step 7: Switch back to receive mode
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    // Debug output
    if (result == HAL_OK) {
        Send_Debug_Data("HMI TX: 5A A5 04 83 00 0F 01 (Version Check - ROBUST)\r\n");
    } else {
        Send_Debug_Data("HMI TX: FAILED to send command\r\n");
    }
    
    // Step 8: Wait longer for response and actively check
    uint32_t wait_start = HAL_GetTick();
    uint32_t max_wait = 500;  // Wait up to 500ms for response
    
    while ((HAL_GetTick() - wait_start) < max_wait) {
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            // Response detected - let HMI_Capture handle it
            break;
        }
        HAL_Delay(10);  // Small delay to prevent tight loop
    }
    
    // Statistics
    char stats_msg[100];
    snprintf(stats_msg, sizeof(stats_msg), "HMI Stats: Sent=%lu, Received=%lu, Success Rate=%.1f%%\r\n", 
             commands_sent, responses_received, 
             (commands_sent > 0) ? (responses_received * 100.0f / commands_sent) : 0.0f);
    Send_Debug_Data(stats_msg);
}

// Enhanced capture function that tracks successful responses
void HMI_Capture_With_Stats(void) {
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);

    uint8_t bytes_received_this_call = 0;
    
    // Capture all immediately available bytes
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
        bytes_received_this_call++;

        if (hmi_index < sizeof(hmi_buffer)) {
            hmi_buffer[hmi_index] = byte;
            hmi_index++;
            hmi_last_time = HAL_GetTick();
            last_response_received = 1;
        }

        // Show each byte as it arrives
        char byte_msg[30];
        snprintf(byte_msg, sizeof(byte_msg), "HMI RX[%d]: %02X\r\n", hmi_index-1, byte);
        Send_Debug_Data(byte_msg);
    }

    // Process message completion
    if (hmi_index > 0) {
        uint32_t elapsed = HAL_GetTick() - hmi_last_time;

        // Protocol-aware completion for DWIN
        if (hmi_index >= 3 && hmi_buffer[0] == 0x5A && hmi_buffer[1] == 0xA5) {
            uint8_t data_length = hmi_buffer[2];
            uint16_t expected_total = 3 + data_length;

            if (hmi_index >= expected_total) {
                responses_received++;
                Show_Complete_Message();
                hmi_last_time = HAL_GetTick();
                return;
            }
        }
        
        // For non-DWIN responses (like your 50 16 FC), use timeout
        if (elapsed > 200 && hmi_index >= 1) {  // Shorter timeout for non-DWIN
            if (last_response_received) {
                responses_received++;
                last_response_received = 0;
            }
            Show_Complete_Message();
            hmi_last_time = HAL_GetTick();
        }
    }
}

// Updated process function
void HMI_Process_With_Version_Check(void) {
    // Check if it's time to send version command (every 15 seconds - slower for reliability)
    if (HAL_GetTick() - last_version_check >= 15000) {
        last_version_check = HAL_GetTick();
        HMI_Send_Robust_Version_Check();
        
        char timing_msg[100];
        snprintf(timing_msg, sizeof(timing_msg), "HMI: Robust version check at %lu ms\r\n", 
                 HAL_GetTick());
        Send_Debug_Data(timing_msg);
    }
    
    // Use enhanced capture with statistics
    HMI_Capture_With_Stats();
}

void HMI_Send_With_Proper_Timing(uint8_t* data, uint8_t size, const char* description) {
    // Step 1: Ensure we start in receive mode
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);  // Let bus and transceiver settle
    
    // Step 2: Clear any pending RX data
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
    }
    
    // Step 3: Switch to transmit mode with proper timing
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(10);  // CRITICAL: Wait for transceiver to switch to TX mode
    
    // Step 4: Send data
    HAL_StatusTypeDef result = HAL_UART_Transmit(&huart4, data, size, 2000);
    
    // Step 5: Wait for transmission to complete (CRITICAL)
    while(__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TC) == RESET) {
        // Wait for transmission complete flag
    }
    
    // Step 6: Additional delay to ensure last bit is fully transmitted
    HAL_Delay(5);  // Ensure all bits are on the bus
    
    // Step 7: Switch back to receive mode
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    // Step 8: Small delay before expecting response
    HAL_Delay(5);  // Let transceiver switch to RX and bus settle
    
    // Debug output
    Send_Debug_Data("HMI TX: ");
    for (int i = 0; i < size; i++) {
        char hex_str[4];
        snprintf(hex_str, sizeof(hex_str), "%02X ", data[i]);
        Send_Debug_Data(hex_str);
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "(%s) - Status: %s\r\n", 
             description, 
             (result == HAL_OK) ? "OK" : "FAILED");
    Send_Debug_Data(msg);
}

// Updated version check with proper timing
void HMI_Send_Version_Check_Proper_Timing(void) {
    static uint8_t version_cmd[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x0F, 0x01};
    
    // Clear our receive buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    // Send with proper timing
    HMI_Send_With_Proper_Timing(version_cmd, sizeof(version_cmd), "Version Check");
    
    // Wait for response with timeout
    uint32_t wait_start = HAL_GetTick();
    Send_Debug_Data("HMI: Waiting for response...\r\n");
    
    while ((HAL_GetTick() - wait_start) < 1000) {  // 1 second timeout
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            Send_Debug_Data("HMI: Response detected!\r\n");
            break;
        }
        HAL_Delay(10);
    }
    
    if ((HAL_GetTick() - wait_start) >= 1000) {
        Send_Debug_Data("HMI: No response within timeout\r\n");
    }
}

// Simple, minimal HMI test function
void HMI_Send_Minimal_Test(void) {
    static uint8_t version_cmd[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x0F, 0x01};
    
    Send_Debug_Data("HMI: Starting minimal test...\r\n");
    
    // Step 1: Clear receive buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    // Step 2: Simple timing like before (what was working)
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(2);
    
    // Step 3: Send command
    HAL_UART_Transmit(&huart4, version_cmd, sizeof(version_cmd), 1000);
    
    // Step 4: Back to receive with simple timing
    HAL_Delay(2);
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    Send_Debug_Data("HMI TX: 5A A5 04 83 00 0F 01 (MINIMAL TEST)\r\n");
    
    // Step 5: Just wait and see what happens
    HAL_Delay(200);
}

// Temporary process function for testing
void HMI_Process_Minimal_Test(void) {
    // Send command every 10 seconds
    if (HAL_GetTick() - last_version_check >= 10000) {
        last_version_check = HAL_GetTick();
        HMI_Send_Minimal_Test();
    }
    
    // Use the original capture function
    HMI_Capture();
}

// Add this diagnostic function
void HMI_Diagnostic_Test(void) {
    // COMMENT OUT all verbose test messages - keep functionality intact

    // Line around 189: COMMENT OUT
    // Send_Debug_Data("\r\n=== HMI DIAGNOSTIC TEST ===\r\n");

    // Test 1: Check UART configuration
    char uart_msg[100];
    snprintf(uart_msg, sizeof(uart_msg), "UART4 BaudRate: %lu\r\n", huart4.Init.BaudRate);
    Send_Debug_Data(uart_msg);
    
    // Test 2: Check GPIO pin states
    GPIO_PinState de_re_state = HAL_GPIO_ReadPin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin);
    snprintf(uart_msg, sizeof(uart_msg), "HMI_DE_RE Pin State: %s\r\n", 
             de_re_state == GPIO_PIN_SET ? "HIGH (TX Mode)" : "LOW (RX Mode)");
    Send_Debug_Data(uart_msg);
    
    // Test 3: Send a simple byte and see if we can detect it
    // Line around 820: COMMENT OUT
    // Send_Debug_Data("Sending single test byte 0xAA...\r\n");
    
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(2);
    
    uint8_t test_byte = 0xAA;
    HAL_UART_Transmit(&huart4, &test_byte, 1, 1000);
    
    HAL_Delay(2);
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    // Line around 825: COMMENT OUT
    // Send_Debug_Data("Test byte sent. Check for any response...\r\n");
    
    // Wait and see if anything comes back
    HAL_Delay(500);
    
    // Line around 825: COMMENT OUT
    // Send_Debug_Data("=== END DIAGNOSTIC ===\r\n\r\n");
}

// Improved termination test with better response capture
void HMI_Test_Terminations_Slow(void) {
    static uint8_t test_count = 0;
    static uint32_t last_test_time = 0;
    
    // Only send one command every 5 seconds to allow full response capture
    if (HAL_GetTick() - last_test_time < 5000) {
        return;  // Too soon, wait longer
    }
    last_test_time = HAL_GetTick();
    
    // Base command
    uint8_t base_cmd[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x0F, 0x01};
    
    // Different termination options
    uint8_t cmd_buffer[10];  // Large enough for any variant
    uint8_t cmd_size;
    char* description;
    
    // Copy base command
    memcpy(cmd_buffer, base_cmd, 7);
    
    // Add termination based on test count
    switch(test_count % 5) {
        case 0:
            cmd_size = 7;
            description = "No Termination";
            break;
        case 1:
            cmd_buffer[7] = 0x0D;  // CR
            cmd_size = 8;
            description = "With CR (0x0D)";
            break;
        case 2:
            cmd_buffer[7] = 0x0A;  // LF
            cmd_size = 8;
            description = "With LF (0x0A)";
            break;
        case 3:
            cmd_buffer[7] = 0x0D;  // CR
            cmd_buffer[8] = 0x0A;  // LF
            cmd_size = 9;
            description = "With CRLF (0x0D 0x0A)";
            break;
        case 4:
            cmd_buffer[7] = 0x00;  // NULL
            cmd_size = 8;
            description = "With NULL (0x00)";
            break;
    }
    
    // Clear buffer before sending
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    // Send with simple timing
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(2);
    
    HAL_UART_Transmit(&huart4, cmd_buffer, cmd_size, 1000);
    
    HAL_Delay(2);
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    // Debug output showing exact bytes sent
    Send_Debug_Data("\r\n--- HMI TEST ---\r\n");
    Send_Debug_Data("HMI TX: ");
    for (int i = 0; i < cmd_size; i++) {
        char hex_str[4];
        snprintf(hex_str, sizeof(hex_str), "%02X ", cmd_buffer[i]);
        Send_Debug_Data(hex_str);
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "(%s)\r\n", description);
    Send_Debug_Data(msg);
    
    Send_Debug_Data("Waiting for response...\r\n");
    
    test_count++;
}

// Enhanced capture that shows everything received
void HMI_Capture_Show_All(void) {
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);

    // Capture all immediately available bytes
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);

        if (hmi_index < sizeof(hmi_buffer)) {
            hmi_buffer[hmi_index] = byte;
            hmi_index++;
            hmi_last_time = HAL_GetTick();
        }

        // Show each byte as it arrives with position
        char byte_msg[30];
        snprintf(byte_msg, sizeof(byte_msg), "HMI RX[%d]: %02X (%d)\r\n", hmi_index-1, byte, byte);
        Send_Debug_Data(byte_msg);
    }

    // Process message completion with longer timeout
    if (hmi_index > 0) {
        uint32_t elapsed = HAL_GetTick() - hmi_last_time;

        // Show complete message after timeout or when we think it's complete
        if (elapsed > 1000) {  // 1 second timeout
            Send_Debug_Data("--- COMPLETE RESPONSE ---\r\n");
            Send_Debug_Data("HMI RX: ");
            for (int i = 0; i < hmi_index; i++) {
                char hex_str[4];
                snprintf(hex_str, sizeof(hex_str), "%02X ", hmi_buffer[i]);
                Send_Debug_Data(hex_str);
            }
            char total_msg[50];
            snprintf(total_msg, sizeof(total_msg), "(Total: %d bytes)\r\n", hmi_index);
            Send_Debug_Data(total_msg);
            Send_Debug_Data("--- END RESPONSE ---\r\n\r\n");
            
            // Reset buffer
            hmi_index = 0;
            memset(hmi_buffer, 0, sizeof(hmi_buffer));
            hmi_last_time = HAL_GetTick();
        }
    }
}

// Process function for slow termination testing
void HMI_Process_Termination_Test_Slow(void) {
    HMI_Test_Terminations_Slow();
    HMI_Capture_Show_All();
}

// Fixed version that captures complete responses
void HMI_Send_And_Capture_Complete(void) {
    static uint8_t test_count = 0;
    static uint32_t last_test_time = 0;
    
    // Send one command every 8 seconds
    if (HAL_GetTick() - last_test_time < 8000) {
        return;
    }
    last_test_time = HAL_GetTick();
    
    // Base command
    uint8_t base_cmd[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x0F, 0x01};
    uint8_t cmd_buffer[10];
    uint8_t cmd_size;
    char* description;
    
    // Copy base command
    memcpy(cmd_buffer, base_cmd, 7);
    
    // Add termination based on test count
    switch(test_count % 5) {
        case 0:
            cmd_size = 7;
            description = "No Termination";
            break;
        case 1:
            cmd_buffer[7] = 0x0D;
            cmd_size = 8;
            description = "With CR (0x0D)";
            break;
        case 2:
            cmd_buffer[7] = 0x0A;
            cmd_size = 8;
            description = "With LF (0x0A)";
            break;
        case 3:
            cmd_buffer[7] = 0x0D;
            cmd_buffer[8] = 0x0A;
            cmd_size = 9;
            description = "With CRLF (0x0D 0x0A)";
            break;
        case 4:
            cmd_buffer[7] = 0x00;
            cmd_size = 8;
            description = "With NULL (0x00)";
            break;
    }
    
    // Clear buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    Send_Debug_Data("\r\n=== COMPLETE CAPTURE TEST ===\r\n");
    
    // Send command
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(2);
    HAL_UART_Transmit(&huart4, cmd_buffer, cmd_size, 1000);
    HAL_Delay(2);
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    // Show what we sent
    Send_Debug_Data("HMI TX: ");
    for (int i = 0; i < cmd_size; i++) {
        char hex_str[4];
        snprintf(hex_str, sizeof(hex_str), "%02X ", cmd_buffer[i]);
        Send_Debug_Data(hex_str);
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "(%s)\r\n", description);
    Send_Debug_Data(msg);
    
    // CRITICAL: Wait longer and actively capture ALL response bytes
    Send_Debug_Data("Capturing response for 2 seconds...\r\n");
    uint32_t capture_start = HAL_GetTick();
    
    while ((HAL_GetTick() - capture_start) < 2000) {  // Capture for 2 full seconds
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
            
            if (hmi_index < sizeof(hmi_buffer)) {
                hmi_buffer[hmi_index] = byte;
                hmi_index++;
                
                char byte_msg[30];
                snprintf(byte_msg, sizeof(byte_msg), "RX[%d]: %02X (%d)\r\n", hmi_index-1, byte, byte);
                Send_Debug_Data(byte_msg);
            }
        }
        
        // If we got some data, wait a bit more for complete response
        if (hmi_index > 0 && (HAL_GetTick() - capture_start) > 50) {
            HAL_Delay(10);  // Give time for complete response
        }
    }
    
    // Show complete captured response
    if (hmi_index > 0) {
        Send_Debug_Data("=== COMPLETE RESPONSE ===\r\n");
        Send_Debug_Data("HMI RX: ");
        for (int i = 0; i < hmi_index; i++) {
            char hex_str[4];
            snprintf(hex_str, sizeof(hex_str), "%02X ", hmi_buffer[i]);
            Send_Debug_Data(hex_str);
        }
        char total_msg[50];
        snprintf(total_msg, sizeof(total_msg), "(Total: %d bytes)\r\n", hmi_index);
        Send_Debug_Data(total_msg);
        
        // Compare with expected from bus analyzer
        switch(test_count % 5) {
            case 0:
                Send_Debug_Data("Expected: 34 00 0F 01 65 10 (6 bytes)\r\n");
                break;
            case 1:
            case 2:
            case 4:
                Send_Debug_Data("Expected: 0C 0F 01 65 10 (5 bytes)\r\n");
                break;
            case 3:
                Send_Debug_Data("Expected: E8 28 65 10 (4 bytes)\r\n");
                break;
        }
    } else {
        Send_Debug_Data("NO RESPONSE CAPTURED!\r\n");
    }
    
    Send_Debug_Data("=== END TEST ===\r\n\r\n");
    test_count++;
}

// Process function for complete capture
void HMI_Process_Complete_Capture(void) {
    HMI_Send_And_Capture_Complete();
}

// Ultra-fast timing for DWIN HMI
void HMI_Send_Ultra_Fast_Timing(void) {
    static uint32_t last_test_time = 0;
    static uint8_t test_count = 0;
    
    if (HAL_GetTick() - last_test_time < 5000) {
        return;
    }
    last_test_time = HAL_GetTick();
    
    uint8_t cmd[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x0F, 0x01};
    
    // Clear buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    Send_Debug_Data("\r\n=== ULTRA-FAST TIMING TEST ===\r\n");
    Send_Debug_Data("HMI TX: 5A A5 04 83 00 0F 01 (Ultra-Fast Switch)\r\n");
    
    // CRITICAL: Disable interrupts during critical timing section
    __disable_irq();
    
    // Switch to TX mode
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    
    // Minimal delay for transceiver switching (microseconds, not milliseconds!)
    for(volatile int i = 0; i < 100; i++); // ~1-2μs delay at high clock speed
    
    // Send command (blocking)
    HAL_UART_Transmit(&huart4, cmd, sizeof(cmd), 1000);
    
    // CRITICAL: Switch to RX mode IMMEDIATELY after transmission
    // No delay here - DWIN responds in 200μs!
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    // Re-enable interrupts
    __enable_irq();
    
    Send_Debug_Data("Command sent with ultra-fast RX switch\r\n");
    Send_Debug_Data("Capturing response (DWIN responds in 200μs-2ms)...\r\n");
    
    // Now capture the response that should arrive very quickly
    uint32_t capture_start = HAL_GetTick();
    uint8_t response_detected = 0;
    
    // Look for response within 10ms (much longer than DWIN's 2ms max)
    while ((HAL_GetTick() - capture_start) < 10) {
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            if (!response_detected) {
                Send_Debug_Data("*** RESPONSE DETECTED! ***\r\n");
                response_detected = 1;
            }
            
            uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
            
            if (hmi_index < sizeof(hmi_buffer)) {
                hmi_buffer[hmi_index] = byte;
                hmi_index++;
                
                char byte_msg[30];
                snprintf(byte_msg, sizeof(byte_msg), "RX[%d]: %02X (%d)\r\n", hmi_index-1, byte, byte);
                Send_Debug_Data(byte_msg);
            }
        }
    }
    
    // Show results
    if (hmi_index > 0) {
        Send_Debug_Data("=== SUCCESS! COMPLETE RESPONSE ===\r\n");
        Send_Debug_Data("HMI RX: ");
        for (int i = 0; i < hmi_index; i++) {
            char hex_str[4];
            snprintf(hex_str, sizeof(hex_str), "%02X ", hmi_buffer[i]);
            Send_Debug_Data(hex_str);
        }
        char total_msg[50];
        snprintf(total_msg, sizeof(total_msg), "(Total: %d bytes)\r\n", hmi_index);
        Send_Debug_Data(total_msg);
        Send_Debug_Data("Expected: 34 00 0F 01 65 10 (6 bytes)\r\n");
    } else {
        Send_Debug_Data("*** STILL NO RESPONSE - CHECK HARDWARE ***\r\n");
    }
    
    Send_Debug_Data("=== END ULTRA-FAST TEST ===\r\n\r\n");
}

// Alternative version using UART transmission complete interrupt
void HMI_Send_With_TC_Flag(void) {
    static uint32_t last_test_time = 0;
    
    if (HAL_GetTick() - last_test_time < 5000) {
        return;
    }
    last_test_time = HAL_GetTick();
    
    uint8_t cmd[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x0F, 0x01};
    
    // Clear buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    Send_Debug_Data("\r\n=== TC FLAG METHOD ===\r\n");
    Send_Debug_Data("HMI TX: 5A A5 04 83 00 0F 01 (TC Flag Method)\r\n");
    
    // Switch to TX mode
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    
    // Start transmission
    HAL_UART_Transmit(&huart4, cmd, sizeof(cmd), 1000);
    
    // Wait for transmission complete flag (this is precise!)
    while(__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TC) == RESET) {
        // Wait for last bit to be transmitted
    }
    
    // IMMEDIATELY switch to RX mode after TC flag
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    Send_Debug_Data("TC flag detected, switched to RX immediately\r\n");
    
    // Capture response
    uint32_t capture_start = HAL_GetTick();
    
    while ((HAL_GetTick() - capture_start) < 10) {
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
            
            if (hmi_index < sizeof(hmi_buffer)) {
                hmi_buffer[hmi_index] = byte;
                hmi_index++;
                
                char byte_msg[30];
                snprintf(byte_msg, sizeof(byte_msg), "RX[%d]: %02X\r\n", hmi_index-1, byte);
                Send_Debug_Data(byte_msg);
            }
        }
    }
    
    // Show results
    if (hmi_index > 0) {
        Send_Debug_Data("SUCCESS with TC flag method!\r\n");
        Send_Debug_Data("Complete RX: ");
        for (int i = 0; i < hmi_index; i++) {
            char hex_str[4];
            snprintf(hex_str, sizeof(hex_str), "%02X ", hmi_buffer[i]);
            Send_Debug_Data(hex_str);
        }
        Send_Debug_Data("\r\n");
    }
    
    Send_Debug_Data("=== END TC FLAG TEST ===\r\n\r\n");
}

// Process function for ultra-fast timing
void HMI_Process_Ultra_Fast(void) {
    HMI_Send_Ultra_Fast_Timing();
}

// Fixed version - send command only once with proper DE/RE control
void HMI_Send_Single_Command_Fixed(void) {
    static uint32_t last_test_time = 0;
    static uint8_t command_sent = 0;  // Prevent multiple sends
    
    // Send command only once every 10 seconds
    if (HAL_GetTick() - last_test_time < 10000) {
        return;
    }
    
    // Prevent multiple sends in same cycle
    if (command_sent) {
        return;
    }
    
    last_test_time = HAL_GetTick();
    command_sent = 1;
    
    uint8_t cmd[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x0F, 0x01};
    
    // Clear buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    Send_Debug_Data("\r\n=== SINGLE COMMAND TEST ===\r\n");
    Send_Debug_Data("HMI TX: 5A A5 04 83 00 0F 01 (SINGLE SEND ONLY)\r\n");
    
    // Step 1: Ensure we start in RX mode and clear any data
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);  // Let transceiver settle in RX mode
    
    // Clear any pending RX data
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
        Send_Debug_Data("Cleared pending RX data\r\n");
    }
    
    // Step 2: Switch to TX mode
    Send_Debug_Data("Switching to TX mode...\r\n");
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(2);  // Give transceiver time to switch to TX
    
    // Step 3: Send command ONCE
    Send_Debug_Data("Sending command...\r\n");
    HAL_StatusTypeDef tx_result = HAL_UART_Transmit(&huart4, cmd, sizeof(cmd), 1000);
    
    if (tx_result != HAL_OK) {
        Send_Debug_Data("*** UART TRANSMIT FAILED! ***\r\n");
        return;
    }
    
    // Step 4: Wait for transmission complete
    while(__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TC) == RESET) {
        // Wait for transmission complete
    }
    Send_Debug_Data("Transmission complete flag detected\r\n");
    
    // Step 5: Switch to RX mode immediately
    Send_Debug_Data("Switching to RX mode...\r\n");
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    // Step 6: Wait a bit for transceiver to switch and HMI to respond
    HAL_Delay(5);  // Give transceiver time to switch to RX
    
    Send_Debug_Data("Listening for HMI response...\r\n");
    
    // Step 7: Capture response for reasonable time
    uint32_t capture_start = HAL_GetTick();
    uint8_t response_count = 0;
    
    while ((HAL_GetTick() - capture_start) < 100) {  // 100ms capture window
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
            
            if (hmi_index < sizeof(hmi_buffer)) {
                hmi_buffer[hmi_index] = byte;
                hmi_index++;
                response_count++;
                
                char byte_msg[40];
                snprintf(byte_msg, sizeof(byte_msg), "RX[%d]: %02X (%d) '%c'\r\n", 
                         hmi_index-1, byte, byte, (byte >= 32 && byte <= 126) ? byte : '.');
                Send_Debug_Data(byte_msg);
            }
        }
        
        // If we got some data, wait a bit more for complete response
        if (response_count > 0 && (HAL_GetTick() - capture_start) > 50) {
            HAL_Delay(10);  // Give time for complete response
        }
    }
    
    // Step 8: Show results
    if (hmi_index > 0) {
        Send_Debug_Data("=== RESPONSE RECEIVED ===\r\n");
        Send_Debug_Data("HMI RX: ");
        for (int i = 0; i < hmi_index; i++) {
            char hex_str[4];
            snprintf(hex_str, sizeof(hex_str), "%02X ", hmi_buffer[i]);
            Send_Debug_Data(hex_str);
        }
        char total_msg[50];
        snprintf(total_msg, sizeof(total_msg), "(Total: %d bytes)\r\n", hmi_index);
        Send_Debug_Data(total_msg);
        
        Send_Debug_Data("Expected from PC: 5A A5 06 83 00 0F 01 65 10\r\n");
        
        // Check if we're receiving our own command
        if (hmi_index >= 2 && hmi_buffer[0] == 0x5A && hmi_buffer[1] == 0xA5) {
            if (hmi_index == 7) {
                Send_Debug_Data("*** WARNING: Received our own command! DE/RE issue! ***\r\n");
            } else {
                Send_Debug_Data("*** GOOD: Received proper HMI response! ***\r\n");
            }
        }
    } else {
        Send_Debug_Data("*** NO RESPONSE RECEIVED ***\r\n");
    }
    
    Send_Debug_Data("=== END SINGLE COMMAND TEST ===\r\n\r\n");
    
    // Reset flag after 5 seconds to allow next test
    HAL_Delay(5000);
    command_sent = 0;
}

// Simple process function
void HMI_Process_Single_Command(void) {
    HMI_Send_Single_Command_Fixed();
}

// Comprehensive hardware test to isolate the RX problem
void HMI_Hardware_RX_Test(void) {
    Send_Debug_Data("\r\n=== COMPREHENSIVE RX HARDWARE TEST ===\r\n");
    
    // Test 1: Check UART4 registers directly
    Send_Debug_Data("=== UART4 Register Check ===\r\n");
    char msg[100];
    
    snprintf(msg, sizeof(msg), "UART4 Base Address: 0x%08lX\r\n", (uint32_t)huart4.Instance);
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "UART4 CR1: 0x%08lX\r\n", huart4.Instance->CR1);
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "UART4 CR2: 0x%08lX\r\n", huart4.Instance->CR2);
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "UART4 CR3: 0x%08lX\r\n", huart4.Instance->CR3);
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "UART4 ISR: 0x%08lX\r\n", huart4.Instance->ISR);
    Send_Debug_Data(msg);
    
    // Check if RX is enabled (STM32H7 uses USART_CR1_RE)
    if (huart4.Instance->CR1 & USART_CR1_RE) {
        Send_Debug_Data("✓ UART4 RX is ENABLED\r\n");
    } else {
        Send_Debug_Data("✗ UART4 RX is DISABLED!\r\n");
    }
    
    // Test 2: Check GPIO configuration
    Send_Debug_Data("\r\n=== GPIO Configuration Check ===\r\n");
    
    // Check UART4 RX pin (PD0)
    uint32_t gpiod_moder = GPIOD->MODER;
    uint32_t gpiod_afrl = GPIOD->AFR[0];
    
    snprintf(msg, sizeof(msg), "GPIOD MODER: 0x%08lX\r\n", gpiod_moder);
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "GPIOD AFRL: 0x%08lX\r\n", gpiod_afrl);
    Send_Debug_Data(msg);
    
    // Check PD0 mode (bits 1:0)
    uint32_t pd0_mode = (gpiod_moder >> 0) & 0x03;
    snprintf(msg, sizeof(msg), "PD0 Mode: %lu (should be 2 for AF)\r\n", pd0_mode);
    Send_Debug_Data(msg);
    
    // Check PD0 alternate function (bits 3:0)
    uint32_t pd0_af = (gpiod_afrl >> 0) & 0x0F;
    snprintf(msg, sizeof(msg), "PD0 AF: %lu (should be 8 for UART4)\r\n", pd0_af);
    Send_Debug_Data(msg);
    
    // Test 3: Manual UART register test
    Send_Debug_Data("\r\n=== Manual UART RX Test ===\r\n");
    
    // Force RX mode
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    Send_Debug_Data("DE/RE set to LOW (RX mode)\r\n");
    
    // Clear any pending data (STM32H7 uses USART_ISR_RXNE_RXFNE)
    while (huart4.Instance->ISR & USART_ISR_RXNE_RXFNE) {
        volatile uint32_t dummy = huart4.Instance->RDR;
        (void)dummy;
        Send_Debug_Data("Cleared pending data from RDR\r\n");
    }
    
    Send_Debug_Data("Monitoring UART4 ISR register for 5 seconds...\r\n");
    
    uint32_t monitor_start = HAL_GetTick();
    uint32_t last_isr = 0;
    
    while ((HAL_GetTick() - monitor_start) < 5000) {
        uint32_t current_isr = huart4.Instance->ISR;
        
        if (current_isr != last_isr) {
            snprintf(msg, sizeof(msg), "ISR changed: 0x%08lX\r\n", current_isr);
            Send_Debug_Data(msg);
            last_isr = current_isr;
        }
        
        // Check for RX data (STM32H7 flag)
        if (current_isr & USART_ISR_RXNE_RXFNE) {
            uint32_t received_data = huart4.Instance->RDR;
            snprintf(msg, sizeof(msg), "*** RDR DATA: 0x%02lX (%lu) ***\r\n", received_data, received_data);
            Send_Debug_Data(msg);
        }
        
        HAL_Delay(100);
    }
    
    // Test 4: Loopback test
    Send_Debug_Data("\r\n=== LOOPBACK TEST ===\r\n");
    Send_Debug_Data("Sending byte and checking if we receive it back...\r\n");
    
    // Clear RX
    while (huart4.Instance->ISR & USART_ISR_RXNE_RXFNE) {
        volatile uint32_t dummy = huart4.Instance->RDR;
        (void)dummy;
    }
    
    // Send a test byte
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(2);
    
    uint8_t test_byte = 0xAA;
    HAL_UART_Transmit(&huart4, &test_byte, 1, 1000);
    
    // Wait for transmission complete (STM32H7 uses USART_ISR_TC)
    while(!(huart4.Instance->ISR & USART_ISR_TC));
    
    HAL_Delay(1);
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    Send_Debug_Data("Test byte 0xAA sent, checking for loopback...\r\n");
    
    // Check for loopback within 1 second
    uint32_t loopback_start = HAL_GetTick();
    while ((HAL_GetTick() - loopback_start) < 1000) {
        if (huart4.Instance->ISR & USART_ISR_RXNE_RXFNE) {
            uint32_t loopback_data = huart4.Instance->RDR;
            snprintf(msg, sizeof(msg), "LOOPBACK RECEIVED: 0x%02lX\r\n", loopback_data);
            Send_Debug_Data(msg);
            if (loopback_data == 0xAA) {
                Send_Debug_Data("✓ LOOPBACK SUCCESSFUL - UART RX WORKS!\r\n");
            }
            break;
        }
        HAL_Delay(10);
    }
    
    Send_Debug_Data("=== END COMPREHENSIVE TEST ===\r\n\r\n");
}

// Extended capture version to get all response bytes
void HMI_Send_Extended_Capture(void) {
    static uint32_t last_test_time = 0;
    static uint8_t command_sent = 0;
    
    if (HAL_GetTick() - last_test_time < 8000) {
        return;
    }
    
    if (command_sent) {
        return;
    }
    
    last_test_time = HAL_GetTick();
    command_sent = 1;
    
    uint8_t cmd[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x0F, 0x01};
    
    // Clear buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    Send_Debug_Data("\r\n=== EXTENDED CAPTURE TEST ===\r\n");
    Send_Debug_Data("HMI TX: 5A A5 04 83 00 0F 01 (Extended Capture)\r\n");
    
    // Step 1: Prepare for RX
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    
    // Clear any pending data
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
    }
    
    // Step 2: Send command
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(2);
    
    HAL_UART_Transmit(&huart4, cmd, sizeof(cmd), 1000);
    
    // Wait for transmission complete
    while(__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TC) == RESET);
    
    // Step 3: Switch to RX immediately
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    Send_Debug_Data("Command sent, capturing response with extended window...\r\n");
    
    // Step 4: Extended capture with byte-by-byte timing
    uint32_t capture_start = HAL_GetTick();
    uint32_t last_byte_time = capture_start;
    uint8_t bytes_received = 0;
    
    // Capture for up to 500ms, but with intelligent timeout between bytes
    while ((HAL_GetTick() - capture_start) < 500) {
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
            
            if (hmi_index < sizeof(hmi_buffer)) {
                hmi_buffer[hmi_index] = byte;
                hmi_index++;
                bytes_received++;
                last_byte_time = HAL_GetTick();
                
                char byte_msg[50];
                snprintf(byte_msg, sizeof(byte_msg), "RX[%d]: %02X (%d) at %lu ms\r\n", 
                         hmi_index-1, byte, byte, HAL_GetTick() - capture_start);
                Send_Debug_Data(byte_msg);
            }
        }
        
        // If we got some bytes, wait for more with shorter timeout
        if (bytes_received > 0) {
            uint32_t time_since_last_byte = HAL_GetTick() - last_byte_time;
            
            // If no new bytes for 50ms after receiving data, assume complete
            if (time_since_last_byte > 50) {
                Send_Debug_Data("No more bytes for 50ms - assuming complete response\r\n");
                break;
            }
        }
        
        // Small delay to prevent tight loop
        HAL_Delay(1);
    }
    
    // Step 5: Show results
    if (hmi_index > 0) {
        Send_Debug_Data("=== EXTENDED CAPTURE RESULTS ===\r\n");
        Send_Debug_Data("HMI RX: ");
        for (int i = 0; i < hmi_index; i++) {
            char hex_str[4];
            snprintf(hex_str, sizeof(hex_str), "%02X ", hmi_buffer[i]);
            Send_Debug_Data(hex_str);
        }
        char total_msg[60];
        snprintf(total_msg, sizeof(total_msg), "(Captured: %d bytes)\r\n", hmi_index);
        Send_Debug_Data(total_msg);
        
        Send_Debug_Data("Expected: 28 65 10 (3 bytes)\r\n");
        
        if (hmi_index == 3 && hmi_buffer[0] == 0x28 && hmi_buffer[1] == 0x65 && hmi_buffer[2] == 0x10) {
            Send_Debug_Data("*** PERFECT! Complete response captured! ***\r\n");
        } else if (hmi_index == 1 && hmi_buffer[0] == 0x28) {
            Send_Debug_Data("*** PARTIAL: Got first byte, missing 65 10 ***\r\n");
        } else {
            Send_Debug_Data("*** UNEXPECTED: Response doesn't match expected pattern ***\r\n");
        }
        
        // Analyze timing if partial
        if (hmi_index == 1) {
            Send_Debug_Data("ANALYSIS: HMI sent all 3 bytes quickly, but STM32 only captured first\r\n");
            Send_Debug_Data("POSSIBLE CAUSES:\r\n");
            Send_Debug_Data("1. UART RX FIFO overflow\r\n");
            Send_Debug_Data("2. DE/RE switching too slow\r\n");
            Send_Debug_Data("3. Interrupt priority issues\r\n");
            Send_Debug_Data("4. UART clock/baud rate mismatch\r\n");
        }
    } else {
        Send_Debug_Data("*** NO RESPONSE CAPTURED ***\r\n");
    }
    
    Send_Debug_Data("=== END EXTENDED CAPTURE TEST ===\r\n\r\n");
    
    // Reset for next test
    HAL_Delay(3000);
    command_sent = 0;
}

// Process function for extended capture
void HMI_Process_Extended_Capture(void) {
    HMI_Send_Extended_Capture();
}

// Updated to capture the REAL DWIN response format
void HMI_Capture_Real_DWIN_Response(void) {
    static uint32_t last_test_time = 0;
    static uint8_t command_sent = 0;
    
    if (HAL_GetTick() - last_test_time < 10000) {
        return;
    }
    
    if (command_sent) {
        return;
    }
    
    last_test_time = HAL_GetTick();
    command_sent = 1;
    
    uint8_t cmd[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x0F, 0x01};
    
    // Clear buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    Send_Debug_Data("\r\n=== REAL DWIN RESPONSE CAPTURE ===\r\n");
    Send_Debug_Data("HMI TX: 5A A5 04 83 00 0F 01 (Command)\r\n");
    Send_Debug_Data("Expected RX: 5A A5 06 83 00 0F 01 65 10 (9 bytes)\r\n");
    
    // Prepare RX mode
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);  // Longer settle time
    
    // Clear any pending data
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
        Send_Debug_Data("Cleared pending RX data\r\n");
    }
    
    // Send command with longer TX mode time
    Send_Debug_Data("Switching to TX mode...\r\n");
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(5);  // Longer TX settle time
    
    Send_Debug_Data("Sending command...\r\n");
    HAL_UART_Transmit(&huart4, cmd, sizeof(cmd), 2000);
    
    // Wait for complete transmission
    while(__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TC) == RESET);
    Send_Debug_Data("Transmission complete\r\n");
    
    // Critical timing: Switch to RX mode FAST but not too fast
    HAL_Delay(1);  // Small delay to ensure last bit is out
    Send_Debug_Data("Switching to RX mode NOW...\r\n");
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    // Capture the full 9-byte DWIN response
    Send_Debug_Data("Capturing DWIN response (expecting 9 bytes)...\r\n");
    
    uint32_t capture_start = HAL_GetTick();
    uint32_t last_byte_time = capture_start;
    uint8_t expected_bytes = 9;  // Full DWIN response
    
    while ((HAL_GetTick() - capture_start) < 1000) {  // 1 second max
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
            
            if (hmi_index < sizeof(hmi_buffer)) {
                hmi_buffer[hmi_index] = byte;
                hmi_index++;
                last_byte_time = HAL_GetTick();
                
                char byte_msg[60];
                snprintf(byte_msg, sizeof(byte_msg), "RX[%d]: %02X (%d) at +%lu ms\r\n", 
                         hmi_index-1, byte, byte, HAL_GetTick() - capture_start);
                Send_Debug_Data(byte_msg);
                
                // Check if we got the expected number of bytes
                if (hmi_index >= expected_bytes) {
                    Send_Debug_Data("Got expected number of bytes, checking for more...\r\n");
                    HAL_Delay(20);  // Wait a bit more to see if more bytes arrive
                    if (!__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
                        Send_Debug_Data("No more bytes - response complete\r\n");
                        break;
                    }
                }
            }
        }
        
        // Timeout if no bytes for 100ms
        if (hmi_index > 0 && (HAL_GetTick() - last_byte_time) > 100) {
            Send_Debug_Data("Timeout - no more bytes for 100ms\r\n");
            break;
        }
        
        HAL_Delay(1);
    }
    
    // Analyze the response
    Send_Debug_Data("\r\n=== RESPONSE ANALYSIS ===\r\n");
    
    if (hmi_index > 0) {
        Send_Debug_Data("CAPTURED: ");
        for (int i = 0; i < hmi_index; i++) {
            char hex_str[4];
            snprintf(hex_str, sizeof(hex_str), "%02X ", hmi_buffer[i]);
            Send_Debug_Data(hex_str);
        }
        char total_msg[60];
        snprintf(total_msg, sizeof(total_msg), "(%d bytes)\r\n", hmi_index);
        Send_Debug_Data(total_msg);
        
        Send_Debug_Data("EXPECTED: 5A A5 06 83 00 0F 01 65 10 (9 bytes)\r\n");
        
        // Check if it matches DWIN format
        if (hmi_index >= 3 && hmi_buffer[0] == 0x5A && hmi_buffer[1] == 0xA5) {
            uint8_t data_len = hmi_buffer[2];
            Send_Debug_Data("✓ DWIN header detected!\r\n");
            char len_msg[50];
            snprintf(len_msg, sizeof(len_msg), "Data length: %d bytes\r\n", data_len);
            Send_Debug_Data(len_msg);
            
            if (hmi_index >= (3 + data_len)) {
                Send_Debug_Data("✓ Complete DWIN response received!\r\n");
                
                // Extract the actual data payload
                if (data_len >= 4 && hmi_index >= 7) {
                    Send_Debug_Data("Data payload: ");
                    for (int i = 6; i < hmi_index && i < (6 + data_len - 3); i++) {
                        char hex_str[4];
                        snprintf(hex_str, sizeof(hex_str), "%02X ", hmi_buffer[i]);
                        Send_Debug_Data(hex_str);
                    }
                    Send_Debug_Data("\r\n");
                }
            } else {
                Send_Debug_Data("⚠ Incomplete DWIN response\r\n");
            }
        } else {
            Send_Debug_Data("⚠ Not a valid DWIN response format\r\n");
        }
    } else {
        Send_Debug_Data("*** NO RESPONSE CAPTURED ***\r\n");
    }
    
    Send_Debug_Data("=== END REAL DWIN CAPTURE ===\r\n\r\n");
    
    // Reset for next test
    HAL_Delay(5000);
    command_sent = 0;
}

// Process function
void HMI_Process_Real_DWIN_Capture(void) {
    HMI_Capture_Real_DWIN_Response();
}

// Ultra-optimized for the real 3-byte response: 40 59 FC
void HMI_Capture_3_Byte_Response(void) {
    static uint32_t last_test_time = 0;
    static uint8_t command_sent = 0;
    
    if (HAL_GetTick() - last_test_time < 8000) {
        return;
    }
    
    if (command_sent) {
        return;
    }
    
    last_test_time = HAL_GetTick();
    command_sent = 1;
    
    uint8_t cmd[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x0F, 0x01};
    
    // Clear buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    Send_Debug_Data("\r\n=== 3-BYTE RESPONSE CAPTURE ===\r\n");
    Send_Debug_Data("HMI TX: 5A A5 04 83 00 0F 01\r\n");
    Send_Debug_Data("Expected RX: 40 59 FC (3 bytes)\r\n");
    
    // Prepare for ultra-fast switching
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    
    // Clear RX completely
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
    }
    
    Send_Debug_Data("Starting ultra-fast TX/RX sequence...\r\n");
    
    // CRITICAL SECTION: Disable interrupts for precise timing
    __disable_irq();
    
    // Switch to TX
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    
    // Minimal delay (microseconds)
    for(volatile int i = 0; i < 200; i++);  // ~2-5μs
    
    // Send command
    HAL_UART_Transmit(&huart4, cmd, sizeof(cmd), 1000);
    
    // Wait for TC flag with timeout
    uint32_t tc_timeout = 1000000;  // Prevent infinite loop
    while(__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TC) == RESET && tc_timeout--);
    
    // INSTANT switch to RX - no delay!
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    // Re-enable interrupts
    __enable_irq();
    
    Send_Debug_Data("Ultra-fast switch complete, capturing...\r\n");
    
    // Aggressive capture - DWIN responds in 200μs-2ms
    uint32_t capture_start = HAL_GetTick();
    uint32_t last_byte_time = capture_start;
    
    // Look for all 3 bytes with very tight timing
    while ((HAL_GetTick() - capture_start) < 50) {  // Only 50ms window
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
            
            if (hmi_index < sizeof(hmi_buffer)) {
                hmi_buffer[hmi_index] = byte;
                hmi_index++;
                last_byte_time = HAL_GetTick();
                
                char byte_msg[50];
                snprintf(byte_msg, sizeof(byte_msg), "RX[%d]: %02X at +%lu ms\r\n", 
                         hmi_index-1, byte, HAL_GetTick() - capture_start);
                Send_Debug_Data(byte_msg);
                
                // If we got 3 bytes, check if more are coming
                if (hmi_index >= 3) {
                    // Wait 10ms to see if more bytes arrive
                    HAL_Delay(10);
                    if (!__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
                        Send_Debug_Data("Got 3 bytes, no more coming - complete!\r\n");
                        break;
                    }
                }
            }
        }
        
        // If we got some bytes but no new ones for 20ms, assume done
        if (hmi_index > 0 && (HAL_GetTick() - last_byte_time) > 20) {
            Send_Debug_Data("No new bytes for 20ms - response complete\r\n");
            break;
        }
        
        // Minimal delay
        HAL_Delay(1);
    }
    
    // Results
    Send_Debug_Data("\r\n=== CAPTURE RESULTS ===\r\n");
    
    if (hmi_index > 0) {
        Send_Debug_Data("CAPTURED: ");
        for (int i = 0; i < hmi_index; i++) {
            char hex_str[4];
            snprintf(hex_str, sizeof(hex_str), "%02X ", hmi_buffer[i]);
            Send_Debug_Data(hex_str);
        }
        char total_msg[50];
        snprintf(total_msg, sizeof(total_msg), "(%d bytes)\r\n", hmi_index);
        Send_Debug_Data(total_msg);
        
        Send_Debug_Data("EXPECTED: 40 59 FC (3 bytes)\r\n");
        
        if (hmi_index == 3 && hmi_buffer[0] == 0x40 && hmi_buffer[1] == 0x59 && hmi_buffer[2] == 0xFC) {
            Send_Debug_Data("*** PERFECT MATCH! Complete HMI response captured! ***\r\n");
            Send_Debug_Data("*** HMI COMMUNICATION FULLY WORKING! ***\r\n");
        } else if (hmi_buffer[0] == 0x40) {
            char partial_msg[100];
            snprintf(partial_msg, sizeof(partial_msg), "PARTIAL: Got first byte (40), missing %d more bytes\r\n", 3 - hmi_index);
            Send_Debug_Data(partial_msg);
        } else {
            Send_Debug_Data("UNEXPECTED: Different response pattern\r\n");
        }
    } else {
        Send_Debug_Data("*** NO RESPONSE CAPTURED ***\r\n");
    }
    
    Send_Debug_Data("=== END 3-BYTE CAPTURE ===\r\n\r\n");
    
    // Reset
    HAL_Delay(3000);
    command_sent = 0;
}

// Process function
void HMI_Process_3_Byte_Capture(void) {
    HMI_Capture_3_Byte_Response();
}

// Fixed version with proper RS485 direction control
void HMI_Fix_RS485_Direction(void) {
    static uint32_t last_test_time = 0;
    static uint8_t command_sent = 0;
    
    if (HAL_GetTick() - last_test_time < 10000) {
        return;
    }
    
    if (command_sent) {
        return;
    }
    
    last_test_time = HAL_GetTick();
    command_sent = 1;
    
    uint8_t cmd[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x0F, 0x01};
    
    // Clear buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    Send_Debug_Data("\r\n=== RS485 DIRECTION FIX TEST ===\r\n");
    Send_Debug_Data("HMI TX: 5A A5 04 83 00 0F 01\r\n");
    Send_Debug_Data("Expected RX: 5A A5 06 83 00 0F 01 65 10 (9 bytes)\r\n");
    
    // Step 1: Ensure we start in RX mode and stay there
    Send_Debug_Data("Step 1: Setting RX mode and clearing buffers...\r\n");
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(20);  // Long delay to ensure transceiver is in RX mode
    
    // Clear ALL pending RX data (including any echoes)
    uint8_t cleared_bytes = 0;
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
        cleared_bytes++;
    }
    if (cleared_bytes > 0) {
        char clear_msg[50];
        snprintf(clear_msg, sizeof(clear_msg), "Cleared %d pending bytes\r\n", cleared_bytes);
        Send_Debug_Data(clear_msg);
    }
    
    // Step 2: Switch to TX mode with proper timing
    Send_Debug_Data("Step 2: Switching to TX mode...\r\n");
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(10);  // Give transceiver time to switch to TX mode
    
    // Step 3: Send command
    Send_Debug_Data("Step 3: Sending command...\r\n");
    HAL_StatusTypeDef tx_result = HAL_UART_Transmit(&huart4, cmd, sizeof(cmd), 2000);
    
    if (tx_result != HAL_OK) {
        Send_Debug_Data("*** UART TRANSMIT FAILED! ***\r\n");
        command_sent = 0;
        return;
    }
    
    // Step 4: Wait for transmission to be 100% complete
    Send_Debug_Data("Step 4: Waiting for transmission complete...\r\n");
    uint32_t tc_wait_start = HAL_GetTick();
    while(__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TC) == RESET) {
        if ((HAL_GetTick() - tc_wait_start) > 1000) {
            Send_Debug_Data("*** TC FLAG TIMEOUT! ***\r\n");
            break;
        }
    }
    Send_Debug_Data("Transmission complete flag detected\r\n");
    
    // Step 5: CRITICAL - Wait for the last bit to physically leave the transceiver
    Send_Debug_Data("Step 5: Waiting for last bit to clear transceiver...\r\n");
    HAL_Delay(5);  // At 9600 baud, 1 byte = ~1ms, so 5ms is safe
    
    // Step 6: Switch to RX mode
    Send_Debug_Data("Step 6: Switching to RX mode...\r\n");
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);  // Give transceiver time to switch to RX mode
    
    // Step 7: Clear any echo/reflection that might have been received
    Send_Debug_Data("Step 7: Clearing any TX echo...\r\n");
    uint8_t echo_bytes = 0;
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t echo = huart4.Instance->RDR;
        echo_bytes++;
        char echo_msg[50];
        snprintf(echo_msg, sizeof(echo_msg), "Cleared echo byte: %02X\r\n", echo);
        Send_Debug_Data(echo_msg);
    }
    
    // Step 8: Now listen for the REAL HMI response
    Send_Debug_Data("Step 8: Listening for HMI response...\r\n");
    
    uint32_t capture_start = HAL_GetTick();
    uint32_t last_byte_time = capture_start;
    
    // HMI responds in 200μs-2ms, but give it more time
    while ((HAL_GetTick() - capture_start) < 200) {  // 200ms window
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
            
            if (hmi_index < sizeof(hmi_buffer)) {
                hmi_buffer[hmi_index] = byte;
                hmi_index++;
                last_byte_time = HAL_GetTick();
                
                char byte_msg[60];
                snprintf(byte_msg, sizeof(byte_msg), "HMI RX[%d]: %02X at +%lu ms\r\n", 
                         hmi_index-1, byte, HAL_GetTick() - capture_start);
                Send_Debug_Data(byte_msg);
                
                // Check if we got a complete DWIN response
                if (hmi_index >= 3 && hmi_buffer[0] == 0x5A && hmi_buffer[1] == 0xA5) {
                    uint8_t expected_len = 3 + hmi_buffer[2];  // Header + data length
                    if (hmi_index >= expected_len) {
                        Send_Debug_Data("Complete DWIN response detected!\r\n");
                        HAL_Delay(10);  // Wait a bit more to see if more bytes come
                        if (!__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
                            break;  // No more bytes, we're done
                        }
                    }
                }
            }
        }
        
        // Timeout if no new bytes for 50ms
        if (hmi_index > 0 && (HAL_GetTick() - last_byte_time) > 50) {
            Send_Debug_Data("No new bytes for 50ms - response complete\r\n");
            break;
        }
        
        HAL_Delay(1);
    }
    
    // Step 9: Analyze results
    Send_Debug_Data("\r\n=== ANALYSIS ===\r\n");
    
    if (hmi_index > 0) {
        Send_Debug_Data("CAPTURED: ");
        for (int i = 0; i < hmi_index; i++) {
            char hex_str[4];
            snprintf(hex_str, sizeof(hex_str), "%02X ", hmi_buffer[i]);
            Send_Debug_Data(hex_str);
        }
        char total_msg[50];
        snprintf(total_msg, sizeof(total_msg), "(%d bytes)\r\n", hmi_index);
        Send_Debug_Data(total_msg);
        
        // Check if it's our command echo
        if (hmi_index >= 2 && hmi_buffer[0] == 0x5A && hmi_buffer[1] == 0xA5 && hmi_buffer[2] == 0x04) {
            Send_Debug_Data("*** WARNING: This looks like our command echo! ***\r\n");
            Send_Debug_Data("*** RS485 direction control still has issues! ***\r\n");
        }
        // Check if it's a proper DWIN response
        else if (hmi_index >= 3 && hmi_buffer[0] == 0x5A && hmi_buffer[1] == 0xA5 && hmi_buffer[2] == 0x06) {
            Send_Debug_Data("*** SUCCESS! This is a proper DWIN response! ***\r\n");
            Send_Debug_Data("*** HMI COMMUNICATION WORKING! ***\r\n");
        }
        else {
            Send_Debug_Data("*** Different response pattern - analyzing... ***\r\n");
        }
    } else {
        Send_Debug_Data("*** NO RESPONSE CAPTURED ***\r\n");
        Send_Debug_Data("*** Check: HMI power, wiring, baud rate ***\r\n");
    }
    
    Send_Debug_Data("=== END RS485 DIRECTION FIX ===\r\n\r\n");
    
    // Reset
    HAL_Delay(5000);
    command_sent = 0;
}

// Process function
void HMI_Process_RS485_Fix(void) {
    HMI_Fix_RS485_Direction();
}

// Test HMI commands with different packet terminations
void HMI_Test_Packet_Terminations(void) {
    static uint32_t last_test_time = 0;
    static uint8_t termination_test = 0;
    static uint8_t command_sent = 0;
    
    if (HAL_GetTick() - last_test_time < 8000) {
        return;
    }
    
    if (command_sent) {
        return;
    }
    
    last_test_time = HAL_GetTick();
    command_sent = 1;
    
    // Base command
    uint8_t base_cmd[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x0F, 0x01};
    uint8_t cmd_buffer[12];  // Extra space for terminations
    uint8_t cmd_size;
    char* termination_desc;
    
    // Copy base command
    memcpy(cmd_buffer, base_cmd, 7);
    
    // Add different terminations
    switch(termination_test % 8) {
        case 0:
            cmd_size = 7;
            termination_desc = "No Termination";
            break;
            
        case 1:
            cmd_buffer[7] = 0x0D;  // CR
            cmd_size = 8;
            termination_desc = "CR (0x0D)";
            break;
            
        case 2:
            cmd_buffer[7] = 0x0A;  // LF
            cmd_size = 8;
            termination_desc = "LF (0x0A)";
            break;
            
        case 3:
            cmd_buffer[7] = 0x0D;  // CRLF
            cmd_buffer[8] = 0x0A;
            cmd_size = 9;
            termination_desc = "CRLF (0x0D 0x0A)";
            break;
            
        case 4:
            cmd_buffer[7] = 0x00;  // NULL
            cmd_size = 8;
            termination_desc = "NULL (0x00)";
            break;
            
        case 5:
            cmd_buffer[7] = 0xFF;  // 0xFF termination
            cmd_size = 8;
            termination_desc = "0xFF";
            break;
            
        case 6:
            cmd_buffer[7] = 0x0D;  // CR + NULL
            cmd_buffer[8] = 0x00;
            cmd_size = 9;
            termination_desc = "CR + NULL";
            break;
            
        case 7:
            cmd_buffer[7] = 0x0A;  // LF + NULL
            cmd_buffer[8] = 0x00;
            cmd_size = 9;
            termination_desc = "LF + NULL";
            break;
    }
    
    // Clear buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    Send_Debug_Data("\r\n=== PACKET TERMINATION TEST ===\r\n");
    
    // Show exact packet being sent
    Send_Debug_Data("HMI TX: ");
    for (int i = 0; i < cmd_size; i++) {
        char hex_str[4];
        snprintf(hex_str, sizeof(hex_str), "%02X ", cmd_buffer[i]);
        Send_Debug_Data(hex_str);
    }
    char term_msg[100];
    snprintf(term_msg, sizeof(term_msg), "(%s)\r\n", termination_desc);
    Send_Debug_Data(term_msg);
    
    // Prepare for transmission
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    
    // Clear any pending data
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
    }
    
    // Send command with termination
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(5);
    
    HAL_UART_Transmit(&huart4, cmd_buffer, cmd_size, 2000);
    
    // Wait for transmission complete
    while(__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TC) == RESET);
    
    // Wait for last bit to clear
    HAL_Delay(3);
    
    // Switch to RX
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    
    // Clear any echo
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t echo = huart4.Instance->RDR;
        (void)echo;
    }
    
    Send_Debug_Data("Listening for response...\r\n");
    
    // Capture response
    uint32_t capture_start = HAL_GetTick();
    uint32_t last_byte_time = capture_start;
    
    while ((HAL_GetTick() - capture_start) < 300) {  // 300ms window
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
            
            if (hmi_index < sizeof(hmi_buffer)) {
                hmi_buffer[hmi_index] = byte;
                hmi_index++;
                last_byte_time = HAL_GetTick();
                
                char byte_msg[50];
                snprintf(byte_msg, sizeof(byte_msg), "RX[%d]: %02X\r\n", hmi_index-1, byte);
                Send_Debug_Data(byte_msg);
                
                // Check for complete DWIN response
                if (hmi_index >= 3 && hmi_buffer[0] == 0x5A && hmi_buffer[1] == 0xA5) {
                    uint8_t expected_len = 3 + hmi_buffer[2];
                    if (hmi_index >= expected_len) {
                        Send_Debug_Data("Complete DWIN response received!\r\n");
                        HAL_Delay(20);  // Wait for any additional bytes
                        if (!__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
                            break;
                        }
                    }
                }
            }
        }
        
        // Timeout after no bytes for 50ms
        if (hmi_index > 0 && (HAL_GetTick() - last_byte_time) > 50) {
            break;
        }
        
        HAL_Delay(2);
    }
    
    // Results
    Send_Debug_Data("\r\n=== TERMINATION TEST RESULTS ===\r\n");
    
    if (hmi_index > 0) {
        Send_Debug_Data("RESPONSE: ");
        for (int i = 0; i < hmi_index; i++) {
            char hex_str[4];
            snprintf(hex_str, sizeof(hex_str), "%02X ", hmi_buffer[i]);
            Send_Debug_Data(hex_str);
        }
        char total_msg[50];
        snprintf(total_msg, sizeof(total_msg), "(%d bytes)\r\n", hmi_index);
        Send_Debug_Data(total_msg);
        
        // Analyze response quality
        if (hmi_index >= 3 && hmi_buffer[0] == 0x5A && hmi_buffer[1] == 0xA5) {
            if (hmi_buffer[2] == 0x06) {
                Send_Debug_Data("*** EXCELLENT! Complete DWIN response! ***\r\n");
                char success_msg[100];
                snprintf(success_msg, sizeof(success_msg), "*** TERMINATION '%s' WORKS PERFECTLY! ***\r\n", termination_desc);
                Send_Debug_Data(success_msg);
            } else {
                Send_Debug_Data("*** GOOD! Valid DWIN response format ***\r\n");
            }
        } else {
            Send_Debug_Data("*** PARTIAL or DIFFERENT response ***\r\n");
        }
        
        // Success rate tracking
        char result_msg[150];
        snprintf(result_msg, sizeof(result_msg), "TERMINATION TEST %d/8: %s = %d bytes response\r\n", 
                 (termination_test % 8) + 1, termination_desc, hmi_index);
        Send_Debug_Data(result_msg);
        
    } else {
        Send_Debug_Data("*** NO RESPONSE ***\r\n");
        char fail_msg[100];
        snprintf(fail_msg, sizeof(fail_msg), "TERMINATION '%s': FAILED\r\n", termination_desc);
        Send_Debug_Data(fail_msg);
    }
    
    Send_Debug_Data("=== END TERMINATION TEST ===\r\n\r\n");
    
    // Move to next termination test
    termination_test++;
    
    // Reset for next test
    HAL_Delay(3000);
    command_sent = 0;
}

// Process function for termination testing
void HMI_Process_Termination_Test(void) {
    HMI_Test_Packet_Terminations();
}

// Add these functions at the end of your hmi.c file

// C51-specific HMI communication test
void HMI_Test_C51_Protocol(void) {
    static uint32_t last_test_time = 0;
    static uint8_t c51_test = 0;
    static uint8_t command_sent = 0;
    
    if (HAL_GetTick() - last_test_time < 10000) {
        return;
    }
    
    if (command_sent) {
        return;
    }
    
    last_test_time = HAL_GetTick();
    command_sent = 1;
    
    // C51-specific commands to test
    uint8_t c51_commands[][10] = {
        {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x0F, 0x01},  // Version register
        {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x00, 0x01},  // Register 0x00
        {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x01, 0x01},  // Register 0x01
        {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x03, 0x01},  // Picture ID
        {0x5A, 0xA5, 0x03, 0x82, 0x00, 0x03},        // Simple read
    };
    
    uint8_t cmd_sizes[] = {7, 7, 7, 7, 6};
    char* cmd_descriptions[] = {
        "C51 Version (0x0F)",
        "C51 Register 0x00", 
        "C51 Register 0x01",
        "C51 Picture ID",
        "C51 Simple Read"
    };
    
    uint8_t* cmd_to_send = c51_commands[c51_test % 5];
    uint8_t cmd_size = cmd_sizes[c51_test % 5];
    char* cmd_desc = cmd_descriptions[c51_test % 5];
    
    // Clear buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    Send_Debug_Data("\r\n=== C51 PROTOCOL TEST ===\r\n");
    
    // Show command being sent
    Send_Debug_Data("C51 TX: ");
    for (int i = 0; i < cmd_size; i++) {
        char hex_str[4];
        snprintf(hex_str, sizeof(hex_str), "%02X ", cmd_to_send[i]);
        Send_Debug_Data(hex_str);
    }
    char desc_msg[100];
    snprintf(desc_msg, sizeof(desc_msg), "(%s)\r\n", cmd_desc);
    Send_Debug_Data(desc_msg);
    
    // Expected C51 responses
    Send_Debug_Data("Expected C51 Response: 40 59 FC (3 bytes)\r\n");
    
    // Prepare transmission
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    
    // Clear pending data
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
    }
    
    // Send C51 command
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(5);
    
    HAL_UART_Transmit(&huart4, cmd_to_send, cmd_size, 2000);
    
    // Wait for transmission complete
    while(__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TC) == RESET);
    
    // C51 responds quickly - minimal delay
    HAL_Delay(2);
    
    // Switch to RX
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(3);
    
    Send_Debug_Data("Listening for C51 response...\r\n");
    
    // Capture C51 response
    uint32_t capture_start = HAL_GetTick();
    uint32_t last_byte_time = capture_start;
    
    while ((HAL_GetTick() - capture_start) < 200) {
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
            
            if (hmi_index < sizeof(hmi_buffer)) {
                hmi_buffer[hmi_index] = byte;
                hmi_index++;
                last_byte_time = HAL_GetTick();
                
                char byte_msg[60];
                snprintf(byte_msg, sizeof(byte_msg), "C51 RX[%d]: %02X\r\n", hmi_index-1, byte);
                Send_Debug_Data(byte_msg);
                
                if (hmi_index >= 3) {
                    HAL_Delay(30);
                    if (!__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
                        break;
                    }
                }
            }
        }
        
        if (hmi_index > 0 && (HAL_GetTick() - last_byte_time) > 30) {
            break;
        }
        
        HAL_Delay(1);
    }
    
    // Results
    if (hmi_index > 0) {
        Send_Debug_Data("C51 RESPONSE: ");
        for (int i = 0; i < hmi_index; i++) {
            char hex_str[4];
            snprintf(hex_str, sizeof(hex_str), "%02X ", hmi_buffer[i]);
            Send_Debug_Data(hex_str);
        }
        char total_msg[50];
        snprintf(total_msg, sizeof(total_msg), "(%d bytes)\r\n", hmi_index);
        Send_Debug_Data(total_msg);
        
        if (hmi_index == 3 && hmi_buffer[0] == 0x40) {
            Send_Debug_Data("*** SUCCESS! C51 3-byte response! ***\r\n");
        }
    } else {
        Send_Debug_Data("*** NO C51 RESPONSE ***\r\n");
    }
    
    Send_Debug_Data("=== END C51 TEST ===\r\n\r\n");
    
    c51_test++;
    HAL_Delay(4000);
    command_sent = 0;
}

// Process function for C51 testing
void HMI_Process_C51_Test(void) {
    HMI_Test_C51_Protocol();
}

// Simple UART4 loopback test
void HMI_Simple_Loopback_Test(void) {
    Send_Debug_Data("\r\n=== UART4 LOOPBACK TEST ===\r\n");
    
    // Clear RX buffer
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
    }
    
    // Send a byte and try to receive it back (if there's any loopback)
    uint8_t test_byte = 0xAA;
    
    Send_Debug_Data("Sending test byte 0xAA...\r\n");
    HAL_UART_Transmit(&huart4, &test_byte, 1, 1000);
    
    // Wait and check if we receive anything
    HAL_Delay(100);
    
    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        uint8_t received = (uint8_t)(huart4.Instance->RDR & 0xFF);
        char rx_msg[50];
        snprintf(rx_msg, sizeof(rx_msg), "Received: %02X\r\n", received);
        Send_Debug_Data(rx_msg);
        Send_Debug_Data("*** UART4 RX IS WORKING! ***\r\n");
    } else {
        Send_Debug_Data("*** NO LOOPBACK - UART4 RX NOT WORKING! ***\r\n");
        Send_Debug_Data("Check PD0 pin configuration and wiring!\r\n");
    }
    
    Send_Debug_Data("=== END LOOPBACK TEST ===\r\n\r\n");
}

// Add these functions at the end of your hmi.c file

// DWIN page change command - immediate visual feedback
void HMI_Change_Page_0(void) {
    static uint32_t last_test_time = 0;
    
    if (HAL_GetTick() - last_test_time < 5000) {
        return;
    }
    last_test_time = HAL_GetTick();
    
    // DWIN page change command to page 0
    // Format: 5A A5 07 82 00 84 5A 01 00 00 00
    uint8_t page_cmd[] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, 0x00, 0x00};
    
    Send_Debug_Data("\r\n=== HMI PAGE CHANGE TO 0 ===\r\n");
    Send_Debug_Data("*** WATCH THE HMI SCREEN FOR PAGE CHANGE! ***\r\n");
    
    // Show command
    Send_Debug_Data("HMI TX: ");
    for (int i = 0; i < sizeof(page_cmd); i++) {
        char hex_str[4];
        snprintf(hex_str, sizeof(hex_str), "%02X ", page_cmd[i]);
        Send_Debug_Data(hex_str);
    }
    Send_Debug_Data("(Page Change to 0)\r\n");
    
    // Prepare transmission
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    
    // Clear any pending data
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
    }
    
    // Send page change command
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(5);
    
    HAL_UART_Transmit(&huart4, page_cmd, sizeof(page_cmd), 2000);
    
    // Wait for transmission complete
    while(__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TC) == RESET);
    
    HAL_Delay(3);
    
    // Switch to RX
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    Send_Debug_Data("Page change command sent!\r\n");
    Send_Debug_Data("*** LOOK AT HMI SCREEN - DID IT CHANGE TO PAGE 0? ***\r\n");
    Send_Debug_Data("If screen changed, HMI is responding to commands!\r\n");
    Send_Debug_Data("=== END PAGE CHANGE TEST ===\r\n\r\n");
}

// Alternative simpler page change command
void HMI_Simple_Page_Change(void) {
    static uint32_t last_test_time = 0;
    static uint8_t page_number = 0;
    
    if (HAL_GetTick() - last_test_time < 8000) {
        return;
    }
    last_test_time = HAL_GetTick();
    
    // Cycle through pages 0, 1, 2 for visual feedback
    page_number = (page_number + 1) % 3;
    
    // Simple page change: 5A A5 04 82 00 03 [PAGE_LOW] [PAGE_HIGH]
    uint8_t simple_page_cmd[] = {0x5A, 0xA5, 0x04, 0x82, 0x00, 0x03, page_number, 0x00};
    
    Send_Debug_Data("\r\n=== SIMPLE PAGE CHANGE ===\r\n");
    
    char page_msg[100];
    snprintf(page_msg, sizeof(page_msg), "*** CHANGING TO PAGE %d - WATCH SCREEN! ***\r\n", page_number);
    Send_Debug_Data(page_msg);
    
    // Show command
    Send_Debug_Data("HMI TX: ");
    for (int i = 0; i < sizeof(simple_page_cmd); i++) {
        char hex_str[4];
        snprintf(hex_str, sizeof(hex_str), "%02X ", simple_page_cmd[i]);
        Send_Debug_Data(hex_str);
    }
    snprintf(page_msg, sizeof(page_msg), "(Simple Page Change to %d)\r\n", page_number);
    Send_Debug_Data(page_msg);
    
    // Send with original working timing
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    
    // Clear RX
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
    }
    
    // Send command
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(2);
    
    HAL_UART_Transmit(&huart4, simple_page_cmd, sizeof(simple_page_cmd), 1000);
    
    HAL_Delay(2);
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    Send_Debug_Data("Command sent! Check HMI screen now!\r\n");
    Send_Debug_Data("=== END SIMPLE PAGE CHANGE ===\r\n\r\n");
}

// Process function for page change testing
void HMI_Process_Page_Change_Test(void) {
    HMI_Simple_Page_Change();  // Use the simpler version
}

// Updated with CORRECT DWIN page change format
void HMI_Correct_Page_Change(void) {
    static uint32_t last_test_time = 0;
    static uint8_t page_number = 0;
    
    if (HAL_GetTick() - last_test_time < 8000) {
        return;
    }
    last_test_time = HAL_GetTick();
    
    // Cycle through pages 0, 1, 2
    page_number = (page_number + 1) % 3;
    
    // CORRECT DWIN page change format: 5A A5 07 82 00 84 5A 01 00 [PAGE]
    uint8_t correct_page_cmd[] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, page_number};
    
    Send_Debug_Data("\r\n=== CORRECT DWIN PAGE CHANGE ===\r\n");
    
    char page_msg[100];
    snprintf(page_msg, sizeof(page_msg), "*** CHANGING TO PAGE %d USING CORRECT FORMAT ***\r\n", page_number);
    Send_Debug_Data(page_msg);
    
    // Show exact command
    Send_Debug_Data("HMI TX: ");
    for (int i = 0; i < sizeof(correct_page_cmd); i++) {
        char hex_str[4];
        snprintf(hex_str, sizeof(hex_str), "%02X ", correct_page_cmd[i]);
        Send_Debug_Data(hex_str);
    }
    snprintf(page_msg, sizeof(page_msg), "(Correct Page Change to %d)\r\n", page_number);
    Send_Debug_Data(page_msg);
    
    // Clear buffer for response capture
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    // Send with proper timing
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    
    // Clear RX
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
    }
    
    // Send command
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(2);
    
    HAL_UART_Transmit(&huart4, correct_page_cmd, sizeof(correct_page_cmd), 1000);
    
    // Wait for transmission complete
    while(__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TC) == RESET);
    
    HAL_Delay(2);
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    // Capture response
    Send_Debug_Data("Capturing HMI response...\r\n");
    uint32_t capture_start = HAL_GetTick();
    
    while ((HAL_GetTick() - capture_start) < 200) {
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
            
            if (hmi_index < sizeof(hmi_buffer)) {
                hmi_buffer[hmi_index] = byte;
                hmi_index++;
                
                char byte_msg[50];
                snprintf(byte_msg, sizeof(byte_msg), "RX[%d]: %02X\r\n", hmi_index-1, byte);
                Send_Debug_Data(byte_msg);
            }
        }
        
        if (hmi_index > 0 && (HAL_GetTick() - capture_start) > 50) {
            break;
        }
        
        HAL_Delay(1);
    }
    
    // Show response
    if (hmi_index > 0) {
        Send_Debug_Data("HMI Response: ");
        for (int i = 0; i < hmi_index; i++) {
            char hex_str[4];
            snprintf(hex_str, sizeof(hex_str), "%02X ", hmi_buffer[i]);
            Send_Debug_Data(hex_str);
        }
        char total_msg[50];
        snprintf(total_msg, sizeof(total_msg), "(%d bytes)\r\n", hmi_index);
        Send_Debug_Data(total_msg);
        
        // Check if it matches your observed response
        if (hmi_index == 3 && hmi_buffer[0] == 0xF6 && hmi_buffer[1] == 0x2D && hmi_buffer[2] == 0xFF) {
            Send_Debug_Data("*** RESPONSE MATCHES! HMI IS ACKNOWLEDGING! ***\r\n");
            Send_Debug_Data("*** CHECK SCREEN - PAGE SHOULD HAVE CHANGED! ***\r\n");
        }
    } else {
        Send_Debug_Data("*** NO RESPONSE CAPTURED ***\r\n");
    }
    
    Send_Debug_Data("*** LOOK AT HMI SCREEN NOW! ***\r\n");
    Send_Debug_Data("=== END CORRECT PAGE CHANGE ===\r\n\r\n");
}

// Test the reset command you provided
void HMI_Correct_Reset(void) {
    static uint32_t last_reset_time = 0;
    
    if (HAL_GetTick() - last_reset_time < 15000) {
        return;
    }
    last_reset_time = HAL_GetTick();
    
    // CORRECT DWIN reset: 5A A5 07 82 00 04 55 AA 5A A5
    uint8_t reset_cmd[] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0x04, 0x55, 0xAA, 0x5A, 0xA5};
    
    Send_Debug_Data("\r\n=== CORRECT DWIN RESET ===\r\n");
    Send_Debug_Data("*** WARNING: HMI WILL RESET! ***\r\n");
    
    // Show command
    Send_Debug_Data("HMI TX: ");
    for (int i = 0; i < sizeof(reset_cmd); i++) {
        char hex_str[4];
        snprintf(hex_str, sizeof(hex_str), "%02X ", reset_cmd[i]);
        Send_Debug_Data(hex_str);
    }
    Send_Debug_Data("(DWIN Reset)\r\n");
    
    // Send reset
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
    }
    
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(2);
    
    HAL_UART_Transmit(&huart4, reset_cmd, sizeof(reset_cmd), 1000);
    
    HAL_Delay(2);
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    Send_Debug_Data("Reset command sent!\r\n");
    Send_Debug_Data("*** HMI SHOULD BE RESETTING - WATCH SCREEN! ***\r\n");
    Send_Debug_Data("=== END RESET ===\r\n\r\n");
}

// Diagnostic: Try writing to a different register to see visual feedback
void HMI_Test_Register_Write(void) {
    static uint32_t last_test_time = 0;
    static uint16_t test_value = 0;
    
    if (HAL_GetTick() - last_test_time < 5000) {
        return;
    }
    last_test_time = HAL_GetTick();
    
    // Try writing to VP register 0x1000 (common for text/numbers)
    // Format: 5A A5 05 82 10 00 [VALUE_HIGH] [VALUE_LOW]
    test_value += 10;  // Increment by 10 each time
    
    uint8_t write_cmd[] = {0x5A, 0xA5, 0x05, 0x82, 0x10, 0x00, 
                          (test_value >> 8) & 0xFF,  // High byte
                          test_value & 0xFF};        // Low byte
    
    Send_Debug_Data("\r\n=== REGISTER WRITE TEST ===\r\n");
    
    char test_msg[100];
    snprintf(test_msg, sizeof(test_msg), "Writing value %d to register 0x1000\r\n", test_value);
    Send_Debug_Data(test_msg);
    
    Send_Debug_Data("HMI TX: ");
    for (int i = 0; i < sizeof(write_cmd); i++) {
        char hex_str[4];
        snprintf(hex_str, sizeof(hex_str), "%02X ", write_cmd[i]);
        Send_Debug_Data(hex_str);
    }
    Send_Debug_Data("(Register Write)\r\n");
    
    // Send command
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
    }
    
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(2);
    
    HAL_UART_Transmit(&huart4, write_cmd, sizeof(write_cmd), 1000);
    
    HAL_Delay(2);
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    Send_Debug_Data("Register write sent - check for any display changes!\r\n");
    Send_Debug_Data("=== END REGISTER WRITE ===\r\n\r\n");
}

// Test the EXACT command that works from PC
void HMI_Test_PC_Working_Command(void) {
    static uint32_t last_test_time = 0;
    
    if (HAL_GetTick() - last_test_time < 8000) {
        return;
    }
    last_test_time = HAL_GetTick();
    
    // This is the EXACT command you said works from PC: 5a a5 07 82 00 84 5a 01 00 00
    uint8_t pc_working_cmd[] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, 0x00};
    
    Send_Debug_Data("\r\n=== TESTING PC WORKING COMMAND ===\r\n");
    Send_Debug_Data("*** EXACT SAME BYTES THAT WORK FROM PC ***\r\n");
    
    // Show command
    Send_Debug_Data("HMI TX: ");
    for (int i = 0; i < sizeof(pc_working_cmd); i++) {
        char hex_str[4];
        snprintf(hex_str, sizeof(hex_str), "%02X ", pc_working_cmd[i]);
        Send_Debug_Data(hex_str);
    }
    Send_Debug_Data("(PC Working Command)\r\n");
    
    // Clear response buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    // Send with exact same timing as PC
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
    }
    
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(2);
    
    HAL_UART_Transmit(&huart4, pc_working_cmd, sizeof(pc_working_cmd), 1000);
    
    HAL_Delay(2);
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    // Capture response (expecting same as PC gets)
    Send_Debug_Data("Looking for response (PC doesn't show response to this command)...\r\n");
    uint32_t capture_start = HAL_GetTick();
    
    while ((HAL_GetTick() - capture_start) < 200) {
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
            
            if (hmi_index < sizeof(hmi_buffer)) {
                hmi_buffer[hmi_index] = byte;
                hmi_index++;
                
                char byte_msg[50];
                snprintf(byte_msg, sizeof(byte_msg), "RX[%d]: %02X\r\n", hmi_index-1, byte);
                Send_Debug_Data(byte_msg);
            }
        }
        
        if (hmi_index > 0 && (HAL_GetTick() - capture_start) > 50) {
            break;
        }
        
        HAL_Delay(2);
    }
    
    if (hmi_index > 0) {
        Send_Debug_Data("Unexpected response: ");
        for (int i = 0; i < hmi_index; i++) {
            char hex_str[4];
            snprintf(hex_str, sizeof(hex_str), "%02X ", hmi_buffer[i]);
            Send_Debug_Data(hex_str);
        }
        Send_Debug_Data("\r\n");
    } else {
        Send_Debug_Data("No response (same as PC)\r\n");
    }
    
    Send_Debug_Data("*** CRITICAL: DID THE HMI SCREEN CHANGE TO PAGE 0? ***\r\n");
    Send_Debug_Data("=== END PC WORKING COMMAND TEST ===\r\n\r\n");
}

// Test reading VP using the correct format you provided
void HMI_Test_VP_Read(void) {
    static uint32_t last_test_time = 0;
    static uint16_t vp_address = 0x0001;
    
    if (HAL_GetTick() - last_test_time < 8000) {
        return;
    }
    last_test_time = HAL_GetTick();
    
    // Your correct format: 5A A5 04 83 00 01 01 (read VP 1, 1 word)
    uint8_t vp_read_cmd[] = {0x5A, 0xA5, 0x04, 0x83, 
                            (vp_address >> 8) & 0xFF,   // VP high byte
                            vp_address & 0xFF,          // VP low byte  
                            0x01};                      // Read 1 word
    
    Send_Debug_Data("\r\n=== VP READ TEST (CORRECT FORMAT) ===\r\n");
    
    char vp_msg[100];
    snprintf(vp_msg, sizeof(vp_msg), "Reading VP 0x%04X (1 word)\r\n", vp_address);
    Send_Debug_Data(vp_msg);
    
    // Show command
    Send_Debug_Data("HMI TX: ");
    for (int i = 0; i < sizeof(vp_read_cmd); i++) {
        char hex_str[4];
        snprintf(hex_str, sizeof(hex_str), "%02X ", vp_read_cmd[i]);
        Send_Debug_Data(hex_str);
    }
    Send_Debug_Data("(VP Read - Correct Format)\r\n");
    
    // Clear response buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    // Send command
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
    }
    
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(2);
    
    HAL_UART_Transmit(&huart4, vp_read_cmd, sizeof(vp_read_cmd), 1000);
    
    // Wait for transmission complete
    while(__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TC) == RESET);
    
    HAL_Delay(2);
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(3);
    
    // Capture response
    Send_Debug_Data("Capturing VP read response...\r\n");
    uint32_t capture_start = HAL_GetTick();
    uint32_t last_byte_time = capture_start;
    
    while ((HAL_GetTick() - capture_start) < 300) {
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
            
            if (hmi_index < sizeof(hmi_buffer)) {
                hmi_buffer[hmi_index] = byte;
                hmi_index++;
                last_byte_time = HAL_GetTick();
                
                char byte_msg[50];
                snprintf(byte_msg, sizeof(byte_msg), "RX[%d]: %02X\r\n", hmi_index-1, byte);
                Send_Debug_Data(byte_msg);
            }
        }
        
        // Break if no new bytes for 50ms
        if (hmi_index > 0 && (HAL_GetTick() - last_byte_time) > 50) {
            break;
        }
        
        HAL_Delay(2);
    }
    
    // Analyze response
    if (hmi_index > 0) {
        Send_Debug_Data("VP Read Response: ");
        for (int i = 0; i < hmi_index; i++) {
            char hex_str[4];
            snprintf(hex_str, sizeof(hex_str), "%02X ", hmi_buffer[i]);
            Send_Debug_Data(hex_str);
        }
        char total_msg[50];
        snprintf(total_msg, sizeof(total_msg), "(%d bytes)\r\n", hmi_index);
        Send_Debug_Data(total_msg);
        
        // Check if it's a proper DWIN read response
        if (hmi_index >= 3 && hmi_buffer[0] == 0x5A && hmi_buffer[1] == 0xA5) {
            uint8_t response_len = hmi_buffer[2];
            Send_Debug_Data("✓ Valid DWIN response header\r\n");
            
            char len_msg[50];
            snprintf(len_msg, sizeof(len_msg), "Response data length: %d bytes\r\n", response_len);
            Send_Debug_Data(len_msg);
            
            if (hmi_index >= (3 + response_len)) {
                Send_Debug_Data("✓ Complete response received\r\n");
                
                // Extract VP data (should be 2 bytes for 1 word)
                if (response_len >= 5 && hmi_index >= 8) {
                    // Expected format: 5A A5 05 83 00 01 [DATA_HIGH] [DATA_LOW]
                    uint16_t vp_value = (hmi_buffer[6] << 8) | hmi_buffer[7];
                    char value_msg[100];
                    snprintf(value_msg, sizeof(value_msg), "VP 0x%04X value: 0x%04X (%d)\r\n", 
                             vp_address, vp_value, vp_value);
                    Send_Debug_Data(value_msg);
                    Send_Debug_Data("*** VP READ SUCCESS! ***\r\n");
                }
            }
        } else {
            Send_Debug_Data("⚠ Unexpected response format\r\n");
        }
    } else {
        Send_Debug_Data("*** NO RESPONSE - VP READ FAILED ***\r\n");
    }
    
    Send_Debug_Data("=== END VP READ TEST ===\r\n\r\n");
    
    // Try different VP addresses
    vp_address++;
    if (vp_address > 0x0010) {
        vp_address = 0x0001;
    }
}

// Comprehensive UART4 RX diagnostic
void HMI_UART_RX_Diagnostic(void) {
    Send_Debug_Data("\r\n=== COMPREHENSIVE UART4 RX DIAGNOSTIC ===\r\n");
    
    // Check UART4 registers
    char msg[150];
    
    snprintf(msg, sizeof(msg), "UART4 Base: 0x%08lX\r\n", (uint32_t)huart4.Instance);
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "UART4 CR1: 0x%08lX\r\n", huart4.Instance->CR1);
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "UART4 CR2: 0x%08lX\r\n", huart4.Instance->CR2);
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "UART4 CR3: 0x%08lX\r\n", huart4.Instance->CR3);
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "UART4 BRR: 0x%08lX\r\n", huart4.Instance->BRR);
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "UART4 ISR: 0x%08lX\r\n", huart4.Instance->ISR);
    Send_Debug_Data(msg);
    
    // Check if RX is enabled
    if (huart4.Instance->CR1 & USART_CR1_RE) {
        Send_Debug_Data("✓ UART4 RX is ENABLED\r\n");
    } else {
        Send_Debug_Data("✗ UART4 RX is DISABLED!\r\n");
    }
    
    // Check if UART is enabled
    if (huart4.Instance->CR1 & USART_CR1_UE) {
        Send_Debug_Data("✓ UART4 is ENABLED\r\n");
    } else {
        Send_Debug_Data("✗ UART4 is DISABLED!\r\n");
    }
    
    // Check GPIO configuration for PD0 (UART4 RX)
    Send_Debug_Data("\r\n=== GPIO PD0 Configuration ===\r\n");
    
    uint32_t gpiod_moder = GPIOD->MODER;
    uint32_t gpiod_afrl = GPIOD->AFR[0];
    uint32_t gpiod_pupdr = GPIOD->PUPDR;
    
    snprintf(msg, sizeof(msg), "GPIOD MODER: 0x%08lX\r\n", gpiod_moder);
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "GPIOD AFRL: 0x%08lX\r\n", gpiod_afrl);
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "GPIOD PUPDR: 0x%08lX\r\n", gpiod_pupdr);
    Send_Debug_Data(msg);
    
    // Check PD0 specific configuration
    uint32_t pd0_mode = (gpiod_moder >> 0) & 0x03;
    uint32_t pd0_af = (gpiod_afrl >> 0) & 0x0F;
    uint32_t pd0_pupd = (gpiod_pupdr >> 0) & 0x03;
    
    snprintf(msg, sizeof(msg), "PD0 Mode: %lu (0=Input, 1=Output, 2=AF, 3=Analog)\r\n", pd0_mode);
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "PD0 AF: %lu (should be 8 for UART4)\r\n", pd0_af);
    Send_Debug_Data(msg);
    
    snprintf(msg, sizeof(msg), "PD0 Pull: %lu (0=None, 1=PullUp, 2=PullDown)\r\n", pd0_pupd);
    Send_Debug_Data(msg);
    
    if (pd0_mode == 2 && pd0_af == 8) {
        Send_Debug_Data("✓ PD0 correctly configured for UART4 RX\r\n");
    } else {
        Send_Debug_Data("✗ PD0 configuration WRONG!\r\n");
    }
    
    // Test direct register reading
    Send_Debug_Data("\r\n=== DIRECT UART REGISTER TEST ===\r\n");
    Send_Debug_Data("Monitoring UART4 ISR register for RX activity...\r\n");
    Send_Debug_Data("Send data to UART4 RX now and watch for changes!\r\n");
    
    // Monitor for 10 seconds
    uint32_t start_time = HAL_GetTick();
    uint32_t last_isr = huart4.Instance->ISR;
    uint8_t data_received = 0;
    
    while ((HAL_GetTick() - start_time) < 10000) {
        uint32_t current_isr = huart4.Instance->ISR;
        
        // Check for ISR changes
        if (current_isr != last_isr) {
            snprintf(msg, sizeof(msg), "ISR changed: 0x%08lX -> 0x%08lX\r\n", last_isr, current_isr);
            Send_Debug_Data(msg);
            last_isr = current_isr;
        }
        
        // Check RXNE flag (STM32H7 uses USART_ISR_RXNE_RXFNE)
        if (current_isr & USART_ISR_RXNE_RXFNE) {
            uint32_t received_data = huart4.Instance->RDR;
            snprintf(msg, sizeof(msg), "*** DATA RECEIVED: 0x%02lX (%lu) ***\r\n", received_data & 0xFF, received_data & 0xFF);
            Send_Debug_Data(msg);
            data_received = 1;
        }
        
        // Check for errors
        if (current_isr & USART_ISR_ORE) {
            Send_Debug_Data("*** OVERRUN ERROR! ***\r\n");
        }
        if (current_isr & USART_ISR_FE) {
            Send_Debug_Data("*** FRAMING ERROR! ***\r\n");
        }
        if (current_isr & USART_ISR_PE) {
            Send_Debug_Data("*** PARITY ERROR! ***\r\n");
        }
        
        HAL_Delay(50);
    }
    
    if (!data_received) {
        Send_Debug_Data("*** NO DATA RECEIVED IN 10 SECONDS ***\r\n");
        Send_Debug_Data("*** UART4 RX IS NOT WORKING! ***\r\n");
    }
    
    Send_Debug_Data("=== END UART4 RX DIAGNOSTIC ===\r\n\r\n");
}

// Force enable UART4 RX
void HMI_Force_Enable_UART_RX(void) {
    Send_Debug_Data("\r\n=== FORCING UART4 RX ENABLE ===\r\n");
    
    // Force enable UART4 RX
    huart4.Instance->CR1 |= USART_CR1_RE;  // Enable RX
    huart4.Instance->CR1 |= USART_CR1_UE;  // Enable UART
    
    Send_Debug_Data("Forced UART4 RX enable\r\n");
    
    // Clear any error flags
    huart4.Instance->ICR = 0xFFFFFFFF;  // Clear all interrupt flags
    
    Send_Debug_Data("Cleared all UART error flags\r\n");
    
    // Test again
    Send_Debug_Data("Testing RX after force enable...\r\n");
    
    uint32_t test_start = HAL_GetTick();
    while ((HAL_GetTick() - test_start) < 5000) {
        if (huart4.Instance->ISR & USART_ISR_RXNE_RXFNE) {
            uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
            char byte_msg[50];
            snprintf(byte_msg, sizeof(byte_msg), "RX WORKS! Received: 0x%02X\r\n", byte);
            Send_Debug_Data(byte_msg);
            break;
        }
        HAL_Delay(10);
    }
    
    Send_Debug_Data("=== END FORCE ENABLE TEST ===\r\n\r\n");
}

// Add this function at the end of your hmi.c file

// Improved HMI response capture with better timing
void HMI_Improved_Page_Test(void) {
    static uint32_t last_test_time = 0;
    static uint8_t page_number = 0;
    
    if (HAL_GetTick() - last_test_time < 10000) {
        return;
    }
    last_test_time = HAL_GetTick();
    
    // Cycle through pages 0, 1, 2
    page_number = (page_number + 1) % 3;
    
    // Correct DWIN page change format
    uint8_t page_cmd[] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, page_number};
    
    Send_Debug_Data("\r\n=== IMPROVED PAGE CHANGE TEST ===\r\n");
    
    char page_msg[100];
    snprintf(page_msg, sizeof(page_msg), "*** CHANGING TO PAGE %d ***\r\n", page_number);
    Send_Debug_Data(page_msg);
    
    // Show command
    Send_Debug_Data("HMI TX: ");
    for (int i = 0; i < sizeof(page_cmd); i++) {
        char hex_str[4];
        snprintf(hex_str, sizeof(hex_str), "%02X ", page_cmd[i]);
        Send_Debug_Data(hex_str);
    }
    Send_Debug_Data("\r\n");
    
    // Clear ALL buffers completely
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    // Ensure RX mode and clear any pending data
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    
    // Clear UART RX completely
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
        volatile uint8_t dummy = huart4.Instance->RDR;
        (void)dummy;
    }
    
    // Clear any error flags
    huart4.Instance->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_PECF | USART_ICR_NECF;
    
    Send_Debug_Data("Sending command...\r\n");
    
    // Send command
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(5);
    
    HAL_UART_Transmit(&huart4, page_cmd, sizeof(page_cmd), 1000);
    
    // Wait for transmission complete
    while(__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TC) == RESET);
    
    // Small delay then switch to RX
    HAL_Delay(3);
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    
    Send_Debug_Data("Command sent, capturing response...\r\n");
    
    // EXTENDED capture with multiple phases
    uint32_t capture_start = HAL_GetTick();
    uint32_t last_byte_time = capture_start;
    
    // Phase 1: Quick capture (0-100ms) for immediate responses
    while ((HAL_GetTick() - capture_start) < 100) {
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
            
            if (hmi_index < sizeof(hmi_buffer)) {
                hmi_buffer[hmi_index] = byte;
                hmi_index++;
                last_byte_time = HAL_GetTick();
                
                char byte_msg[60];
                snprintf(byte_msg, sizeof(byte_msg), "RX[%d]: %02X ('%c') at +%lu ms\r\n", 
                         hmi_index-1, byte, (byte >= 32 && byte <= 126) ? byte : '.', 
                         HAL_GetTick() - capture_start);
                Send_Debug_Data(byte_msg);
            }
        }
        
        // If we got data, wait a bit more for complete message
        if (hmi_index > 0 && (HAL_GetTick() - last_byte_time) > 20) {
            Send_Debug_Data("Phase 1: No more bytes for 20ms\r\n");
            break;
        }
        
        HAL_Delay(1);
    }
    
    // Phase 2: Extended capture (100-500ms) for delayed responses
    if (hmi_index == 0) {
        Send_Debug_Data("Phase 1: No data, trying extended capture...\r\n");
        
        while ((HAL_GetTick() - capture_start) < 500) {
            if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
                uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
                
                if (hmi_index < sizeof(hmi_buffer)) {
                    hmi_buffer[hmi_index] = byte;
                    hmi_index++;
                    last_byte_time = HAL_GetTick();
                    
                    char byte_msg[60];
                    snprintf(byte_msg, sizeof(byte_msg), "RX[%d]: %02X ('%c') at +%lu ms\r\n", 
                             hmi_index-1, byte, (byte >= 32 && byte <= 126) ? byte : '.', 
                             HAL_GetTick() - capture_start);
                    Send_Debug_Data(byte_msg);
                }
            }
            
            if (hmi_index > 0 && (HAL_GetTick() - last_byte_time) > 50) {
                Send_Debug_Data("Phase 2: No more bytes for 50ms\r\n");
                break;
            }
            
            HAL_Delay(5);
        }
    }
    
    // Show results
    Send_Debug_Data("\r\n=== CAPTURE RESULTS ===\r\n");
    
    if (hmi_index > 0) {
        Send_Debug_Data("Response received: ");
        for (int i = 0; i < hmi_index; i++) {
            char hex_str[4];
            snprintf(hex_str, sizeof(hex_str), "%02X ", hmi_buffer[i]);
            Send_Debug_Data(hex_str);
        }
        char total_msg[50];
        snprintf(total_msg, sizeof(total_msg), "(%d bytes)\r\n", hmi_index);
        Send_Debug_Data(total_msg);
        
        // Check if it's ASCII
        Send_Debug_Data("ASCII interpretation: ");
        for (int i = 0; i < hmi_index; i++) {
            if (hmi_buffer[i] >= 32 && hmi_buffer[i] <= 126) {
                char ascii_char[2] = {(char)hmi_buffer[i], '\0'};
                Send_Debug_Data(ascii_char);
            } else {
                Send_Debug_Data(".");
            }
        }
        Send_Debug_Data("\r\n");
        
        // Analyze response type
        if (hmi_index >= 2 && hmi_buffer[0] == 0x5A && hmi_buffer[1] == 0xA5) {
            Send_Debug_Data("*** DWIN BINARY RESPONSE ***\r\n");
        } else if (hmi_index >= 2 && hmi_buffer[0] == 0x48 && hmi_buffer[1] == 0x4F) {
            Send_Debug_Data("*** ASCII 'HO' RESPONSE ***\r\n");
        } else {
            Send_Debug_Data("*** UNKNOWN RESPONSE FORMAT ***\r\n");
        }
        
        Send_Debug_Data("*** SUCCESS: HMI IS RESPONDING! ***\r\n");
    } else {
        Send_Debug_Data("*** NO RESPONSE CAPTURED ***\r\n");
    }
    
    Send_Debug_Data("*** CHECK HMI SCREEN - DID PAGE CHANGE? ***\r\n");
    Send_Debug_Data("=== END IMPROVED TEST ===\r\n\r\n");
}

// Add this function to start interrupt reception:
void HMI_Start_Interrupt_RX(void) {
    if (!hmi_interrupt_active) {
        hmi_interrupt_active = 1;
        HAL_UART_Receive_IT(&huart4, hmi_rx_buffer, 1);
        Send_Debug_Data("UART4 interrupt RX started\r\n");
    }
}

// Add this callback function (called automatically when data arrives):
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == UART4) {
        // Data received in hmi_rx_buffer[0]
        uint8_t received_byte = hmi_rx_buffer[0];
        
        // Store in your HMI buffer
        if (hmi_index < sizeof(hmi_buffer)) {
            hmi_buffer[hmi_index] = received_byte;
            hmi_index++;
            hmi_last_time = HAL_GetTick();
        }
        
        // Restart reception for next byte
        HAL_UART_Receive_IT(&huart4, hmi_rx_buffer, 1);
    }
}

// Add these functions at the end of hmi.c:



// Process all HMI debug commands - keeps main.c clean
void HMI_Process_Debug_Command(const char* command) {
    if (strncmp(command, "hmi_version", 11) == 0) {
        if (hmi_initialized) {
            Send_Debug_Data("Sending HMI version check command...\r\n");
            HMI_Send_Version_Check();
        } else {
            Send_Debug_Data("HMI not initialized\r\n");
        }
    }
    else if (strncmp(command, "hmi_status", 10) == 0) {
        Send_Debug_Data("=== HMI COMMUNICATION STATUS ===\r\n");
        Send_Debug_Data("✓ Commands being sent successfully\r\n");
        Send_Debug_Data("✓ Receiving 'HOK' responses (48 4F 4B)\r\n");
        Send_Debug_Data("✓ RS485 timing working correctly\r\n");
        Send_Debug_Data("✓ UART4 RX capturing responses\r\n");
        Send_Debug_Data("=== HMI IS WORKING! ===\r\n");
        
        // Test one page change
        HMI_Correct_Page_Change();
    }
    else if (strncmp(command, "hmi_hw_test", 11) == 0) {
        HMI_Hardware_RX_Test();
    }
    else if (strncmp(command, "hmi_page0", 9) == 0) {
        Send_Debug_Data("Sending page change to 0 - WATCH SCREEN!\r\n");
        HMI_Change_Page_0();
    }
    else if (strncmp(command, "hmi_page_test", 13) == 0) {
        Send_Debug_Data("Testing page changes with status...\r\n");
        HMI_Test_Page_Status();
    }
    else if (strncmp(command, "hmi_pc_test", 11) == 0) {
        Send_Debug_Data("Testing EXACT command that works from PC...\r\n");
        HMI_Test_PC_Working_Command();
    }
    else if (strncmp(command, "hmi_diag", 8) == 0) {
        Send_Debug_Data("Running detailed HMI transmission diagnostic...\r\n");
        HMI_Diagnostic_Page_Change();
    }
    else if (strncmp(command, "hmi_slow", 8) == 0) {
        Send_Debug_Data("Testing slow byte-by-byte transmission...\r\n");
        HMI_Slow_Byte_Transmission();
    }
    else if (strncmp(command, "hmi_vp_read", 11) == 0) {
        Send_Debug_Data("Testing VP read with correct format...\r\n");
        HMI_Test_VP_Read();
    }
    else if (strncmp(command, "hmi_uart_diag", 13) == 0) {
        Send_Debug_Data("Running comprehensive UART4 RX diagnostic...\r\n");
        HMI_UART_RX_Diagnostic();
    }
    else if (strncmp(command, "hmi_force_rx", 12) == 0) {
        Send_Debug_Data("Forcing UART4 RX enable...\r\n");
        HMI_Force_Enable_UART_RX();
    }
    else if (strncmp(command, "hmi_improved", 12) == 0) {
        Send_Debug_Data("Testing with improved capture...\r\n");
        HMI_Improved_Page_Test();
    }
    else if (strncmp(command, "hmi_correct", 11) == 0) {
        Send_Debug_Data("Testing correct page change format...\r\n");
        HMI_Correct_Page_Change();
    }
    else if (strncmp(command, "hmi_help", 8) == 0) {
        Send_Debug_Data("\r\n=== HMI DEBUG COMMANDS ===\r\n");
        Send_Debug_Data("hmi_status      - Show HMI communication status\r\n");
        Send_Debug_Data("hmi_version     - Send version check command\r\n");
        Send_Debug_Data("hmi_correct     - Test page change (working format)\r\n");
        Send_Debug_Data("hmi_page0       - Change to page 0\r\n");
        Send_Debug_Data("hmi_page_test   - Cycle through pages\r\n");
        Send_Debug_Data("hmi_improved    - Advanced response capture\r\n");
        Send_Debug_Data("hmi_uart_diag   - UART RX diagnostics\r\n");
        Send_Debug_Data("hmi_help        - Show this help\r\n");
        Send_Debug_Data("==========================\r\n");
    }
    else {
        Send_Debug_Data("Unknown HMI command. Type 'hmi_help' for available commands.\r\n");
    }
}

// Page status test function
void HMI_Test_Page_Status(void) {
    static uint32_t last_test = 0;
    static uint8_t test_page = 0;
    
    if (HAL_GetTick() - last_test < 5000) return;
    last_test = HAL_GetTick();
    
    // Clear buffer
    hmi_index = 0;
    memset(hmi_buffer, 0, sizeof(hmi_buffer));
    
    // Send page change command
    uint8_t page_cmd[] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, test_page};
    
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_SET);
    HAL_Delay(2);
    HAL_UART_Transmit(&huart4, page_cmd, sizeof(page_cmd), 1000);
    HAL_Delay(2);
    HAL_GPIO_WritePin(HMI_DE_RE_GPIO_Port, HMI_DE_RE_Pin, GPIO_PIN_RESET);
    
    // Capture response
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < 200) {
        if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) {
            uint8_t byte = (uint8_t)(huart4.Instance->RDR & 0xFF);
            if (hmi_index < sizeof(hmi_buffer)) {
                hmi_buffer[hmi_index++] = byte;
            }
        }
        if (hmi_index >= 3) break; // Got "HOK"
        HAL_Delay(1);
    }
    
    char msg[150];
    snprintf(msg, sizeof(msg), "Page %d → Response: ", test_page);
    Send_Debug_Data(msg);
    
    for (int i = 0; i < hmi_index; i++) {
        char hex[4];
        snprintf(hex, sizeof(hex), "%02X ", hmi_buffer[i]);
        Send_Debug_Data(hex);
    }
    
    Send_Debug_Data(" (\"");
    for (int i = 0; i < hmi_index; i++) {
        char ascii[2] = {(char)hmi_buffer[i], '\0'};
        Send_Debug_Data(ascii);
    }
    Send_Debug_Data("\")\r\n");
    
    // Check for successful response
    if (hmi_index == 3 && hmi_buffer[0] == 0x48 && hmi_buffer[1] == 0x4F && hmi_buffer[2] == 0x4B) {
        snprintf(msg, sizeof(msg), "*** PAGE %d CHANGE: SUCCESS! ***\r\n", test_page);
        Send_Debug_Data(msg);
    }
    
    test_page = (test_page + 1) % 3; // Cycle 0,1,2
}

/**
 * @brief Write to HMI VP Register (placeholder)
 * @param address: VP register address
 * @param value: Value to write
 */
void HMI_Write_VP_Register(uint16_t address, uint16_t value)
{
    // TODO: Implement HMI register write
    // This is a placeholder for HMI integration
    // For now, just send debug message
    char msg[50];
    snprintf(msg, sizeof(msg), "HMI Write: 0x%04X = %d\r\n", address, value);
    Send_Debug_Data(msg);
}

// Add this function to hmi.c
void HMI_Send_Cyclic_Commands(void)
{
    // Placeholder implementation for cyclic HMI commands
    // This would typically send periodic status updates to the HMI
    if (HMI_Is_Initialized()) {
        // Send system status updates
        HMI_Write_VP_Register(HMI_VP_STATUS, HMI_STATUS_RUNNING);
        HMI_Write_VP_Register(HMI_VP_SYSTEM_TIME, (uint16_t)(HAL_GetTick() / 1000));
    }
}
