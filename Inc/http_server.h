// http_server.h
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "main.h"
#include "w5500_driver.h"

#define HTTP_PORT           80
#define HTTP_SOCKET         0
#define BUFFER_SIZE         512

extern uint16_t sensor_temp;
extern uint16_t sensor_press;
extern uint16_t sensor_flow;
extern uint16_t sensor_level;
extern uint8_t di_status[8];
extern uint8_t do_status[8];

void HTTP_Server_Init(void);
void HTTP_Server_Process(void);
void HTTP_SendResponse(uint8_t sn);
void HTTP_HandleRequest(uint8_t sn, char* request);

#endif
