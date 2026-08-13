#include <string.h>
#include "esp_log.h"
#include "wifi_scan.h"
#include "wifi_mode.h"

static const char *TAG = "wifi_scan";
wifi_mode_t mode;

esp_err_t wifi_scan_start(wifi_scan_result_t *result, uint16_t max_num, uint16_t *count)
{
    if (result == NULL || count == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_AP)
    {
        wifi_mode_config_start();
    }
    /*
        开始扫描
        false:非阻塞扫描
    */
    ESP_ERROR_CHECK(
        esp_wifi_scan_start(
            NULL,
            true));
    uint16_t ap_num = max_num;
    // 获取扫描结果
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_num));
    if (ap_num > max_num)
    {
        ap_num = max_num;
    }
    wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_num);
    if (ap_list == NULL)
    {
        return ESP_ERR_NO_MEM;
    }
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_list));
    for (int i = 0; i < ap_num; i++)
    {
        memset(&result[i], 0, sizeof(wifi_scan_result_t));
        strcpy(result[i].ssid, (char *)ap_list[i].ssid);
        result[i].rssi = ap_list[i].rssi;
        result[i].channel = ap_list[i].primary;
        result[i].authmode = ap_list[i].authmode;
        ESP_LOGI(TAG, "[%d] %s RSSI:%d CH:%d", i, result[i].ssid, result[i].rssi, result[i].channel);
    }
    *count = ap_num;
    free(ap_list);
    return ESP_OK;
}