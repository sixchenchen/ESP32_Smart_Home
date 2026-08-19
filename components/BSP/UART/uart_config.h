#ifndef UART_CONFIG_H
#define UART_CONFIG_H

#include "driver/uart.h"

// ==================== UART 硬件配置 ====================
#define UART_PORT_NUM UART_NUM_2
#define UART_BAUD_RATE 115200
#define UART_TX_GPIO GPIO_NUM_17
#define UART_RX_GPIO GPIO_NUM_16
#define UART_BUF_SIZE 1024

#endif