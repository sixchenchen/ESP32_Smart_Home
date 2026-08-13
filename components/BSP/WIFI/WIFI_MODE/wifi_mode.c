#include <string.h>

#include "esp_wifi.h"
#include "esp_log.h"

#include "wifi_mode.h"

static const char *TAG = "wifi_mode";

#define AP_SSID "ESP32_CONFIG"
#define AP_PASSWORD "12345678"

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
                    .max_connection = 4,
                    .authmode = WIFI_AUTH_WPA_WPA2_PSK,
                    .password = AP_PASSWORD,
                }};

    /*
        开启APSTA
    */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    /*
        配置AP
    */
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    ESP_LOGI(TAG, "wifi config mode APSTA");

    return ESP_OK;
}

/*
    配置STA连接路由器
*/
esp_err_t wifi_mode_sta_connect(
    const char *ssid,
    const char *password)
{
    wifi_config_t sta_config = {0};
    strlcpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid));
    strlcpy((char *)sta_config.sta.password, password, sizeof(sta_config.sta.password));

    /*
        配置STA
    */
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    /*
        开始连接
    */
    ESP_ERROR_CHECK(esp_wifi_connect());
    ESP_LOGI(TAG, "STA connecting %s", ssid);
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
        ESP_LOGI(TAG, "close AP");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
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
        ESP_LOGI(TAG, "enable APSTA");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
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