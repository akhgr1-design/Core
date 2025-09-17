#ifndef W5500_DIAGNOSTICS_H
#define W5500_DIAGNOSTICS_H

#include "main.h"
#include "spi.h"

// Function prototypes
void W5500_Test_CS(void);
void W5500_Test_Reset(void);
uint8_t W5500_Test_SPI_Modes(void);

#endif /* W5500_DIAGNOSTICS_H */
