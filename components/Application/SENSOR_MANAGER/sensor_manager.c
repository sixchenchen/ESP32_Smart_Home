#include "sensor_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "SENSOR_MANAGER";

static SemaphoreHandle_t s_mutex = NULL;

static sensor_data_t s_cache[SENSOR_CACHE_SIZE];
static uint8_t s_cache_count = 0;
static uint16_t s_cache_head = 0;
static uint16_t s_cache_tail = 0;
static sensor_data_callback_t s_data_callback = NULL;
static void *s_ctx_callback = NULL;
static uint32_t s_last_poll_ms = 0;

static void sensor_manager_on_protocol_event(const sen_protocol_event_t *event, void *user_ctx);
static bool sensor_manager_lock(void);
static void sensor_manager_unlock(void);

static bool sensor_manager_lock(void)
{
    if (s_mutex == NULL)
    {
        return true;
    }
    return (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE);
}

static void sensor_manager_unlock(void)
{
    if (s_mutex != NULL)
    {
        xSemaphoreGive(s_mutex);
    }
}

static void sensor_manager_cache_data(const sen_protocol_event_t *event)
{
    // 检查数据长度：sensor_id(1) + timestamp(4) + count(2) = 7
    if (event->len < 7)
    {
        ESP_LOGW(TAG, "Invalid sensor data length: %d", event->len);
        return;
    }

    uint16_t next_head = (s_cache_head + 1) % SENSOR_CACHE_SIZE;
    if (next_head == s_cache_tail)
    {
        ESP_LOGW(TAG, "Sensor cache full, dropping oldest");
        s_cache_tail = (s_cache_tail + 1) % SENSOR_CACHE_SIZE;
    }

    sensor_data_t *data = &s_cache[s_cache_head];
    data->sensor_id = event->data[0];
    data->timestamp_ms = ((uint32_t)event->data[1] << 0) |
                         ((uint32_t)event->data[2] << 8) |
                         ((uint32_t)event->data[3] << 16) |
                         ((uint32_t)event->data[4] << 24);
    // count 是 2 字节
    data->count = ((uint16_t)event->data[5] << 0) |
                  ((uint16_t)event->data[6] << 8);
    data->receive_time_ms = esp_timer_get_time() / 1000;

    ESP_LOGI(TAG, "Cached: id=%d, ts=%u, count=%d",data->sensor_id, data->timestamp_ms, data->count);

    s_cache_head = next_head;
}

static void sensor_manager_process_cache(void)
{
    if (s_data_callback == NULL)
    {
        return;
    }

    while (s_cache_tail != s_cache_head)
    {
        sensor_data_t *data = &s_cache[s_cache_tail];
        s_data_callback(data, s_ctx_callback);
        s_cache_tail = (s_cache_tail + 1) % SENSOR_CACHE_SIZE;
    }
}

static void sensor_manager_on_protocol_event(const sen_protocol_event_t *event, void *user_ctx)
{
    (void)user_ctx;

    if (event == NULL)
    {
        return;
    }

    // 处理上报的传感器数据
    if (event->type == SEN_EVENT_RX_FRAME && event->cmd == SEN_CMD_SENSOR_DATA)
    {
        if (sensor_manager_lock())
        {
            sensor_manager_cache_data(event);
            sensor_manager_unlock();
        }
        ESP_LOGI(TAG, "Cached sensor data: sensor_id=%d, len=%d", event->data[0], event->len);
    }

    if (event->type == SEN_EVENT_TIMEOUT)
    {
        ESP_LOGW(TAG, "Protocol timeout: cmd=0x%02X, seq=%d", event->cmd, event->seq);
    }
}

// ==================== 公共接口 ====================

esp_err_t sensor_manager_init(void)
{
    if (s_mutex == NULL)
    {
        s_mutex = xSemaphoreCreateMutex();
    }

    memset(s_cache, 0, sizeof(s_cache));
    s_cache_head = 0;
    s_cache_tail = 0;
    s_last_poll_ms = 0;

    sen_protocol_init();
    sen_protocol_set_event_callback(sensor_manager_on_protocol_event, NULL);

    ESP_LOGI(TAG, "Sensor manager initialized");
    return ESP_OK;
}

void sensor_manager_set_data_callback(sensor_data_callback_t callback, void *user_ctx)
{
    s_data_callback = callback;
    s_ctx_callback = user_ctx;
}

void sensor_manager_poll(uint32_t now_ms)
{
    // 处理协议层超时
    sen_protocol_process_tick(now_ms);

    // 处理缓存数据（每 100ms 处理一次）
    if ((now_ms - s_last_poll_ms) >= 100)
    {
        s_last_poll_ms = now_ms;
        if (sensor_manager_lock())
        {
            sensor_manager_process_cache();
            sensor_manager_unlock();
        }
    }
}

uint32_t sensor_manager_get_cache_count(void)
{
    uint32_t count = 0;
    if (sensor_manager_lock())
    {
        if (s_cache_head >= s_cache_tail)
        {
            count = s_cache_head - s_cache_tail;
        }
        else
        {
            count = SENSOR_CACHE_SIZE - s_cache_tail + s_cache_head;
        }
        sensor_manager_unlock();
    }
    return count;
}