/**
 * @file    w5500_tcp_server.c
 * @brief   W5500 TCP Server Implementation
 * @author  Industrial Chiller Control System
 * @version 1.0
 * @date    2025
 */

#include "w5500_tcp_server.h"
#include "w5500_driver.h"
#include "w5500_socket.h"
#include "spi_w5500.h"
#include <string.h>
#include <stdio.h>

/* Global variables */
static tcp_client_t tcp_clients[TCP_MAX_CLIENTS];
static tcp_server_stats_t server_stats;
static tcp_server_state_t server_state = TCP_STATE_IDLE;
static uint16_t server_port = TCP_SERVER_PORT;
static uint32_t server_start_time = 0;
static uint8_t data_streaming_enabled = 0;
static uint32_t streaming_interval = 5000; // Default 5 seconds
static uint32_t last_stream_time = 0;

/* Callback function pointers */
static tcp_client_connect_callback_t client_connect_cb = NULL;
static tcp_client_disconnect_callback_t client_disconnect_cb = NULL;
static tcp_data_received_callback_t data_received_cb = NULL;

/* Private function prototypes */
static void TCP_Server_InitClient(uint8_t client_id);
static uint8_t TCP_Server_FindFreeClient(void);
static void TCP_Server_HandleNewConnection(void);
static void TCP_Server_HandleClientData(uint8_t client_id);
static void TCP_Server_HandleClientDisconnect(uint8_t client_id);
static void TCP_Server_CheckTimeouts(void);
static void TCP_Server_UpdateStats(void);
static uint16_t TCP_Server_SocketSend(uint8_t socket, uint8_t *data, uint16_t length);
static uint16_t TCP_Server_SocketReceive(uint8_t socket, uint8_t *buffer, uint16_t max_length);
static void TCP_Server_FormatSensorData(char *buffer, uint16_t *temp_sensors, uint16_t *press_sensors);

/**
 * @brief Initialize TCP server on specified port
 */
uint8_t TCP_Server_Init(uint16_t port) {
    char msg[80];
    snprintf(msg, sizeof(msg), "TCP Server: Initializing on port %d...\r\n", port);
    W5500_Debug_Message(msg);

    server_port = port;
    server_state = TCP_STATE_IDLE;
    server_start_time = HAL_GetTick();

    /* Initialize client structures */
    for (uint8_t i = 0; i < TCP_MAX_CLIENTS; i++) {
        TCP_Server_InitClient(i);
    }

    /* Reset statistics */
    memset(&server_stats, 0, sizeof(tcp_server_stats_t));

    /* Initialize Socket 1 for TCP server */
    if (!W5500_Socket_Init(TCP_SERVER_SOCKET, W5500_MODE_TCP, server_port)) {
        W5500_Debug_Message("TCP Server: Socket initialization failed!\r\n");
        return 0;
    }

    W5500_Debug_Message("TCP Server: Initialization complete\r\n");
    return 1;
}

/**
 * @brief Start TCP server listening for connections
 */
uint8_t TCP_Server_Start(void) {
    W5500_Debug_Message("TCP Server: Starting server...\r\n");

    /* Put socket in LISTEN mode */
    uint8_t status = W5500_Socket_GetStatus(TCP_SERVER_SOCKET);
    if (status != W5500_SOCK_INIT) {
        W5500_Debug_Message("TCP Server: Socket not in INIT state, reinitializing...\r\n");
        if (!W5500_Socket_Init(TCP_SERVER_SOCKET, W5500_MODE_TCP, server_port)) {
            return 0;
        }
    }

    /* Send LISTEN command */
    // Note: This would require implementing socket command sending
    // For now, assume socket is ready
    server_state = TCP_STATE_LISTENING;
    server_start_time = HAL_GetTick();

    char msg[60];
    snprintf(msg, sizeof(msg), "TCP Server: Listening on port %d\r\n", server_port);
    W5500_Debug_Message(msg);

    return 1;
}

/**
 * @brief Stop TCP server and close all connections
 */
uint8_t TCP_Server_Stop(void) {
    W5500_Debug_Message("TCP Server: Stopping server...\r\n");

    /* Disconnect all clients */
    for (uint8_t i = 0; i < TCP_MAX_CLIENTS; i++) {
        if (tcp_clients[i].state != TCP_STATE_IDLE) {
            TCP_Server_DisconnectClient(i);
        }
    }

    /* Close server socket */
    W5500_Socket_Close(TCP_SERVER_SOCKET);
    server_state = TCP_STATE_IDLE;

    W5500_Debug_Message("TCP Server: Stopped\r\n");
    return 1;
}

/**
 * @brief Main TCP server processing function
 */
void TCP_Server_Process(void) {
    static uint32_t last_process_time = 0;

    /* Throttle processing to avoid overwhelming the system */
    if (HAL_GetTick() - last_process_time < 100) return; // Process every 100ms
    last_process_time = HAL_GetTick();

    if (server_state == TCP_STATE_IDLE) return;

    /* Update server statistics */
    TCP_Server_UpdateStats();

    /* Check for timeouts */
    TCP_Server_CheckTimeouts();

    /* Handle server socket */
    if (server_state == TCP_STATE_LISTENING) {
        TCP_Server_HandleNewConnection();
    }

    /* Process existing clients */
    for (uint8_t i = 0; i < TCP_MAX_CLIENTS; i++) {
        if (tcp_clients[i].state == TCP_STATE_CONNECTED ||
            tcp_clients[i].state == TCP_STATE_DATA_READY) {
            TCP_Server_HandleClientData(i);
        }
    }

    /* Handle automatic data streaming */
    if (data_streaming_enabled &&
        HAL_GetTick() - last_stream_time > streaming_interval) {
        last_stream_time = HAL_GetTick();

        /* Example: Stream current sensor data to all clients */
        char stream_data[200];
        snprintf(stream_data, sizeof(stream_data),
                "STREAM: Time=%lu, Clients=%lu, Uptime=%lu\r\n",
                HAL_GetTick(), server_stats.active_connections,
                (HAL_GetTick() - server_start_time) / 1000);
        TCP_Server_Broadcast((uint8_t*)stream_data, strlen(stream_data));
    }
}

/**
 * @brief Send data to specific client
 */
uint16_t TCP_Server_Send(uint8_t client_id, uint8_t *data, uint16_t length) {
    if (client_id >= TCP_MAX_CLIENTS) return 0;
    if (tcp_clients[client_id].state != TCP_STATE_CONNECTED) return 0;

    uint16_t sent = TCP_Server_SocketSend(tcp_clients[client_id].socket_id, data, length);

    if (sent > 0) {
        tcp_clients[client_id].bytes_sent += sent;
        tcp_clients[client_id].last_activity = HAL_GetTick();
        server_stats.total_bytes_sent += sent;
    }

    return sent;
}

/**
 * @brief Send data to all connected clients
 */
uint8_t TCP_Server_Broadcast(uint8_t *data, uint16_t length) {
    uint8_t clients_sent = 0;

    for (uint8_t i = 0; i < TCP_MAX_CLIENTS; i++) {
        if (TCP_Server_Send(i, data, length) > 0) {
            clients_sent++;
        }
    }

    return clients_sent;
}

/**
 * @brief Check if client is connected
 */
uint8_t TCP_Server_IsClientConnected(uint8_t client_id) {
    if (client_id >= TCP_MAX_CLIENTS) return 0;
    return (tcp_clients[client_id].state == TCP_STATE_CONNECTED) ? 1 : 0;
}

/**
 * @brief Disconnect specific client
 */
uint8_t TCP_Server_DisconnectClient(uint8_t client_id) {
    if (client_id >= TCP_MAX_CLIENTS) return 0;

    char msg[60];
    snprintf(msg, sizeof(msg), "TCP Server: Disconnecting client %d\r\n", client_id);
    W5500_Debug_Message(msg);

    /* Close client socket */
    W5500_Socket_Close(tcp_clients[client_id].socket_id);

    /* Call disconnect callback if registered */
    if (client_disconnect_cb) {
        client_disconnect_cb(client_id);
    }

    /* Reset client state */
    TCP_Server_InitClient(client_id);

    return 1;
}

/**
 * @brief Get server statistics
 */
uint8_t TCP_Server_GetStats(tcp_server_stats_t *stats) {
    if (stats == NULL) return 0;

    memcpy(stats, &server_stats, sizeof(tcp_server_stats_t));
    stats->uptime_seconds = (HAL_GetTick() - server_start_time) / 1000;

    return 1;
}

/**
 * @brief Send sensor data to connected clients
 */
uint8_t TCP_Server_SendSensorData(uint16_t *temp_sensors, uint16_t *press_sensors) {
    char sensor_data[300];
    TCP_Server_FormatSensorData(sensor_data, temp_sensors, press_sensors);
    return TCP_Server_Broadcast((uint8_t*)sensor_data, strlen(sensor_data));
}

/**
 * @brief Send system status message to connected clients
 */
uint8_t TCP_Server_SendSystemStatus(const char *status_msg) {
    char status_packet[200];
    snprintf(status_packet, sizeof(status_packet), "STATUS: %s\r\n", status_msg);
    return TCP_Server_Broadcast((uint8_t*)status_packet, strlen(status_packet));
}

/**
 * @brief Enable/disable automatic sensor data streaming
 */
void TCP_Server_SetDataStreaming(uint8_t enable, uint32_t interval_ms) {
    data_streaming_enabled = enable;
    streaming_interval = interval_ms;
    last_stream_time = HAL_GetTick();

    char msg[80];
    snprintf(msg, sizeof(msg), "TCP Server: Data streaming %s, interval=%lu ms\r\n",
             enable ? "enabled" : "disabled", interval_ms);
    W5500_Debug_Message(msg);
}

/**
 * @brief Register callback functions for TCP events
 */
void TCP_Server_RegisterCallbacks(tcp_client_connect_callback_t connect_cb,
                                  tcp_client_disconnect_callback_t disconnect_cb,
                                  tcp_data_received_callback_t data_cb) {
    client_connect_cb = connect_cb;
    client_disconnect_cb = disconnect_cb;
    data_received_cb = data_cb;
}

/**
 * @brief Debug function to print server status
 */
void TCP_Server_Debug(void) {
    char msg[150];
    snprintf(msg, sizeof(msg),
            "TCP Server: State=%d, Port=%d, Clients=%lu, Sent=%lu, Received=%lu\r\n",
            server_state, server_port, server_stats.active_connections,
            server_stats.total_bytes_sent, server_stats.total_bytes_received);
    W5500_Debug_Message(msg);

    for (uint8_t i = 0; i < TCP_MAX_CLIENTS; i++) {
        if (tcp_clients[i].state != TCP_STATE_IDLE) {
            snprintf(msg, sizeof(msg), "Client %d: Socket=%d, State=%d, IP=%d.%d.%d.%d\r\n",
                    i, tcp_clients[i].socket_id, tcp_clients[i].state,
                    tcp_clients[i].remote_ip[0], tcp_clients[i].remote_ip[1],
                    tcp_clients[i].remote_ip[2], tcp_clients[i].remote_ip[3]);
            W5500_Debug_Message(msg);
        }
    }
}

/* Private Functions */

/**
 * @brief Initialize client structure
 */
static void TCP_Server_InitClient(uint8_t client_id) {
    memset(&tcp_clients[client_id], 0, sizeof(tcp_client_t));
    tcp_clients[client_id].client_id = client_id;
    tcp_clients[client_id].socket_id = TCP_SERVER_SOCKET + client_id + 1; // Sockets 2-5
    tcp_clients[client_id].state = TCP_STATE_IDLE;
}

/**
 * @brief Handle new incoming connections
 */
static void TCP_Server_HandleNewConnection(void) {
    /* Check if server socket has pending connection */
    uint8_t socket_status = W5500_Socket_GetStatus(TCP_SERVER_SOCKET);

    /* This is a simplified version - full implementation would check for
     * SOCK_ESTABLISHED status and handle the connection handshake
     */
    if (socket_status == W5500_SOCK_ESTABLISHED) {
        uint8_t free_client = TCP_Server_FindFreeClient();
        if (free_client < TCP_MAX_CLIENTS) {
            /* Accept connection */
            tcp_clients[free_client].state = TCP_STATE_CONNECTED;
            tcp_clients[free_client].connect_time = HAL_GetTick();
            tcp_clients[free_client].last_activity = HAL_GetTick();

            server_stats.total_connections++;
            server_stats.active_connections++;
            server_stats.last_client_connect = HAL_GetTick();

            char msg[80];
            snprintf(msg, sizeof(msg), "TCP Server: Client %d connected\r\n", free_client);
            W5500_Debug_Message(msg);

            if (client_connect_cb) {
                client_connect_cb(free_client, tcp_clients[free_client].remote_ip);
            }
        }
    }
}

/**
 * @brief Handle data from connected client
 */
static void TCP_Server_HandleClientData(uint8_t client_id) {
    uint8_t buffer[TCP_BUFFER_SIZE];

    /* Check for received data */
    uint16_t received = TCP_Server_SocketReceive(tcp_clients[client_id].socket_id,
                                                buffer, sizeof(buffer));

    if (received > 0) {
        tcp_clients[client_id].bytes_received += received;
        tcp_clients[client_id].last_activity = HAL_GetTick();
        server_stats.total_bytes_received += received;

        /* Call data received callback if registered */
        if (data_received_cb) {
            data_received_cb(client_id, buffer, received);
        }

        /* Simple echo for testing */
        char echo[100];
        snprintf(echo, sizeof(echo), "ECHO: Received %d bytes\r\n", received);
        TCP_Server_Send(client_id, (uint8_t*)echo, strlen(echo));
    }
}

/**
 * @brief Find free client slot
 */
static uint8_t TCP_Server_FindFreeClient(void) {
    for (uint8_t i = 0; i < TCP_MAX_CLIENTS; i++) {
        if (tcp_clients[i].state == TCP_STATE_IDLE) {
            return i;
        }
    }
    return TCP_MAX_CLIENTS; // No free slots
}

/**
 * @brief Check for client timeouts
 */
static void TCP_Server_CheckTimeouts(void) {
    uint32_t current_time = HAL_GetTick();

    for (uint8_t i = 0; i < TCP_MAX_CLIENTS; i++) {
        if (tcp_clients[i].state == TCP_STATE_CONNECTED) {
            if (current_time - tcp_clients[i].last_activity > TCP_CLIENT_TIMEOUT) {
                W5500_Debug_Message("TCP Server: Client timeout, disconnecting\r\n");
                TCP_Server_DisconnectClient(i);
            }
        }
    }
}

/**
 * @brief Update server statistics
 */
static void TCP_Server_UpdateStats(void) {
    server_stats.active_connections = 0;
    for (uint8_t i = 0; i < TCP_MAX_CLIENTS; i++) {
        if (tcp_clients[i].state == TCP_STATE_CONNECTED) {
            server_stats.active_connections++;
        }
    }
}

/**
 * @brief Format sensor data for transmission
 */
static void TCP_Server_FormatSensorData(char *buffer, uint16_t *temp_sensors, uint16_t *press_sensors) {
    snprintf(buffer, 300, "SENSORS,TIME=%lu", HAL_GetTick());

    /* Add temperature sensors */
    if (temp_sensors) {
        strcat(buffer, ",TEMP=");
        for (int i = 0; i < 12; i++) {
            char temp_str[10];
            snprintf(temp_str, sizeof(temp_str), "%d.%d",
                    temp_sensors[i] / 10, temp_sensors[i] % 10);
            strcat(buffer, temp_str);
            if (i < 11) strcat(buffer, ",");
        }
    }

    /* Add pressure sensors */
    if (press_sensors) {
        strcat(buffer, ",PRESS=");
        for (int i = 0; i < 12; i++) {
            char press_str[10];
            snprintf(press_str, sizeof(press_str), "%d.%d",
                    press_sensors[i] / 10, press_sensors[i] % 10);
            strcat(buffer, press_str);
            if (i < 11) strcat(buffer, ",");
        }
    }

    strcat(buffer, "\r\n");
}

/* Placeholder socket functions - these need to be implemented based on W5500 socket operations */
static uint16_t TCP_Server_SocketSend(uint8_t socket, uint8_t *data, uint16_t length) {
    /* This would implement actual W5500 socket data transmission */
    return length; // Placeholder
}

static uint16_t TCP_Server_SocketReceive(uint8_t socket, uint8_t *buffer, uint16_t max_length) {
    /* This would implement actual W5500 socket data reception */
    return 0; // Placeholder
}
