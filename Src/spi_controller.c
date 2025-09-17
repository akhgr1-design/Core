#include "spi_controller.h"
#include "utilities.h"
#include "uart_comm.h"
#include "main.h"

static SPI_HandleTypeDef *hspi;
static GPIO_TypeDef* cs_port;
static uint16_t cs_pin;

void spi_init(SPI_HandleTypeDef *hspi_instance)
{
    hspi = hspi_instance;

    // Configure CS pin (update these based on your actual configuration)
    cs_port = GPIOE;           // Replace with your CS port (e.g., GPIOE)
    cs_pin = GPIO_PIN_11;      // Replace with your CS pin (e.g., GPIO_PIN_11)
    // Set CS high (deselected)
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

    Send_Debug_Data("SPI Controller initialized\r\n");
}

void spi_cs_select(void)
{
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
    HAL_Delay(1); // Critical delay for W5500
}

void spi_cs_deselect(void)
{
    HAL_Delay(1); // Critical delay for W5500
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
}

uint8_t spi_transfer(uint8_t data)
{
    uint8_t rx_data;
    HAL_SPI_TransmitReceive(hspi, &data, &rx_data, 1, SPI_TIMEOUT);
    return rx_data;
}

void spi_write(uint8_t *data, uint16_t size)
{
    HAL_SPI_Transmit(hspi, data, size, SPI_TIMEOUT);
}

void spi_read(uint8_t *data, uint16_t size)
{
    HAL_SPI_Receive(hspi, data, size, SPI_TIMEOUT);
}

// W5500 Specific functions
void SPI_WriteRegByte(uint32_t reg, uint8_t value)
{
    uint8_t tx_buf[4];

    // W5500 SPI format: 0b0000 FAAA AAAA AAAA
    tx_buf[0] = (reg >> 8) & 0x1F;  // Address high (5 bits)
    tx_buf[1] = reg & 0xFF;         // Address low (8 bits)
    tx_buf[2] = value;              // Data to write

    spi_cs_select();
    spi_write(tx_buf, 3);
    spi_cs_deselect();
}

uint8_t SPI_ReadRegByte(uint32_t reg)
{
    uint8_t tx_buf[4] = {0};
    uint8_t rx_buf[4] = {0};

    // W5500 SPI format: 0b0000 FAAA AAAA AAAA + read flag (0x08)
    tx_buf[0] = ((reg >> 8) & 0x1F) | 0x08;  // Address high + read flag
    tx_buf[1] = reg & 0xFF;                  // Address low
    tx_buf[2] = 0x00;                        // Dummy byte for reading

    spi_cs_select();
    HAL_SPI_TransmitReceive(hspi, tx_buf, rx_buf, 3, SPI_TIMEOUT);
    spi_cs_deselect();

    return rx_buf[2];
}

void SPI_WriteRegBlock(uint32_t reg, uint8_t *data, uint16_t size)
{
    uint8_t tx_buf[2];

    // Write address
    tx_buf[0] = (reg >> 8) & 0x1F;  // Address high (5 bits)
    tx_buf[1] = reg & 0xFF;         // Address low (8 bits)

    spi_cs_select();
    spi_write(tx_buf, 2);
    spi_write(data, size);
    spi_cs_deselect();
}

void SPI_ReadRegBlock(uint32_t reg, uint8_t *data, uint16_t size)
{
    uint8_t tx_buf[2];

    // Read address + read flag
    tx_buf[0] = ((reg >> 8) & 0x1F) | 0x08;  // Address high + read flag
    tx_buf[1] = reg & 0xFF;                  // Address low

    spi_cs_select();
    spi_write(tx_buf, 2);
    spi_read(data, size);
    spi_cs_deselect();
}

uint8_t spi_check_errors(void)
{
    return (hspi->ErrorCode != HAL_SPI_ERROR_NONE);
}

void spi_reset(void)
{
    HAL_SPI_DeInit(hspi);
    HAL_Delay(10);
    HAL_SPI_Init(hspi);
    Send_Debug_Data("SPI reset completed\r\n");
}
