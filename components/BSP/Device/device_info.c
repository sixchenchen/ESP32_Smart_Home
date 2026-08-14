#include "device_info.h"
#include "esp_efuse.h"
#include "esp_log.h"
#include "esp_mac.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "DEVICE";
static char s_device_id[DEVICE_ID_LEN] = {0};

static void device_init_id(void)
{
    if (s_device_id[0] != '\0')
    {
        return;
    }

    uint8_t mac[6];

    // ✅ 方法1：获取基准 MAC（推荐用于设备 ID）
    esp_err_t ret = esp_efuse_mac_get_default(mac);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get MAC: %d", ret);
        strcpy(s_device_id, "UNKNOWN_ID");
        return;
    }

    snprintf(s_device_id, sizeof(s_device_id), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

const char *device_get_id(void)
{
    device_init_id();
    return s_device_id;
}

void device_print_info(void)
{
    uint8_t base_mac[6];
    uint8_t sta_mac[6];
    uint8_t ap_mac[6];
    uint8_t bt_mac[6];
    esp_efuse_mac_get_default(base_mac);
    esp_read_mac(sta_mac, ESP_MAC_WIFI_STA);
    esp_read_mac(ap_mac, ESP_MAC_WIFI_SOFTAP);
    esp_read_mac(bt_mac, ESP_MAC_BT);

    ESP_LOGI(TAG, "===== MAC Addresses =====");
    ESP_LOGI(TAG, "Base MAC:     %02X:%02X:%02X:%02X:%02X:%02X",
             base_mac[0], base_mac[1], base_mac[2],
             base_mac[3], base_mac[4], base_mac[5]);
    ESP_LOGI(TAG, "Wi-Fi STA:    %02X:%02X:%02X:%02X:%02X:%02X",
             sta_mac[0], sta_mac[1], sta_mac[2],
             sta_mac[3], sta_mac[4], sta_mac[5]);
    ESP_LOGI(TAG, "Wi-Fi AP:     %02X:%02X:%02X:%02X:%02X:%02X",
             ap_mac[0], ap_mac[1], ap_mac[2],
             ap_mac[3], ap_mac[4], ap_mac[5]);
    ESP_LOGI(TAG, "Bluetooth:    %02X:%02X:%02X:%02X:%02X:%02X",
             bt_mac[0], bt_mac[1], bt_mac[2],
             bt_mac[3], bt_mac[4], bt_mac[5]);
    ESP_LOGI(TAG, "Device ID:    %s", device_get_id());
    ESP_LOGI(TAG, "========================");
}
const char *device_get_name(void)
{
    return "ESP32_Smart_Home"; // 或者从 NVS / 配置中读取
}