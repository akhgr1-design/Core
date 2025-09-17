/**
 * @file    w5500_tcp_server.h
 * @brief   W5500 TCP Server Implementation for Industrial Chiller Control
 * @author  Industrial Chiller Control System
 * @version 1.0
 * @date    2025
 *
 * @description
 * TCP Server module for remote monitoring and control of chiller system.
 * Provides real-time sensor data streaming, relay status monitoring,
 * remote configuration, and system diagnostics over Ethernet.
 */

#ifndef W5500_TCP_SERVER_H
#define W5500_TCP_SERVER_H

#include "main.h"
#include "w5500_socket.h"
#include <stdint.h>

/* TCP Server Configuration */
#define TCP_SERVER_PORT             8080    // Default listening port
#define TCP_SERVER_SOCKET           1       // Use Socket 1 for TCP server
#define TCP_MAX_CLIENTS             4       // Maximum concurrent clients
#define TCP_BUFFER_SIZE             512     // TX/RX buffer size
#define TCP_CLIENT_TIMEOUT          30000   // Client timeout in ms
#define TCP_KEEPALIVE_INTERVAL      10000   // Keep-alive interval in ms

/* TCP Server States */
typedef enum {
    TCP_STATE_IDLE = 0,
    TCP_STATE_LISTENING,
    TCP_STATE_CONNECTED,
    TCP_STATE_DATA_READY,
    TCP_STATE_SENDING,
    TCP_STATE_DISCONNECTING,
    TCP_STATE_ERROR
} tcp_server_state_t;

/* Client Connection Structure */
typedef struct {
    uint8_t client_id;
    uint8_t socket_id;
    tcp_server_state_t state;
    uint32_t connect_time;
    uint32_t last_activity;
    uint8_t remote_ip[4];
    uint16_t remote_port;
    uint32_t bytes_sent;
    uint32_t bytes_received;
    uint8_t error_count;
} tcp_client_t;

/* TCP Server Statistics */
typedef struct {
    uint32_t total_connections;
    uint32_t active_connections;
    uint32_t total_bytes_sent;
    uint32_t total_bytes_received;
    uint32_t connection_errors;
    uint32_t uptime_seconds;
    uint32_t last_client_connect;
} tcp_server_stats_t;

/* TCP Command Types (for remote control) */
typedef enum {
    TCP_CMD_GET_STATUS = 0x01,
    TCP_CMD_GET_SENSORS = 0x02,
    TCP_CMD_GET_RELAYS = 0x03,
    TCP_CMD_SET_RELAY = 0x04,
    TCP_CMD_GET_CONFIG = 0x05,
    TCP_CMD_SET_CONFIG = 0x06,
    TCP_CMD_GET_STATS = 0x07,
    TCP_CMD_SYSTEM_INFO = 0x08,
    TCP_CMD_PING = 0xFF
} tcp_command_t;

/* Function Declarations */

/**
 * @brief Initialize TCP server on specified port
 * @param port TCP port to listen on (default: 8080)
 * @return 1 if successful, 0 if failed
 */
uint8_t TCP_Server_Init(uint16_t port);

/**
 * @brief Start TCP server listening for connections
 * @return 1 if successful, 0 if failed
 */
uint8_t TCP_Server_Start(void);

/**
 * @brief Stop TCP server and close all connections
 * @return 1 if successful, 0 if failed
 */
uint8_t TCP_Server_Stop(void);

/**
 * @brief Main TCP server processing function (call from main loop)
 * Handles connection management, data processing, and client communication
 */
void TCP_Server_Process(void);

/**
 * @brief Send data to specific client
 * @param client_id Client ID (0-3)
 * @param data Pointer to data buffer
 * @param length Data length in bytes
 * @return Number of bytes sent, 0 if failed
 */
uint16_t TCP_Server_Send(uint8_t client_id, uint8_t *data, uint16_t length);

/**
 * @brief Send data to all connected clients
 * @param data Pointer to data buffer
 * @param length Data length in bytes
 * @return Number of clients data was sent to
 */
uint8_t TCP_Server_Broadcast(uint8_t *data, uint16_t length);

/**
 * @brief Check if client is connected
 * @param client_id Client ID (0-3)
 * @return 1 if connected, 0 if not connected
 */
uint8_t TCP_Server_IsClientConnected(uint8_t client_id);

/**
 * @brief Get client connection information
 * @param client_id Client ID (0-3)
 * @param client Pointer to client structure to fill
 * @return 1 if successful, 0 if failed
 */
uint8_t TCP_Server_GetClientInfo(uint8_t client_id, tcp_client_t *client);

/**
 * @brief Disconnect specific client
 * @param client_id Client ID (0-3)
 * @return 1 if successful, 0 if failed
 */
uint8_t TCP_Server_DisconnectClient(uint8_t client_id);

/**
 * @brief Get server statistics
 * @param stats Pointer to statistics structure to fill
 * @return 1 if successful, 0 if failed
 */
uint8_t TCP_Server_GetStats(tcp_server_stats_t *stats);

/**
 * @brief Reset server statistics
 */
void TCP_Server_ResetStats(void);

/**
 * @brief Get current server state
 * @return Current server state
 */
tcp_server_state_t TCP_Server_GetState(void);

/**
 * @brief Send sensor data to connected clients
 * @param temp_sensors Array of temperature values (12 sensors)
 * @param press_sensors Array of pressure values (12 sensors)
 * @return Number of clients data was sent to
 */
uint8_t TCP_Server_SendSensorData(uint16_t *temp_sensors, uint16_t *press_sensors);

/**
 * @brief Send relay status to connected clients
 * @param relay_states 12-bit relay status (bit per relay)
 * @return Number of clients data was sent to
 */
uint8_t TCP_Server_SendRelayStatus(uint16_t relay_states);

/**
 * @brief Send system status message to connected clients
 * @param status_msg Status message string
 * @return Number of clients message was sent to
 */
uint8_t TCP_Server_SendSystemStatus(const char *status_msg);

/**
 * @brief Process incoming command from client
 * @param client_id Client ID that sent command
 * @param command Command data buffer
 * @param length Command length
 * @return 1 if command processed successfully, 0 if failed
 */
uint8_t TCP_Server_ProcessCommand(uint8_t client_id, uint8_t *command, uint16_t length);

/**
 * @brief Enable/disable automatic sensor data streaming
 * @param enable 1 to enable streaming, 0 to disable
 * @param interval_ms Streaming interval in milliseconds
 */
void TCP_Server_SetDataStreaming(uint8_t enable, uint32_t interval_ms);

/**
 * @brief Debug function to print server status
 */
void TCP_Server_Debug(void);

/* Callback function types for application integration */
typedef void (*tcp_client_connect_callback_t)(uint8_t client_id, uint8_t *remote_ip);
typedef void (*tcp_client_disconnect_callback_t)(uint8_t client_id);
typedef void (*tcp_data_received_callback_t)(uint8_t client_id, uint8_t *data, uint16_t length);

/**
 * @brief Register callback functions for TCP events
 * @param connect_cb Called when client connects
 * @param disconnect_cb Called when client disconnects
 * @param data_cb Called when data received from client
 */
void TCP_Server_RegisterCallbacks(tcp_client_connect_callback_t connect_cb,
                                  tcp_client_disconnect_callback_t disconnect_cb,
                                  tcp_data_received_callback_t data_cb);

#endif /* W5500_TCP_SERVER_H */
