#ifndef __WIFI_SCAN_H__
#define __WIFI_SCAN_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"

typedef struct
{
    char ssid[33];
    uint8_t rssi;
    uint8_t channel;
    wifi_auth_mode_t authmode;
} wifi_scan_result_t;

esp_err_t wifi_scan_start(wifi_scan_result_t *result, uint16_t max_num, uint16_t *count);

#endif // !