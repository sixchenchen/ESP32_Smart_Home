#ifndef UART_DRV_H
#define UART_DRV_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

// 接收回调函数类型
typedef void (*uart_rx_callback_t)(const uint8_t *data, uint16_t len);
typedef void (*uart_rx_mos_byte_callback_t)(uint8_t data);
typedef void (*uart_rx_sen_byte_callback_t)(uint8_t data);

/**
 * @brief 初始化 UART 驱动
 * @return ESP_OK 成功，其他失败
 */
esp_err_t uart_drv_init(void);

/**
 * @brief 发送数据
 * @param data 数据指针
 * @param len 数据长度
 * @return ESP_OK 成功，其他失败
 */
esp_err_t uart_drv_send(const uint8_t *data, uint16_t len);

/**
 * @brief 注册批量接收回调
 * @param callback 回调函数（每次接收完整数据包时调用）
 */
void uart_drv_register_callback(uart_rx_callback_t callback);

/**
 * @brief 注册逐字节接收回调
 * @param callback 回调函数（每次接收一个字节时调用）
 */
void uart_drv_register_mos_byte_callback(uart_rx_mos_byte_callback_t callback);

/**
 * @brief 注册光栅传感器回调
 * @param callback 回调函数
 */
void uart_drv_register_sen_byte_callback(uart_rx_sen_byte_callback_t callback);

#endif