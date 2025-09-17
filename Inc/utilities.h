#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdint.h>
#include <string.h>

void Utilities_Delay(uint32_t ms);
uint32_t Utilities_GetTick(void);
void Utilities_FormatIP(char *buffer, uint8_t *ip);
void Utilities_FormatMAC(char *buffer, uint8_t *mac);

#endif /* UTILITIES_H */
