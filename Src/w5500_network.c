/*
#include "w5500_network.h"
#include "w5500_conf.h"
#include "w5500_platform.h"
//#include "wizchip_conf.h"
#include <stdio.h>  // For printf()

// Network configuration
// static uint8_t mac[6] = W5500_MAC_ADDR;
// static uint8_t ip[4] = W5500_IP_ADDR;
// static uint8_t sn[4] = W5500_SUBNET;
// static uint8_t gw[4] = W5500_GATEWAY;

// Error handler weak override
__weak void Error_Handler(void) {
    while(1) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_2);  // Toggle ERR_LED
        HAL_Delay(500);
    }
}

void W5500_Network_Init(void) {
    W5500_Platform_Init();
    
    // Register callbacks with correct signatures
    reg_wizchip_cs_cbfunc(W5500_CS_Select, W5500_CS_Deselect);
    reg_wizchip_spi_cbfunc(W5500_SPI_ReadByte, W5500_SPI_WriteByte);
    
    // Initialize Wizchip
    uint8_t txsize[8] = {2, 2, 2, 2, 2, 2, 2, 2};
    uint8_t rxsize[8] = {2, 2, 2, 2, 2, 2, 2, 2};
    if (wizchip_init(txsize, rxsize) != WIZ_OK) {
        Error_Handler();
    }
    
    // Create network info structure
    wiz_NetInfo netInfo = {
        .mac = W5500_MAC_ADDR,
        .ip = W5500_IP_ADDR,
        .sn = W5500_SUBNET,
        .gw = W5500_GATEWAY,
        .dns = {8,8,8,8},  // Google DNS
        .dhcp = NETINFO_STATIC  // Static IP mode
    };
    
    // Set network parameters
    wizchip_setnetinfo(&netInfo);
}

NetworkStatus_t W5500_GetNetworkStatus(void) {
    uint8_t linkStatus;
    ctlwizchip(CW_GET_LINK_STATUS, &linkStatus);
    return (linkStatus == PHY_LINK_ON) ? NET_STATUS_CONNECTED : NET_STATUS_DISCONNECTED;
}

// Additional Socket and Network Functions would be implemented here
*/
