/**
 * @file    w5500_socket.h
 * @brief   W5500 Socket Management and Buffer Configuration
 * @author  Industrial Chiller Control System
 * @version 1.0
 * @date    2025
 *
 * @description
 * This module handles W5500 socket initialization, buffer allocation,
 * and socket state management according to datasheet specifications.
 * Critical for proper network packet processing and link stability.
 */

#ifndef W5500_SOCKET_H
#define W5500_SOCKET_H

#include "main.h"
#include <stdint.h>

/* Socket Buffer Configuration */
#define W5500_MAX_SOCKETS           8
#define W5500_TOTAL_MEMORY          16384   // 16KB total internal memory
#define W5500_SOCKET_BUFFER_SIZE    2048    // 2KB per socket (TX + RX)

/* W5500 Socket Buffer Registers */
#define W5500_TMSR                  0x001B  // TX Memory Size Register
#define W5500_RMSR                  0x001A  // RX Memory Size Register

/* Socket Register Base Addresses */
#define W5500_SOCKET_BASE           0x0400  // Socket 0 base address
#define W5500_SOCKET_OFFSET         0x0100  // Offset between sockets

/* Socket Register Offsets (from socket base) */
#define W5500_Sn_MR                 0x0000  // Socket Mode Register
#define W5500_Sn_CR                 0x0001  // Socket Command Register
#define W5500_Sn_IR                 0x0002  // Socket Interrupt Register
#define W5500_Sn_SR                 0x0003  // Socket Status Register
#define W5500_Sn_PORT0              0x0004  // Socket Source Port
#define W5500_Sn_PORT1              0x0005
#define W5500_Sn_DHAR0              0x0006  // Destination Hardware Address
#define W5500_Sn_DIPR0              0x000C  // Destination IP Address
#define W5500_Sn_DPORT0             0x0010  // Destination Port
#define W5500_Sn_MSSR0              0x0012  // Maximum Segment Size
#define W5500_Sn_TOS                0x0015  // Type of Service
#define W5500_Sn_TTL                0x0016  // Time to Live

/* Socket Commands */
#define W5500_CMD_OPEN              0x01
#define W5500_CMD_LISTEN            0x02
#define W5500_CMD_CONNECT           0x04
#define W5500_CMD_DISCON            0x08
#define W5500_CMD_CLOSE             0x10
#define W5500_CMD_SEND              0x20
#define W5500_CMD_SEND_MAC          0x21
#define W5500_CMD_SEND_KEEP         0x22
#define W5500_CMD_RECV              0x40

/* Socket Modes */
#define W5500_MODE_CLOSED           0x00
#define W5500_MODE_TCP              0x01
#define W5500_MODE_UDP              0x02
#define W5500_MODE_IPRAW            0x03
#define W5500_MODE_MACRAW           0x04
#define W5500_MODE_PPPoE            0x05

/* Socket Status Values */
#define W5500_SOCK_CLOSED           0x00
#define W5500_SOCK_INIT             0x13
#define W5500_SOCK_LISTEN           0x14
#define W5500_SOCK_SYNSENT          0x15
#define W5500_SOCK_SYNRECV          0x16
#define W5500_SOCK_ESTABLISHED      0x17
#define W5500_SOCK_FIN_WAIT         0x18
#define W5500_SOCK_CLOSING          0x1A
#define W5500_SOCK_TIME_WAIT        0x1B
#define W5500_SOCK_CLOSE_WAIT       0x1C
#define W5500_SOCK_LAST_ACK         0x1D
#define W5500_SOCK_UDP              0x22
#define W5500_SOCK_IPRAW            0x32
#define W5500_SOCK_MACRAW           0x42
#define W5500_SOCK_PPPoE            0x5F

/* Socket Buffer Size Options (in KB) */
typedef enum {
    SOCKET_BUF_1KB = 0x00,
    SOCKET_BUF_2KB = 0x01,
    SOCKET_BUF_4KB = 0x02,
    SOCKET_BUF_8KB = 0x03
} socket_buffer_size_t;

/* Socket State Structure */
typedef struct {
    uint8_t socket_id;
    uint8_t mode;
    uint8_t status;
    uint16_t local_port;
    uint8_t tx_buffer_size;
    uint8_t rx_buffer_size;
    uint32_t last_activity;
    uint8_t error_count;
} w5500_socket_t;

/* Function Declarations */
uint8_t W5500_Socket_Init_TCP_Server_Debug(uint8_t socket, uint16_t port);
void W5500_Socket_Diagnostic(uint8_t socket);
void Test_Socket_Access_Methods(uint8_t socket);

uint8_t W5500_Socket_Init_TCP_Fixed(uint8_t socket, uint16_t port);
void Test_Corrected_Socket_Access(uint8_t socket);

uint8_t W5500_Socket_Init_TCP_Direct(uint8_t socket, uint16_t port);
void Test_Direct_Socket_Access(uint8_t socket);
uint8_t W5500_Init_Socket_Buffers_Proper(void);
/**
 * @brief Initialize all socket buffers according to datasheet
 * @return 1 if successful, 0 if failed
 */
uint8_t W5500_Socket_InitBuffers(void);

/**
 * @brief Configure buffer sizes for specific socket
 * @param socket Socket number (0-7)
 * @param tx_size TX buffer size
 * @param rx_size RX buffer size
 * @return 1 if successful, 0 if failed
 */
uint8_t W5500_Socket_SetBufferSize(uint8_t socket, socket_buffer_size_t tx_size, socket_buffer_size_t rx_size);

/**
 * @brief Initialize specific socket with mode and port
 * @param socket Socket number (0-7)
 * @param mode Socket mode (TCP, UDP, etc.)
 * @param port Local port number
 * @return 1 if successful, 0 if failed
 */
uint8_t W5500_Socket_Init(uint8_t socket, uint8_t mode, uint16_t port);

/**
 * @brief Get current socket status
 * @param socket Socket number (0-7)
 * @return Socket status value
 */
uint8_t W5500_Socket_GetStatus(uint8_t socket);

/**
 * @brief Close specific socket
 * @param socket Socket number (0-7)
 * @return 1 if successful, 0 if failed
 */
uint8_t W5500_Socket_Close(uint8_t socket);

/**
 * @brief Close all sockets (system reset)
 * @return 1 if successful, 0 if failed
 */
uint8_t W5500_Socket_CloseAll(void);

/**
 * @brief Check if socket is ready for operation
 * @param socket Socket number (0-7)
 * @return 1 if ready, 0 if not ready
 */
uint8_t W5500_Socket_IsReady(uint8_t socket);

/**
 * @brief Get socket state information
 * @param socket Socket number (0-7)
 * @param state Pointer to socket state structure
 * @return 1 if successful, 0 if failed
 */
uint8_t W5500_Socket_GetState(uint8_t socket, w5500_socket_t *state);

/**
 * @brief Reset all socket states (used during link recovery)
 * @return 1 if successful, 0 if failed
 */
uint8_t W5500_Socket_ResetAll(void);

/**
 * @brief Debug function to print socket status
 * @param socket Socket number (0-7), 0xFF for all sockets
 */
void W5500_Socket_Debug(uint8_t socket);

#endif /* W5500_SOCKET_H */
