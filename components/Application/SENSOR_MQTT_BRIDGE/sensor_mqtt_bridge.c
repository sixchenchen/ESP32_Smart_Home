#include "sensor_mqtt_bridge.h"

#include "sensor_manager.h"
#include "mqtt_manager.h"
#include "mqtt_message.h"
#include "mqtt_topic.h"
#include "esp_log.h"

static const char *TAG = "SENSOR_MQTT";

/*
    GD32 单条数据回调 → 通过 MQTT 上报
*/
static void sensor_data_callback(const sensor_data_t *data, void *ctx)
{
    if (data == NULL)
    {
        return;
    }

    ESP_LOGI(TAG, "Sensor data: id=%d, ts=%lu, count=%d",
             data->sensor_id, data->timestamp_ms, data->count);

    // 检查 MQTT 是否运行
    if (!mqtt_manager_is_running())
    {
        ESP_LOGW(TAG, "MQTT not running, data will be cached in sensor_manager");
        return;
    }

    // 创建 JSON 消息
    char *json = mqtt_message_create_sensor_data(
        data->sensor_id,
        data->timestamp_ms,
        data->count);

    if (json == NULL)
    {
        ESP_LOGE(TAG, "JSON create failed");
        return;
    }

    // 发布到 MQTT
    esp_err_t ret = mqtt_manager_publish(
        mqtt_topic_sensor(), // 主题: device/{id}/sensor
        json,                // 消息内容
        strlen(json),        // 长度
        1,                   // QoS 1
        false                // 不保留
    );

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Published sensor data: id=%d", data->sensor_id);
    }
    else
    {
        ESP_LOGE(TAG, "Publish failed: %d", ret);
    }

    free(json);
}

esp_err_t sensor_mqtt_bridge_init(void)
{
    sensor_manager_set_data_callback(sensor_data_callback, NULL);
    ESP_LOGI(TAG, "Sensor MQTT bridge initialized");
    return ESP_OK;
}