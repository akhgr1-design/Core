#ifndef __W5500_NETWORK_H
#define __W5500_NETWORK_H

#include <stdint.h>

typedef enum {
    NET_STATUS_DISCONNECTED = 0,
    NET_STATUS_CONNECTED = 1
} NetworkStatus_t;

// Function prototypes
void W5500_Network_Init(void);
NetworkStatus_t W5500_GetNetworkStatus(void);

// Socket Management
int8_t W5500_SocketOpen(uint8_t sock, uint8_t protocol, uint16_t port);
void W5500_SocketClose(uint8_t sock);
int8_t W5500_SocketConnect(uint8_t sock, uint8_t* destIP, uint16_t destPort);
int16_t W5500_SocketSend(uint8_t sock, const uint8_t* data, uint16_t size);
int16_t W5500_SocketReceive(uint8_t sock, uint8_t* data, uint16_t size);

// Diagnostic Functions
void W5500_PrintNetworkInfo(void);

#endif /* __W5500_NETWORK_H */
