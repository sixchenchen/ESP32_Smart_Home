#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "esp_err.h"
#include <stdint.h>

typedef enum
{
    MQTT_STATUS_CONNECTED,
    MQTT_STATUS_DISCONNECTED
} mqtt_status_t;

/*
    注册接收回调
    MQTT收到数据后调用
*/
typedef void (*mqtt_rx_callback_t)(
    const char *topic,
    const uint8_t *data,
    int len);

typedef void (*mqtt_status_callback_t)(mqtt_status_t status);
/*
    MQTT初始化
*/
esp_err_t mqtt_manager_init(void);

/*
    MQTT启动
*/
esp_err_t mqtt_manager_start(void);

/*
    发布消息
*/
esp_err_t mqtt_manager_publish(const char *topic, const char *data, int len);

void mqtt_manager_register_callback(mqtt_rx_callback_t callback);
void mqtt_manager_register_status_callback(mqtt_status_callback_t callback);

#endif