#ifndef __WIFI_CONFIG_H__
#define __WIFI_CONFIG_H__

#define WIFI_NAMESPACE "wifi"
#define WIFI_SSID_KEY "ssid"
#define WIFI_PASS_KEY "password"

/*
    保存WiFi
*/
esp_err_t wifi_config_save(const char *ssid, const char *password);

/*
    读取WiFi
    返回: ESP_OK 成功
*/
esp_err_t wifi_config_load(char *ssid, char *password);

/*
    删除WiFi配置
*/
esp_err_t wifi_config_clear(void);

#endif