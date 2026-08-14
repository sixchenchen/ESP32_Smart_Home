#ifndef __UART_DRV_H__
#define __UART_DRV_H__

#include "esp_err.h"
#include <stdint.h>

#define UART_PORT_NUM UART_NUM_1
#define UART_TX_GPIO GPIO_NUM_17
#define UART_RX_GPIO GPIO_NUM_16

#define UART_BUF_SIZE 1024
#define UART_BAUD_RATE 115200

typedef void (*uart_rx_callback_t)(uint8_t data);

esp_err_t uart_drv_init(void);

esp_err_t uart_drv_send(uint8_t *data, uint16_t len);

void uart_drv_register_callback(uart_rx_callback_t callback);

#endif