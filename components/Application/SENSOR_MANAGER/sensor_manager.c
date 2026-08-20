#include "sensor_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <string.h>
#include "sensor_mqtt_bridge.h"

static const char *TAG = "SENSOR_MANAGER";

// 互斥锁
static SemaphoreHandle_t s_mutex = NULL;

// 缓存队列
static sensor_data_t s_cache[SENSOR_CACHE_SIZE];
static uint16_t s_cache_head = 0;
static uint16_t s_cache_tail = 0;

// 任务控制
static TaskHandle_t s_task_handle = NULL;
static SemaphoreHandle_t s_task_semaphore = NULL;

// 回调
static sensor_data_callback_t s_data_callback = NULL;
static void *s_ctx_callback = NULL;

#define TASK_PROCESS_INTERVAL_MS 100
#define TASK_STACK_SIZE 4096

// ==================== 内部函数 ====================

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
    if (event->len < SENSOR_ITEM_SIZE || event->len % SENSOR_ITEM_SIZE != 0)
    {
        ESP_LOGW(TAG, "Invalid sensor data length: %d", event->len);
        return;
    }

    uint8_t count = event->len / SENSOR_ITEM_SIZE;
    ESP_LOGI(TAG, "Processing %d sensor items", count);

    if (!sensor_manager_lock())
    {
        return;
    }

    for (uint8_t i = 0; i < count; i++)
    {
        uint8_t offset = i * SENSOR_ITEM_SIZE;

        uint16_t next_head = (s_cache_head + 1) % SENSOR_CACHE_SIZE;
        if (next_head == s_cache_tail)
        {
            ESP_LOGW(TAG, "Cache full, dropping oldest");
            s_cache_tail = (s_cache_tail + 1) % SENSOR_CACHE_SIZE;
        }

        sensor_data_t *data = &s_cache[s_cache_head];
        data->sensor_id = event->data[offset];
        data->timestamp_ms = ((uint32_t)event->data[offset + 1] << 0) |
                             ((uint32_t)event->data[offset + 2] << 8) |
                             ((uint32_t)event->data[offset + 3] << 16) |
                             ((uint32_t)event->data[offset + 4] << 24);
        data->count = ((uint16_t)event->data[offset + 5] << 0) |
                      ((uint16_t)event->data[offset + 6] << 8);
        data->receive_time_ms = esp_timer_get_time() / 1000;

        ESP_LOGI(TAG, "Cached[%d]: id=%d, ts=%u, count=%d",
                 i, data->sensor_id, data->timestamp_ms, data->count);

        s_cache_head = next_head;
    }

    sensor_manager_unlock();

    if (s_task_semaphore != NULL)
    {
        xSemaphoreGive(s_task_semaphore);
    }
}

static void sensor_manager_process_cache(void)
{
    if (s_data_callback == NULL)
    {
        return;
    }

    if (!sensor_manager_lock())
    {
        return;
    }

    while (s_cache_tail != s_cache_head)
    {
        sensor_data_t *data = &s_cache[s_cache_tail];
        s_data_callback(data, s_ctx_callback);
        s_cache_tail = (s_cache_tail + 1) % SENSOR_CACHE_SIZE;
    }

    sensor_manager_unlock();
}

static void sensor_manager_on_protocol_event(const sen_protocol_event_t *event, void *user_ctx)
{
    (void)user_ctx;

    if (event == NULL)
    {
        return;
    }

    if (event->type == SEN_EVENT_RX_FRAME && event->cmd == SEN_CMD_SENSOR_DATA)
    {
        sensor_manager_cache_data(event);
        ESP_LOGI(TAG, "Cached sensor data: sensor_id=%d, len=%d", event->data[0], event->len);
    }

    if (event->type == SEN_EVENT_TIMEOUT)
    {
        ESP_LOGW(TAG, "Protocol timeout: cmd=0x%02X, seq=%d", event->cmd, event->seq);
    }
}

// 任务函数
static void sensor_task(void *arg)
{
    uint32_t last_process_ms = 0;

    ESP_LOGI(TAG, "Sensor task started");

    while (1)
    {
        uint32_t now_ms = esp_timer_get_time() / 1000;

        if (xSemaphoreTake(s_task_semaphore, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            ESP_LOGD(TAG, "Semaphore triggered, processing cache");
            sensor_manager_process_cache();
            last_process_ms = now_ms;
        }

        if ((now_ms - last_process_ms) >= TASK_PROCESS_INTERVAL_MS)
        {
            sensor_manager_process_cache();
            last_process_ms = now_ms;
        }

        sen_protocol_process_tick(now_ms);
        sensor_mqtt_bridge_poll(now_ms);
    }
}

// ==================== 公共接口 ====================

esp_err_t sensor_manager_init(void)
{
    if (s_mutex == NULL)
    {
        s_mutex = xSemaphoreCreateMutex();
    }

    if (s_task_semaphore == NULL)
    {
        s_task_semaphore = xSemaphoreCreateBinary();
        if (s_task_semaphore == NULL)
        {
            ESP_LOGE(TAG, "Failed to create semaphore");
            return ESP_FAIL;
        }
    }

    memset(s_cache, 0, sizeof(s_cache));
    s_cache_head = 0;
    s_cache_tail = 0;

    sen_protocol_init();
    sen_protocol_set_event_callback(sensor_manager_on_protocol_event, NULL);

    ESP_LOGI(TAG, "Sensor manager initialized");
    return ESP_OK;
}

// 启动传感器处理任务
esp_err_t sensor_manager_task_start(void)
{
    if (s_task_handle != NULL)
    {
        ESP_LOGW(TAG, "Sensor task already running");
        return ESP_OK;
    }

    BaseType_t ret = xTaskCreate(
        sensor_task,
        "sensor_task",
        TASK_STACK_SIZE,
        NULL,
        8,
        &s_task_handle);

    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create sensor task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sensor task started");
    return ESP_OK;
}

void sensor_manager_poll(uint32_t now_ms)
{
    sen_protocol_process_tick(now_ms);
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

void sensor_manager_set_data_callback(sensor_data_callback_t callback, void *user_ctx)
{
    s_data_callback = callback;
    s_ctx_callback = user_ctx;
}