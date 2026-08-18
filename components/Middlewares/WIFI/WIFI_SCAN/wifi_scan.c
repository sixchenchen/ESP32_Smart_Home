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
    wifi_mode_t current_mode;
    esp_wifi_get_mode(&current_mode);
    bool need_switch = false;
    if (current_mode == WIFI_MODE_AP)
    {
        // 不能直接用 AP 模式扫描，需要先切换到 STA
        wifi_mode_config_start();
        need_switch = true;
        vTaskDelay(pdMS_TO_TICKS(100)); // 等待模式切换
    }
    // 开始扫描, false:非阻塞扫描
    esp_err_t ret = esp_wifi_scan_start(NULL, true);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "扫描启动失败: %d", ret);
        return ret;
    };
    uint16_t ap_num = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_num));
    if (ap_num > max_num)
    {
        ap_num = max_num;
    }
    // 获取扫描结果
    wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_num);
    if (ap_list == NULL)
    {
        return ESP_ERR_NO_MEM;
    }
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_list));
    for (int i = 0; i < ap_num; i++)
    {
        memset(&result[i], 0, sizeof(wifi_scan_result_t));
        strncpy(result[i].ssid, (char *)ap_list[i].ssid, sizeof(result[i].ssid) - 1);
        result[i].ssid[sizeof(result[i].ssid) - 1] = '\0';
        result[i].rssi = ap_list[i].rssi;
        result[i].channel = ap_list[i].primary;
        result[i].authmode = ap_list[i].authmode;
        ESP_LOGI(TAG, "[%d] %s RSSI:%d CH:%d", i, result[i].ssid, result[i].rssi, result[i].channel);
    }
    *count = ap_num;
    free(ap_list);
    return ESP_OK;
}