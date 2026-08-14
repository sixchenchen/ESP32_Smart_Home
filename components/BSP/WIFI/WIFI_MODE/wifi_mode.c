#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "wifi_mode.h"

static const char *TAG = "wifi_mode";

/*
    开启 APSTA 配网模式
    手机:连接 ESP32_CONFIG
    ESP32:同时开启STA等待连接路由器
*/
esp_err_t wifi_mode_config_start(void)
{
    wifi_config_t ap_config =
        {
            .ap =
                {
                    .ssid = AP_SSID,
                    .ssid_len = strlen(AP_SSID),
                    .channel = 1,
                    .max_connection = WIFI_MAX_CONNECTION,
                    .authmode = WIFI_AUTH_WPA_WPA2_PSK,
                    .password = AP_PASSWORD,
                }};
    // 开启APSTA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    // 配置AP
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_LOGI(TAG, "wifi config mode APSTA");
    return ESP_OK;
}

/*
    配置STA连接路由器
*/
esp_err_t wifi_mode_sta_connect(const char *ssid, const char *password)
{
    if (ssid == NULL || strlen(ssid) == 0)
    {
        ESP_LOGE(TAG, "SSID 为空！");
        return ESP_FAIL;
    }
    // 设置 WiFi 配置
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
    if (password != NULL && strlen(password) > 0)
    {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';
    }
    else
    {
        wifi_config.sta.password[0] = '\0';
    }
    ESP_LOGI(TAG, "配置 WiFi: SSID=%s", ssid);
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "设置 WiFi 配置失败: %d", ret);
        return ESP_FAIL;
    }
    ret = esp_wifi_connect();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "WiFi 连接失败: %d", ret);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "正在连接 WiFi: %s", ssid);
    return ESP_OK;
}

/*
    连接成功
    APSTA -> STA
    手机连接ESP32热点断开
*/
esp_err_t wifi_mode_switch_sta(void)
{
    wifi_mode_t mode;
    ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));
    if (mode == WIFI_MODE_APSTA)
    {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_LOGI(TAG, "APSTA -> STA");
    }
    return ESP_OK;
}

/*
    连接失败
    STA -> APSTA
    重新开启手机配网
*/
esp_err_t wifi_mode_switch_apsta(void)
{
    wifi_mode_t mode;
    ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));
    if (mode != WIFI_MODE_APSTA)
    {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_LOGI(TAG, "STA -> APSTA");
    }
    return ESP_OK;
}

/*
    获取当前wifi模式
*/
esp_err_t wifi_mode_get(wifi_mode_t *mode)
{
    if (mode == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_wifi_get_mode(mode);
}