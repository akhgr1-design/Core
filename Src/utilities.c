#include "utilities.h"
#include "main.h"
#include <stdio.h>  // ADDED for snprintf

void Utilities_Delay(uint32_t ms) {
    HAL_Delay(ms);
}

uint32_t Utilities_GetTick(void) {
    return HAL_GetTick();
}

void Utilities_FormatIP(char *buffer, uint8_t *ip) {
    snprintf(buffer, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

void Utilities_FormatMAC(char *buffer, uint8_t *mac) {
    snprintf(buffer, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
