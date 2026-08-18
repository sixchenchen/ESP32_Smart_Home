#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

// MQTT 状态枚举
typedef enum
{
    MQTT_STATE_UNINIT = 0,
    MQTT_STATE_INIT,
    MQTT_STATE_STARTING,
    MQTT_STATE_RUNNING,
    MQTT_STATE_STOPPING,
    MQTT_STATE_STOPPED,
    MQTT_STATE_ERROR,
} mqtt_state_t;

// 回调函数类型
typedef void (*mqtt_rx_callback_t)(const char *topic, const uint8_t *data, int len);
typedef void (*mqtt_status_callback_t)(mqtt_state_t state);

// 生命周期管理
esp_err_t mqtt_manager_init(void);
esp_err_t mqtt_manager_start(void);
esp_err_t mqtt_manager_stop(void);
esp_err_t mqtt_manager_destroy(void);

// 状态查询
mqtt_state_t mqtt_manager_get_state(void);
const char *mqtt_manager_get_state_string(void);
bool mqtt_manager_is_running(void);

esp_err_t mqtt_manager_subscribe(const char *topic, int qos);

// WiFi 状态驱动
void mqtt_manager_on_wifi_connected(void);
void mqtt_manager_on_wifi_disconnected(void);

// 发布消息
esp_err_t mqtt_manager_publish(const char *topic, const char *data, int len, int qos, bool retain);

// 回调注册
void mqtt_manager_register_callback(mqtt_rx_callback_t callback);
void mqtt_manager_register_status_callback(mqtt_status_callback_t callback);

#endif