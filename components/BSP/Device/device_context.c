#include "device_context.h"

#include "esp_efuse.h"
#include "esp_mac.h"
#include "esp_log.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "DEVICE_CTX";

static device_context_t ctx;

static bool initialized = false;

esp_err_t device_context_init(void)
{
    if (initialized)
    {
        return ESP_OK;
    }
    memset(&ctx, 0, sizeof(ctx));
    esp_err_t ret;
    ret = esp_efuse_mac_get_default(ctx.base_mac);
    if (ret != ESP_OK)
    {
        return ret;
    }

    snprintf(ctx.device_id, sizeof(ctx.device_id), "%02X%02X%02X%02X%02X%02X",
             ctx.base_mac[0],
             ctx.base_mac[1],
             ctx.base_mac[2],
             ctx.base_mac[3],
             ctx.base_mac[4],
             ctx.base_mac[5]);
    strcpy(ctx.device_name, DEVICE_NAME);
    strcpy(ctx.product_id, "MOS_CONTROLLER");
    strcpy(ctx.hardware_version, HARDWARE_VERSION);
    strcpy(ctx.firmware_version, FIRMWARE_VERSION);
    esp_read_mac(ctx.sta_mac, ESP_MAC_WIFI_STA);
    esp_read_mac(ctx.ap_mac, ESP_MAC_WIFI_SOFTAP);
    esp_read_mac(ctx.bt_mac, ESP_MAC_BT);
    initialized = true;
    ESP_LOGI(TAG, "device id=%s", ctx.device_id);
    return ESP_OK;
}

const device_context_t *device_context_get(void)
{

    if (!initialized)
    {
        device_context_init();
    }

    return &ctx;
}
