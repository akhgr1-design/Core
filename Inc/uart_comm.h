#ifndef UART_COMM_H
#define UART_COMM_H

#include <stdint.h>

// Function prototypes
void Init_UARTs(void);
void Send_Debug_Data(const char *message);
void Send_HMI_Data(const char *message);
void Send_RS485_Data(const char *message);

// DE Pin definitions
#define U485_DE_RE_PORT     GPIOD
#define U485_DE_RE_PIN      GPIO_PIN_5

#define DEBUG_DE_RE_PORT    GPIOE
#define DEBUG_DE_RE_PIN     GPIO_PIN_8

#endif /* UART_COMM_H */