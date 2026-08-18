#ifndef __WIFI_MODE_H__
#define __WIFI_MODE_H__

#include "esp_err.h"
#include "esp_wifi.h"

#define AP_SSID "ESP32_CONFIG"
#define AP_PASSWORD "12345678"
#define WIFI_MAX_CONNECTION 4

// 定义回调函数类型
typedef void (*wifi_connected_cb_t)(const char *ssid, const char *password);

/*
    开启配网模式
    AP: ESP32_CONFIG
    STA:等待连接路由器
*/
esp_err_t wifi_mode_config_start(void);

/*
    STA连接路由器
*/
esp_err_t wifi_mode_sta_connect(const char *ssid, const char *password);

/*
    APSTA -> STA，连接成功后关闭AP
*/
esp_err_t wifi_mode_switch_sta(void);

/*
    STA -> APSTA,连接失败重新进入配置模式
*/
esp_err_t wifi_mode_switch_apsta(void);

/*
    获取当前ESP32 wifi模式
*/
esp_err_t wifi_mode_get(wifi_mode_t *mode);
#endif