#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "wifi_config.h"

static const char *TAG = "wifi_config";

/*
    保存WiFi
*/
esp_err_t wifi_config_save(const char *ssid, const char *password)
{
    nvs_handle_t handle;
    esp_err_t ret;
    ret = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK)
    {
        return ret;
    }
    ret = nvs_set_str(handle, WIFI_SSID_KEY, ssid);
    if (ret != ESP_OK)
    {
        nvs_close(handle);
        return ret;
    }
    ret = nvs_set_str(handle, WIFI_PASS_KEY, password);
    if (ret != ESP_OK)
    {
        nvs_close(handle);
        return ret;
    }
    /*
        提交保存
    */
    ret = nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "wifi config saved");
    return ret;
}

/*
    读取WiFi
*/

esp_err_t wifi_config_load(char *ssid, char *password)
{
    nvs_handle_t handle;
    esp_err_t ret;
    ret = nvs_open(WIFI_NAMESPACE, NVS_READONLY, &handle);

    if (ret != ESP_OK)
    {
        return ret;
    }
    size_t ssid_len = 32;
    size_t pass_len = 64;
    ret = nvs_get_str(handle, WIFI_SSID_KEY, ssid, &ssid_len);
    if (ret != ESP_OK)
    {
        nvs_close(handle);
        return ret;
    }
    ret = nvs_get_str(handle, WIFI_PASS_KEY, password, &pass_len);
    nvs_close(handle);
    return ret;
}

/*
    删除配置
*/
esp_err_t wifi_config_clear(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK)
    {
        return ret;
    }
    nvs_erase_key(handle, WIFI_SSID_KEY);
    nvs_erase_key(handle, WIFI_PASS_KEY);
    ret = nvs_commit(handle);
    nvs_close(handle);
    return ret;
}
