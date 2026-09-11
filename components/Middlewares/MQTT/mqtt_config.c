#include "mqtt_config.h"
#include "device_context.h"
#include "mqtt_topic.h"
#include "nvs_utils.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "mqtt_config";

// ========== 默认临时服务器配置（首次注册用） ==========
#define DEFAULT_BROKER_URI "mqtt://192.168.124.6:1884"
#define DEFAULT_USERNAME "MQTT1"
#define DEFAULT_PASSWORD "123456"

// ========== 获取默认配置 ==========
void mqtt_config_get_default(mqtt_config_t *config)
{
    if (config == NULL)
        return;

    memset(config, 0, sizeof(mqtt_config_t));
    const device_context_t *dev = device_context_get();

    // 临时服务器配置
    strlcpy(config->broker_uri, DEFAULT_BROKER_URI, sizeof(config->broker_uri));
    strlcpy(config->username, DEFAULT_USERNAME, sizeof(config->username));
    strlcpy(config->password, DEFAULT_PASSWORD, sizeof(config->password));
    strlcpy(config->client_id, dev->device_id, sizeof(config->client_id));
    strlcpy(config->will_topic, mqtt_topic_will(), sizeof(mqtt_topic_will()));
    config->is_provisioned = false;
    ESP_LOGI(TAG, "Default config: broker=%s, client_id=%s", config->broker_uri, config->client_id);
}

// ========== 加载配置 ==========
esp_err_t mqtt_config_load(mqtt_config_t *config)
{
    if (config == NULL) // 这里面判断的是地址
        return ESP_ERR_INVALID_ARG;

    memset(config, 0, sizeof(mqtt_config_t));

    size_t len;
    esp_err_t ret;

    // 1. broker_uri
    len = sizeof(config->broker_uri);
    ret = nvs_utils_load_str(MQTT_CONFIG_NAMESPACE, MQTT_KEY_BROKER_URI, config->broker_uri, &len);
    if (ret != ESP_OK)
    {
        strlcpy(config->broker_uri, DEFAULT_BROKER_URI, sizeof(config->broker_uri));
    }

    // 2. client_id
    len = sizeof(config->client_id);
    ret = nvs_utils_load_str(MQTT_CONFIG_NAMESPACE, MQTT_KEY_CLIENT_ID, config->client_id, &len);
    if (ret != ESP_OK)
    {
        const device_context_t *dev = device_context_get();
        strlcpy(config->client_id, dev->device_id, sizeof(config->client_id));
    }

    // 3. username
    len = sizeof(config->username);
    ret = nvs_utils_load_str(MQTT_CONFIG_NAMESPACE, MQTT_KEY_USERNAME, config->username, &len);
    if (ret != ESP_OK)
    {
        strlcpy(config->username, DEFAULT_USERNAME, sizeof(config->username));
    }

    // 4. password
    len = sizeof(config->password);
    ret = nvs_utils_load_str(MQTT_CONFIG_NAMESPACE, MQTT_KEY_PASSWORD, config->password, &len);
    if (ret != ESP_OK)
    {
        strlcpy(config->password, DEFAULT_PASSWORD, sizeof(config->password));
    }

    // 5. will_topic
    len = sizeof(config->will_topic);
    ret = nvs_utils_load_str(MQTT_CONFIG_NAMESPACE, MQTT_KEY_WILL_TOPIC, config->will_topic, &len);
    if (ret != ESP_OK)
    {
        strlcpy(config->will_topic, mqtt_topic_will(), sizeof(mqtt_topic_will()));
    }

    // 6. is_provisioned
    uint8_t provisioned = 0;
    ret = nvs_utils_load_u8(MQTT_CONFIG_NAMESPACE, MQTT_KEY_PROVISIONED, &provisioned);
    config->is_provisioned = (ret == ESP_OK && provisioned == 1);

    ESP_LOGI(TAG, "Config loaded: broker=%s, client_id=%s, provisioned=%d", config->broker_uri, config->client_id, config->is_provisioned);
    return ESP_OK;
}

// ========== 保存配置 ==========
esp_err_t mqtt_config_save(const mqtt_config_t *config)
{
    if (config == NULL)
        return ESP_ERR_INVALID_ARG;

    esp_err_t ret;

    ret = nvs_utils_save_str(MQTT_CONFIG_NAMESPACE, MQTT_KEY_BROKER_URI, config->broker_uri);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save broker_uri");
        return ret;
    }

    ret = nvs_utils_save_str(MQTT_CONFIG_NAMESPACE, MQTT_KEY_CLIENT_ID, config->client_id);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save client_id");
        return ret;
    }

    ret = nvs_utils_save_str(MQTT_CONFIG_NAMESPACE, MQTT_KEY_USERNAME, config->username);
    if (ret != ESP_OK)
        return ret;

    ret = nvs_utils_save_str(MQTT_CONFIG_NAMESPACE, MQTT_KEY_PASSWORD, config->password);
    if (ret != ESP_OK)
        return ret;

    ret = nvs_utils_save_str(MQTT_CONFIG_NAMESPACE, MQTT_KEY_WILL_TOPIC, config->will_topic);
    if (ret != ESP_OK)
        return ret;

    ret = nvs_utils_save_u8(MQTT_CONFIG_NAMESPACE, MQTT_KEY_PROVISIONED, config->is_provisioned ? 1 : 0);
    if (ret != ESP_OK)
        return ret;

    ESP_LOGI(TAG, "Config saved: broker=%s, provisioned=%d", config->broker_uri, config->is_provisioned);
    return ESP_OK;
}

// ========== 检查是否已注册 ==========
bool mqtt_config_is_provisioned(void)
{
    mqtt_config_t config;
    if (mqtt_config_load(&config) == ESP_OK)
    {
        return config.is_provisioned;
    }
    return false;
}

// ========== 清除配置 ==========
esp_err_t mqtt_config_clear(void)
{
    esp_err_t ret = nvs_utils_erase_all(MQTT_CONFIG_NAMESPACE);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "MQTT config cleared");
    }
    else
    {
        ESP_LOGW(TAG, "Failed to clear MQTT config: %d", ret);
    }
    return ret;
}