#include "sensor_mqtt_bridge.h"
#include "sensor_manager.h"
#include "mqtt_manager.h"
#include "mqtt_message.h"
#include "mqtt_topic.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "SENSOR_MQTT";

static batch_cache_t s_batch_cache = {0};

static void pack_sensor_data(const sensor_data_t *src, uint8_t *dst)
{
    dst[0] = src->sensor_id;
    dst[1] = (src->timestamp_ms >> 0) & 0xFF;
    dst[2] = (src->timestamp_ms >> 8) & 0xFF;
    dst[3] = (src->timestamp_ms >> 16) & 0xFF;
    dst[4] = (src->timestamp_ms >> 24) & 0xFF;
    dst[5] = (src->count >> 0) & 0xFF;
    dst[6] = (src->count >> 8) & 0xFF;
}

static void flush_batch(void)
{
    if (s_batch_cache.count == 0)
    {
        return;
    }

    ESP_LOGI(TAG, "Flushing batch: %d items", s_batch_cache.count);

    if (!mqtt_manager_is_running())
    {
        ESP_LOGW(TAG, "MQTT not running, batch dropped");
        s_batch_cache.count = 0;
        s_batch_cache.first_time_ms = 0;
        return;
    }

    uint8_t packed_data[BATCH_MAX_COUNT * ITEM_SIZE];
    for (uint8_t i = 0; i < s_batch_cache.count; i++)
    {
        pack_sensor_data(&s_batch_cache.data[i], &packed_data[i * ITEM_SIZE]);
    }

    char *json = mqtt_message_create_sensor_batch(packed_data, s_batch_cache.count);

    if (json != NULL)
    {
        esp_err_t ret = mqtt_manager_publish(
            mqtt_topic_sensor(),
            json,
            strlen(json),
            1,
            false);

        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "Published batch: %d sensors", s_batch_cache.count);
        }
        else
        {
            ESP_LOGE(TAG, "Batch publish failed: %d", ret);
        }
        free(json);
    }

    s_batch_cache.count = 0;
    s_batch_cache.first_time_ms = 0;
}

static void add_to_batch(const sensor_data_t *data)
{
    if (s_batch_cache.count >= BATCH_MAX_COUNT)
    {
        ESP_LOGW(TAG, "Batch cache full, flushing...");
        flush_batch();
        add_to_batch(data);
        return;
    }

    if (s_batch_cache.count == 0)
    {
        s_batch_cache.first_time_ms = esp_timer_get_time() / 1000;
    }

    memcpy(&s_batch_cache.data[s_batch_cache.count], data, sizeof(sensor_data_t));
    s_batch_cache.count++;

    ESP_LOGD(TAG, "Added to batch: count=%d, id=%d",
             s_batch_cache.count, data->sensor_id);
}

// ✅ 这个回调函数被 sensor_manager 通过函数指针调用
static void sensor_data_callback(const sensor_data_t *data, void *ctx)
{
    if (data == NULL)
    {
        return;
    }

    ESP_LOGI(TAG, "Received: id=%d, ts=%lu, count=%d",
             data->sensor_id, data->timestamp_ms, data->count);

    if (!mqtt_manager_is_running())
    {
        ESP_LOGW(TAG, "MQTT not running, data cached in sensor_manager");
        return;
    }

    add_to_batch(data);

    if (s_batch_cache.count >= BATCH_MAX_COUNT)
    {
        flush_batch();
    }
}

void sensor_mqtt_bridge_poll(uint32_t now_ms)
{
    if (s_batch_cache.count == 0)
    {
        return;
    }

    if ((now_ms - s_batch_cache.first_time_ms) >= BATCH_TIMEOUT_MS)
    {
        ESP_LOGI(TAG, "Batch timeout, flushing...");
        flush_batch();
    }
}

esp_err_t sensor_mqtt_bridge_init(void)
{
    memset(&s_batch_cache, 0, sizeof(s_batch_cache));
    sensor_manager_set_data_callback(sensor_data_callback, NULL);
    ESP_LOGI(TAG, "Sensor MQTT bridge initialized (batch mode)");
    return ESP_OK;
}