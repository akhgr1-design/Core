#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "w5500_driver.h"
#include <stdint.h>

#define TCP_SERVER_PORT     5000
#define TCP_BUFFER_SIZE     128
#define SOCKET_TCP_SERVER   0

// Externals (from main.c)
extern uint16_t sensor_temp;
extern uint16_t sensor_press;
extern uint16_t sensor_flow;
extern uint16_t sensor_level;

// Functions
void TCP_Server_Init(void);
void TCP_Server_Process(void);

#endif /* TCP_SERVER_H */
