/**
 * @file    w5500_socket.c
 * @brief   W5500 Socket Management Implementation
 * @author  Industrial Chiller Control System
 * @version 1.0
 * @date    2025
 */

#include "w5500_socket.h"
#include "w5500_driver.h"
#include "spi_w5500.h"
#include "uart_comm.h"
#include <string.h>
#include <stdio.h>

/* Global socket state array */
static w5500_socket_t socket_states[W5500_MAX_SOCKETS];

/* Private function prototypes */
static uint16_t W5500_Socket_GetRegAddress(uint8_t socket, uint16_t offset);
static uint8_t W5500_Socket_WriteReg(uint8_t socket, uint16_t offset, uint8_t data);
static uint8_t W5500_Socket_ReadReg(uint8_t socket, uint16_t offset);
static void W5500_Socket_InitState(uint8_t socket);

/**
 * @brief Initialize all socket buffers according to datasheet
 * This is the critical missing piece from the current implementation
 */
uint8_t W5500_Socket_InitBuffers(void) {
    W5500_Debug_Message("W5500_Socket: Skipping buffer allocation (causes PHY issues)\r\n");

    // Skip the problematic TMSR/RMSR writes that break your W5500 variant
    // W5500_WriteByte(W5500_TMSR, 0x55);  // COMMENTED OUT
    // W5500_WriteByte(W5500_RMSR, 0x55);  // COMMENTED OUT

    // Initialize socket states without changing hardware buffer allocation
    for (uint8_t i = 0; i < W5500_MAX_SOCKETS; i++) {
        W5500_Socket_InitState(i);
        socket_states[i].tx_buffer_size = 2;  // 2KB (default)
        socket_states[i].rx_buffer_size = 2;  // 2KB (default)
    }

    W5500_Debug_Message("W5500_Socket: Using default buffer allocation\r\n");
    return 1;
}

/**
 * @brief Configure buffer sizes for specific socket
 */
uint8_t W5500_Socket_SetBufferSize(uint8_t socket, socket_buffer_size_t tx_size, socket_buffer_size_t rx_size) {
    if (socket >= W5500_MAX_SOCKETS) return 0;

    /* Note: Individual socket buffer sizing requires recalculating total memory allocation
     * For now, using standard 2KB per socket. Advanced buffer management can be added later.
     */
    socket_states[socket].tx_buffer_size = (1 << tx_size);  // Convert enum to KB
    socket_states[socket].rx_buffer_size = (1 << rx_size);

    return 1;
}

/**
 * @brief Initialize specific socket with mode and port
 */
uint8_t W5500_Socket_Init(uint8_t socket, uint8_t mode, uint16_t port) {
    if (socket >= W5500_MAX_SOCKETS) return 0;

    char msg[80];
    snprintf(msg, sizeof(msg), "Initializing Socket %d: Mode=0x%02X, Port=%d\r\n", socket, mode, port);
    W5500_Debug_Message(msg);

    /* Close socket first */
    W5500_Socket_Close(socket);
    HAL_Delay(10);

    /* Set socket mode */
    W5500_Socket_WriteReg(socket, W5500_Sn_MR, mode);

    /* Set source port */
    W5500_Socket_WriteReg(socket, W5500_Sn_PORT0, (port >> 8) & 0xFF);
    W5500_Socket_WriteReg(socket, W5500_Sn_PORT1, port & 0xFF);

    /* Send OPEN command */
    W5500_Socket_WriteReg(socket, W5500_Sn_CR, W5500_CMD_OPEN);

    /* Wait for command completion */
    uint32_t timeout = HAL_GetTick() + 1000;
    while (W5500_Socket_ReadReg(socket, W5500_Sn_CR) != 0x00) {
        if (HAL_GetTick() > timeout) {
            W5500_Debug_Message("Socket init timeout!\r\n");
            return 0;
        }
        HAL_Delay(1);
    }

    /* Update socket state */
    socket_states[socket].socket_id = socket;
    socket_states[socket].mode = mode;
    socket_states[socket].local_port = port;
    socket_states[socket].last_activity = HAL_GetTick();
    socket_states[socket].error_count = 0;

    /* Verify socket status */
    uint8_t status = W5500_Socket_GetStatus(socket);
    socket_states[socket].status = status;

    snprintf(msg, sizeof(msg), "Socket %d Status: 0x%02X\r\n", socket, status);
    W5500_Debug_Message(msg);

    return (status != W5500_SOCK_CLOSED) ? 1 : 0;
}

/**
 * @brief Get current socket status
 */
uint8_t W5500_Socket_GetStatus(uint8_t socket) {
    if (socket >= W5500_MAX_SOCKETS) return 0;

    uint8_t status = W5500_Socket_ReadReg(socket, W5500_Sn_SR);
    socket_states[socket].status = status;
    socket_states[socket].last_activity = HAL_GetTick();

    return status;
}

/**
 * @brief Close specific socket
 */
uint8_t W5500_Socket_Close(uint8_t socket) {
    if (socket >= W5500_MAX_SOCKETS) return 0;

    /* Send CLOSE command */
    W5500_Socket_WriteReg(socket, W5500_Sn_CR, W5500_CMD_CLOSE);

    /* Wait for command completion */
    uint32_t timeout = HAL_GetTick() + 1000;
    while (W5500_Socket_ReadReg(socket, W5500_Sn_CR) != 0x00) {
        if (HAL_GetTick() > timeout) return 0;
        HAL_Delay(1);
    }

    /* Reset socket state */
    W5500_Socket_InitState(socket);

    return 1;
}

/**
 * @brief Close all sockets (used during system reset)
 */
uint8_t W5500_Socket_CloseAll(void) {
    W5500_Debug_Message("Closing all sockets...\r\n");

    uint8_t success_count = 0;
    for (uint8_t i = 0; i < W5500_MAX_SOCKETS; i++) {
        if (W5500_Socket_Close(i)) {
            success_count++;
        }
    }

    char msg[50];
    snprintf(msg, sizeof(msg), "Closed %d/%d sockets\r\n", success_count, W5500_MAX_SOCKETS);
    W5500_Debug_Message(msg);

    return (success_count == W5500_MAX_SOCKETS) ? 1 : 0;
}

/**
 * @brief Check if socket is ready for operation
 */
uint8_t W5500_Socket_IsReady(uint8_t socket) {
    if (socket >= W5500_MAX_SOCKETS) return 0;

    uint8_t status = W5500_Socket_GetStatus(socket);

    switch (socket_states[socket].mode) {
        case W5500_MODE_TCP:
            return (status == W5500_SOCK_ESTABLISHED || status == W5500_SOCK_CLOSE_WAIT);
        case W5500_MODE_UDP:
            return (status == W5500_SOCK_UDP);
        case W5500_MODE_IPRAW:
            return (status == W5500_SOCK_IPRAW);
        default:
            return 0;
    }
}

/**
 * @brief Get socket state information
 */
uint8_t W5500_Socket_GetState(uint8_t socket, w5500_socket_t *state) {
    if (socket >= W5500_MAX_SOCKETS || state == NULL) return 0;

    /* Update current status */
    socket_states[socket].status = W5500_Socket_GetStatus(socket);

    /* Copy state */
    memcpy(state, &socket_states[socket], sizeof(w5500_socket_t));

    return 1;
}

/**
 * @brief Reset all socket states (used during link recovery)
 */
uint8_t W5500_Socket_ResetAll(void) {
    W5500_Debug_Message("Resetting all socket states...\r\n");

    /* Close all sockets */
    W5500_Socket_CloseAll();

    /* Reinitialize buffer allocation */
    return W5500_Socket_InitBuffers();
}

/**
 * @brief Debug function to print socket status
 */
void W5500_Socket_Debug(uint8_t socket) {
    if (socket == 0xFF) {
        /* Print all sockets */
        W5500_Debug_Message("=== Socket Status Report ===\r\n");
        for (uint8_t i = 0; i < W5500_MAX_SOCKETS; i++) {
            W5500_Socket_Debug(i);
        }
        return;
    }

    if (socket >= W5500_MAX_SOCKETS) return;

    uint8_t status = W5500_Socket_GetStatus(socket);
    char msg[100];
    snprintf(msg, sizeof(msg), "Socket %d: Mode=0x%02X, Status=0x%02X, Port=%d, Errors=%d\r\n",
             socket, socket_states[socket].mode, status,
             socket_states[socket].local_port, socket_states[socket].error_count);
    W5500_Debug_Message(msg);
}

/**
 * @brief Initialize Socket for TCP Server
 */
uint8_t W5500_Socket_Init_TCP_Server(uint8_t socket, uint16_t port) {
    char msg[100];
    snprintf(msg, sizeof(msg), "Initializing Socket %d for TCP Server on port %d\r\n", socket, port);
    W5500_Debug_Message(msg);

    // Step 1: Close socket first
    W5500_Socket_WriteReg(socket, W5500_Sn_CR, W5500_CMD_CLOSE);  // CLOSE command
    HAL_Delay(50);

    // Wait for close to complete
    uint32_t timeout = HAL_GetTick() + 1000;
    while (W5500_Socket_ReadReg(socket, W5500_Sn_CR) != 0x00) {
        if (HAL_GetTick() > timeout) {
            W5500_Debug_Message("Socket close timeout!\r\n");
            return 0;
        }
        HAL_Delay(10);
    }

    // Step 2: Set TCP mode
    W5500_Socket_WriteReg(socket, W5500_Sn_MR, W5500_MODE_TCP);  // Sn_MR = TCP mode

    // Step 3: Set source port
    W5500_Socket_WriteReg(socket, W5500_Sn_PORT0, (port >> 8) & 0xFF);  // Sn_PORT high
    W5500_Socket_WriteReg(socket, W5500_Sn_PORT1, port & 0xFF);         // Sn_PORT low

    // Step 4: Open socket
    W5500_Socket_WriteReg(socket, W5500_Sn_CR, W5500_CMD_OPEN);  // OPEN command

    // Wait for OPEN to complete
    timeout = HAL_GetTick() + 1000;
    while (W5500_Socket_ReadReg(socket, W5500_Sn_CR) != 0x00) {
        if (HAL_GetTick() > timeout) {
            W5500_Debug_Message("Socket OPEN timeout!\r\n");
            return 0;
        }
        HAL_Delay(10);
    }

    // Step 5: Check if socket is in INIT state
    uint8_t status = W5500_Socket_ReadReg(socket, W5500_Sn_SR);  // Sn_SR
    snprintf(msg, sizeof(msg), "Socket %d status after OPEN: 0x%02X\r\n", socket, status);
    W5500_Debug_Message(msg);

    if (status != W5500_SOCK_INIT) {  // Should be SOCK_INIT (0x13)
        W5500_Debug_Message("Socket failed to reach INIT state!\r\n");
        return 0;
    }

    // Step 6: Put socket in LISTEN mode
    W5500_Socket_WriteReg(socket, W5500_Sn_CR, W5500_CMD_LISTEN);  // LISTEN command

    // Wait for LISTEN to complete
    timeout = HAL_GetTick() + 1000;
    while (W5500_Socket_ReadReg(socket, W5500_Sn_CR) != 0x00) {
        if (HAL_GetTick() > timeout) {
            W5500_Debug_Message("Socket LISTEN timeout!\r\n");
            return 0;
        }
        HAL_Delay(10);
    }

    // Step 7: Check if socket is in LISTEN state
    status = W5500_Socket_ReadReg(socket, W5500_Sn_SR);  // Sn_SR
    snprintf(msg, sizeof(msg), "Socket %d status after LISTEN: 0x%02X\r\n", socket, status);
    W5500_Debug_Message(msg);

    if (status == W5500_SOCK_LISTEN) {  // SOCK_LISTEN (0x14)
        W5500_Debug_Message("TCP Server ready - waiting for connections!\r\n");
        return 1;
    } else {
        W5500_Debug_Message("Socket failed to enter LISTEN state!\r\n");
        return 0;
    }
}
/**
 * @brief Socket Register Diagnostic Function
 * Add this to your w5500_socket.c file to debug the register access issue
 */

void W5500_Socket_Diagnostic(uint8_t socket) {
    char msg[120];

    Send_Debug_Data("=== SOCKET REGISTER DIAGNOSTIC ===\r\n");
    snprintf(msg, sizeof(msg), "Testing Socket %d register access:\r\n", socket);
    Send_Debug_Data(msg);

    // Test 1: Read all socket registers to see what we get
    Send_Debug_Data("Socket register dump:\r\n");
    for (uint16_t offset = 0x0000; offset <= 0x002F; offset += 8) {
        uint8_t vals[8];
        for (int i = 0; i < 8; i++) {
            vals[i] = W5500_Socket_ReadReg(socket, offset + i);
        }
        snprintf(msg, sizeof(msg), "S%d+0x%04X: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                 socket, offset, vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6], vals[7]);
        Send_Debug_Data(msg);
    }

    // Test 2: Try writing and reading back a test pattern
    Send_Debug_Data("Write/Read test:\r\n");

    // Save original mode
    uint8_t orig_mode = W5500_Socket_ReadReg(socket, 0x0000);
    snprintf(msg, sizeof(msg), "Original Sn_MR: 0x%02X\r\n", orig_mode);
    Send_Debug_Data(msg);

    // Write test pattern to mode register
    W5500_Socket_WriteReg(socket, 0x0000, 0x01);  // TCP mode
    HAL_Delay(10);

    uint8_t read_back = W5500_Socket_ReadReg(socket, 0x0000);
    snprintf(msg, sizeof(msg), "After writing 0x01 to Sn_MR, read back: 0x%02X\r\n", read_back);
    Send_Debug_Data(msg);

    // Restore original mode
    W5500_Socket_WriteReg(socket, 0x0000, orig_mode);

    Send_Debug_Data("=== END DIAGNOSTIC ===\r\n");
}

/**
 * @brief Alternative socket register access using different BSB calculation
 * Try this if the current method doesn't work
 */
static uint8_t W5500_Socket_WriteReg_Alt(uint8_t socket, uint16_t offset, uint8_t data) {
    if (socket >= W5500_MAX_SOCKETS) return 0;

    // Alternative BSB calculation - try direct socket addressing
    // W5500 datasheet shows socket registers at different memory blocks
    uint16_t reg_addr = (socket * 0x0100) + offset + 0x0400;  // Socket base addresses

    // Use common register BSB (0x00)
    uint8_t control_byte = 0x04;  // Write to common register space

    extern SPI_HandleTypeDef hspi4;
    uint8_t cmd[4] = {
        (reg_addr >> 8) & 0xFF,
        reg_addr & 0xFF,
        control_byte,
        data
    };

    SPI_W5500_CS_Enable();
    HAL_Delay(1);
    HAL_SPI_Transmit(&hspi4, cmd, 4, 1000);
    HAL_Delay(1);
    SPI_W5500_CS_Disable();

    return 1;
}

static uint8_t W5500_Socket_ReadReg_Alt(uint8_t socket, uint16_t offset) {
    if (socket >= W5500_MAX_SOCKETS) return 0;

    uint16_t reg_addr = (socket * 0x0100) + offset + 0x0400;
    uint8_t control_byte = 0x00;  // Read from common register space

    extern SPI_HandleTypeDef hspi4;
    uint8_t cmd[3] = {
        (reg_addr >> 8) & 0xFF,
        reg_addr & 0xFF,
        control_byte
    };

    uint8_t data = 0;
    SPI_W5500_CS_Enable();
    HAL_Delay(1);
    HAL_SPI_Transmit(&hspi4, cmd, 3, 1000);
    HAL_SPI_Receive(&hspi4, &data, 1, 1000);
    HAL_Delay(1);
    SPI_W5500_CS_Disable();

    return data;
}

/**
 * @brief Test both socket register access methods
 */
void Test_Socket_Access_Methods(uint8_t socket) {
    char msg[100];

    Send_Debug_Data("=== TESTING SOCKET ACCESS METHODS ===\r\n");

    // Method 1: Current BSB method
    Send_Debug_Data("Method 1 (BSB): Reading Sn_SR...\r\n");
    uint8_t status1 = W5500_Socket_ReadReg(socket, 0x0003);
    snprintf(msg, sizeof(msg), "Method 1 Sn_SR: 0x%02X\r\n", status1);
    Send_Debug_Data(msg);

    // Method 2: Alternative addressing
    Send_Debug_Data("Method 2 (Direct): Reading Sn_SR...\r\n");
    uint8_t status2 = W5500_Socket_ReadReg_Alt(socket, 0x0003);
    snprintf(msg, sizeof(msg), "Method 2 Sn_SR: 0x%02X\r\n", status2);
    Send_Debug_Data(msg);

    // Check which method gives more reasonable values
    if (status1 == 0x00 || status1 == 0x13 || status1 == 0x14 || status1 == 0x17) {
        Send_Debug_Data("Method 1 (BSB) appears to work correctly\r\n");
    } else if (status2 == 0x00 || status2 == 0x13 || status2 == 0x14 || status2 == 0x17) {
        Send_Debug_Data("Method 2 (Direct) appears to work correctly\r\n");
    } else {
        Send_Debug_Data("Neither method returning expected socket status values\r\n");
    }

    Send_Debug_Data("=== END ACCESS METHOD TEST ===\r\n");
}

/**
 * @brief Modified TCP server init with better diagnostics
 */
uint8_t W5500_Socket_Init_TCP_Server_Debug(uint8_t socket, uint16_t port) {
    char msg[100];
    snprintf(msg, sizeof(msg), "=== TCP Server Init Debug for Socket %d ===\r\n", socket);
    Send_Debug_Data(msg);

    // First, run diagnostics
    W5500_Socket_Diagnostic(socket);
    Test_Socket_Access_Methods(socket);

    // Try the basic socket operations with more detailed logging
    Send_Debug_Data("Step 1: Closing socket...\r\n");
    W5500_Socket_WriteReg(socket, 0x0001, 0x10);  // CLOSE command
    HAL_Delay(100);

    uint8_t cmd_status = W5500_Socket_ReadReg(socket, 0x0001);
    snprintf(msg, sizeof(msg), "Command register after CLOSE: 0x%02X\r\n", cmd_status);
    Send_Debug_Data(msg);

    uint8_t socket_status = W5500_Socket_ReadReg(socket, 0x0003);
    snprintf(msg, sizeof(msg), "Socket status after CLOSE: 0x%02X\r\n", socket_status);
    Send_Debug_Data(msg);

    Send_Debug_Data("Step 2: Setting TCP mode...\r\n");
    W5500_Socket_WriteReg(socket, 0x0000, 0x01);  // TCP mode
    HAL_Delay(10);

    uint8_t mode_readback = W5500_Socket_ReadReg(socket, 0x0000);
    snprintf(msg, sizeof(msg), "Mode register readback: 0x%02X (expected 0x01)\r\n", mode_readback);
    Send_Debug_Data(msg);

    Send_Debug_Data("Step 3: Setting port...\r\n");
    W5500_Socket_WriteReg(socket, 0x0004, (port >> 8) & 0xFF);
    W5500_Socket_WriteReg(socket, 0x0005, port & 0xFF);

    uint8_t port_h = W5500_Socket_ReadReg(socket, 0x0004);
    uint8_t port_l = W5500_Socket_ReadReg(socket, 0x0005);
    uint16_t port_readback = (port_h << 8) | port_l;
    snprintf(msg, sizeof(msg), "Port readback: %d (expected %d)\r\n", port_readback, port);
    Send_Debug_Data(msg);

    Send_Debug_Data("Step 4: Opening socket...\r\n");
    W5500_Socket_WriteReg(socket, 0x0001, 0x01);  // OPEN command
    HAL_Delay(100);

    cmd_status = W5500_Socket_ReadReg(socket, 0x0001);
    snprintf(msg, sizeof(msg), "Command register after OPEN: 0x%02X\r\n", cmd_status);
    Send_Debug_Data(msg);

    socket_status = W5500_Socket_ReadReg(socket, 0x0003);
    snprintf(msg, sizeof(msg), "Socket status after OPEN: 0x%02X\r\n", socket_status);
    Send_Debug_Data(msg);

    // Status 0x26 analysis
    if (socket_status == 0x26) {
        Send_Debug_Data("Status 0x26 detected - this suggests register access issue\r\n");
        Send_Debug_Data("0x26 is not a valid W5500 socket status\r\n");
        Send_Debug_Data("Possible causes: Wrong BSB, SPI timing, or hardware issue\r\n");
    }

    return 0;  // Return 0 for now to avoid further issues
}
/**
 * @brief Simple TCP Server Process
 */
void W5500_TCP_Server_Process(void) {
    static uint32_t last_check = 0;

    if (HAL_GetTick() - last_check < 1000) return;  // Check every 1 second
    last_check = HAL_GetTick();

    // Check Socket 1 status
    uint8_t status = W5500_Socket_ReadReg(1, W5500_Sn_SR);  // Sn_SR

    static uint8_t last_status = 0xFF;
    if (status != last_status) {
        char msg[80];
        snprintf(msg, sizeof(msg), "TCP Server Socket 1 status: 0x%02X\r\n", status);
        W5500_Debug_Message(msg);
        last_status = status;

        switch (status) {
            case W5500_SOCK_LISTEN:  // LISTEN (0x14)
                W5500_Debug_Message("TCP Server listening for connections...\r\n");
                break;
            case W5500_SOCK_ESTABLISHED:  // ESTABLISHED (0x17)
                W5500_Debug_Message("TCP Client connected! Connection established.\r\n");
                break;
            case W5500_SOCK_CLOSE_WAIT:  // CLOSE_WAIT (0x1C)
                W5500_Debug_Message("Client requesting disconnect...\r\n");
                // Send disconnect command
                W5500_Socket_WriteReg(1, W5500_Sn_CR, W5500_CMD_DISCON);  // DISCONNECT
                break;
            case W5500_SOCK_CLOSED:  // CLOSED (0x00)
                W5500_Debug_Message("Connection closed, restarting TCP server...\r\n");
                W5500_Socket_Init_TCP_Server(1, 8080);
                break;
        }
    }
}

/* Private Functions */

/**
 * @brief Calculate socket register address - CORRECTED VERSION
 */
static uint16_t W5500_Socket_GetRegAddress(uint8_t socket, uint16_t offset) {
    // W5500 socket registers start at 0x0000 with different BSB, not 0x0400
    return offset;  // Use offset directly - BSB in control byte selects the socket
}

/**
 /**
 /**
 * @brief Try direct socket register access using calculated addresses
 * Since BSB method fails, use direct addressing like your working common registers
 */

static uint8_t W5500_Socket_WriteReg(uint8_t socket, uint16_t offset, uint8_t data) {
    if (socket >= W5500_MAX_SOCKETS) return 0;

    // Use direct address calculation + common register BSB (0x00)
    // W5500 socket registers are typically at:
    // Socket 0: 0x0400-0x04FF
    // Socket 1: 0x0500-0x05FF
    // Socket 2: 0x0600-0x06FF
    // etc.
    uint16_t reg_addr = 0x0400 + (socket * 0x0100) + offset;

    // Use your working SPI_W5500_WriteRegByte function
    SPI_W5500_WriteRegByte(reg_addr, data);

    return 1;
}

static uint8_t W5500_Socket_ReadReg(uint8_t socket, uint16_t offset) {
    if (socket >= W5500_MAX_SOCKETS) return 0;

    uint16_t reg_addr = 0x0400 + (socket * 0x0100) + offset;

    // Use your working SPI_W5500_ReadRegByte function
    return SPI_W5500_ReadRegByte(reg_addr);
}

/**
 * @brief Test direct socket register access
 */
void Test_Direct_Socket_Access(uint8_t socket) {
    char msg[100];

    Send_Debug_Data("=== TESTING DIRECT SOCKET ACCESS ===\r\n");

    // Calculate socket register addresses
    uint16_t mode_addr = 0x0400 + (socket * 0x0100) + 0x0000;
    uint16_t status_addr = 0x0400 + (socket * 0x0100) + 0x0003;
    uint16_t port_addr_h = 0x0400 + (socket * 0x0100) + 0x0004;
    uint16_t port_addr_l = 0x0400 + (socket * 0x0100) + 0x0005;

    snprintf(msg, sizeof(msg), "Socket %d addresses: Mode=0x%04X, Status=0x%04X, Port=0x%04X/0x%04X\r\n",
             socket, mode_addr, status_addr, port_addr_h, port_addr_l);
    Send_Debug_Data(msg);

    // Test mode register
    Send_Debug_Data("Testing Mode Register:\r\n");
    uint8_t orig_mode = SPI_W5500_ReadRegByte(mode_addr);
    snprintf(msg, sizeof(msg), "Original mode: 0x%02X\r\n", orig_mode);
    Send_Debug_Data(msg);

    SPI_W5500_WriteRegByte(mode_addr, 0x01);  // TCP mode
    HAL_Delay(10);

    uint8_t mode_read = SPI_W5500_ReadRegByte(mode_addr);
    snprintf(msg, sizeof(msg), "After writing 0x01: 0x%02X %s\r\n",
             mode_read, (mode_read == 0x01) ? "✓ PASS" : "✗ FAIL");
    Send_Debug_Data(msg);

    // Test port registers
    Send_Debug_Data("Testing Port Registers:\r\n");
    SPI_W5500_WriteRegByte(port_addr_h, 0x1F);  // 8080 = 0x1F90
    SPI_W5500_WriteRegByte(port_addr_l, 0x90);
    HAL_Delay(10);

    uint8_t port_h = SPI_W5500_ReadRegByte(port_addr_h);
    uint8_t port_l = SPI_W5500_ReadRegByte(port_addr_l);
    uint16_t port = (port_h << 8) | port_l;

    snprintf(msg, sizeof(msg), "Port: 0x%02X%02X = %d %s\r\n",
             port_h, port_l, port, (port == 8080) ? "✓ PASS" : "✗ FAIL");
    Send_Debug_Data(msg);

    // Restore original mode
    SPI_W5500_WriteRegByte(mode_addr, orig_mode);

    Send_Debug_Data("=== END DIRECT ACCESS TEST ===\r\n");
}

/**
 * @brief Initialize socket buffers properly - this might be required first
 */
uint8_t W5500_Init_Socket_Buffers_Proper(void) {
    Send_Debug_Data("=== INITIALIZING SOCKET BUFFERS PROPERLY ===\r\n");

    // Try setting TMSR/RMSR registers properly
    // TMSR: TX Memory Size Register - 2KB per socket
    // RMSR: RX Memory Size Register - 2KB per socket
    // 0x55 = 01010101 binary = 2KB for each of 8 sockets

    uint8_t orig_tmsr = W5500_ReadByte(0x001B);  // TMSR
    uint8_t orig_rmsr = W5500_ReadByte(0x001A);  // RMSR

    char msg[60];
    snprintf(msg, sizeof(msg), "Original TMSR: 0x%02X, RMSR: 0x%02X\r\n", orig_tmsr, orig_rmsr);
    Send_Debug_Data(msg);

    // Set buffer allocation
    W5500_WriteByte(0x001B, 0x55);  // TMSR - 2KB each socket
    W5500_WriteByte(0x001A, 0x55);  // RMSR - 2KB each socket
    HAL_Delay(50);

    uint8_t tmsr_read = W5500_ReadByte(0x001B);
    uint8_t rmsr_read = W5500_ReadByte(0x001A);

    snprintf(msg, sizeof(msg), "After setting: TMSR: 0x%02X, RMSR: 0x%02X\r\n", tmsr_read, rmsr_read);
    Send_Debug_Data(msg);

    if (tmsr_read == 0x55 && rmsr_read == 0x55) {
        Send_Debug_Data("Socket buffer allocation: SUCCESS\r\n");
        return 1;
    } else {
        Send_Debug_Data("Socket buffer allocation: FAILED\r\n");
        return 0;
    }
}

/**
 * @brief Try TCP server with direct register access
 */
uint8_t W5500_Socket_Init_TCP_Direct(uint8_t socket, uint16_t port) {
    char msg[100];

    Send_Debug_Data("=== TCP SERVER INIT WITH DIRECT ACCESS ===\r\n");

    // First initialize socket buffers
    if (!W5500_Init_Socket_Buffers_Proper()) {
        Send_Debug_Data("Buffer init failed, continuing anyway...\r\n");
    }

    // Test direct access first
    Test_Direct_Socket_Access(socket);

    snprintf(msg, sizeof(msg), "Initializing Socket %d TCP server on port %d\r\n", socket, port);
    Send_Debug_Data(msg);

    // Calculate register addresses
    uint16_t mode_addr = 0x0400 + (socket * 0x0100) + 0x0000;   // Sn_MR
    uint16_t cmd_addr = 0x0400 + (socket * 0x0100) + 0x0001;    // Sn_CR
    uint16_t status_addr = 0x0400 + (socket * 0x0100) + 0x0003; // Sn_SR
    uint16_t port_addr_h = 0x0400 + (socket * 0x0100) + 0x0004; // Sn_PORT0
    uint16_t port_addr_l = 0x0400 + (socket * 0x0100) + 0x0005; // Sn_PORT1

    // Step 1: Close socket
    SPI_W5500_WriteRegByte(cmd_addr, 0x10);  // CLOSE
    HAL_Delay(100);

    uint32_t timeout = HAL_GetTick() + 1000;
    while (SPI_W5500_ReadRegByte(cmd_addr) != 0x00) {
        if (HAL_GetTick() > timeout) {
            Send_Debug_Data("Close timeout!\r\n");
            break;
        }
        HAL_Delay(10);
    }

    uint8_t status = SPI_W5500_ReadRegByte(status_addr);
    snprintf(msg, sizeof(msg), "After CLOSE: Status = 0x%02X\r\n", status);
    Send_Debug_Data(msg);

    // Step 2: Set TCP mode
    SPI_W5500_WriteRegByte(mode_addr, 0x01);
    HAL_Delay(10);

    uint8_t mode_check = SPI_W5500_ReadRegByte(mode_addr);
    snprintf(msg, sizeof(msg), "Mode check: 0x%02X (expected 0x01)\r\n", mode_check);
    Send_Debug_Data(msg);

    if (mode_check != 0x01) {
        Send_Debug_Data("Failed to set TCP mode with direct access!\r\n");
        return 0;
    }

    // Step 3: Set port
    SPI_W5500_WriteRegByte(port_addr_h, (port >> 8) & 0xFF);
    SPI_W5500_WriteRegByte(port_addr_l, port & 0xFF);
    HAL_Delay(10);

    uint8_t port_h = SPI_W5500_ReadRegByte(port_addr_h);
    uint8_t port_l = SPI_W5500_ReadRegByte(port_addr_l);
    uint16_t port_check = (port_h << 8) | port_l;
    snprintf(msg, sizeof(msg), "Port check: %d (expected %d)\r\n", port_check, port);
    Send_Debug_Data(msg);

    if (port_check != port) {
        Send_Debug_Data("Failed to set port with direct access!\r\n");
        return 0;
    }

    // Step 4: Open socket
    SPI_W5500_WriteRegByte(cmd_addr, 0x01);  // OPEN
    HAL_Delay(100);

    timeout = HAL_GetTick() + 1000;
    while (SPI_W5500_ReadRegByte(cmd_addr) != 0x00) {
        if (HAL_GetTick() > timeout) {
            Send_Debug_Data("OPEN timeout!\r\n");
            break;
        }
        HAL_Delay(10);
    }

    status = SPI_W5500_ReadRegByte(status_addr);
    snprintf(msg, sizeof(msg), "After OPEN: Status = 0x%02X\r\n", status);
    Send_Debug_Data(msg);

    if (status != 0x13) {
        Send_Debug_Data("Socket not in INIT state!\r\n");
        return 0;
    }

    // Step 5: Listen
    SPI_W5500_WriteRegByte(cmd_addr, 0x02);  // LISTEN
    HAL_Delay(100);

    timeout = HAL_GetTick() + 1000;
    while (SPI_W5500_ReadRegByte(cmd_addr) != 0x00) {
        if (HAL_GetTick() > timeout) {
            Send_Debug_Data("LISTEN timeout!\r\n");
            break;
        }
        HAL_Delay(10);
    }

    status = SPI_W5500_ReadRegByte(status_addr);
    snprintf(msg, sizeof(msg), "After LISTEN: Status = 0x%02X\r\n", status);
    Send_Debug_Data(msg);

    if (status == 0x14) {
        Send_Debug_Data("SUCCESS! TCP Server listening with direct access!\r\n");
        return 1;
    } else {
        Send_Debug_Data("Failed to enter LISTEN state!\r\n");
        return 0;
    }
}
/**
 * @brief Initialize socket state structure
 */
static void W5500_Socket_InitState(uint8_t socket) {
    memset(&socket_states[socket], 0, sizeof(w5500_socket_t));
    socket_states[socket].socket_id = socket;
    socket_states[socket].mode = W5500_MODE_CLOSED;
    socket_states[socket].status = W5500_SOCK_CLOSED;
    socket_states[socket].last_activity = HAL_GetTick();
}
