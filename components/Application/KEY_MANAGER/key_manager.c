#include "key_manager.h"
#include "wifi_manager.h" 
#include "key.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

static const char *TAG = "KEY_MANAGER";

static void key_manager_task(void *arg)
{
    key_event_t event;
    QueueHandle_t queue = key_get_queue();

    while (1)
    {
        if (xQueueReceive(queue, &event, portMAX_DELAY))
        {
            switch (event)
            {
            // 短按
            case KEY_EVENT_SHORT_PRESS:
                ESP_LOGI(TAG, "short press");
                // 这里以后扩展
                // 例如: LED切换, MQTT发送, 模式切换
                break;

            // 长按
            case KEY_EVENT_LONG_PRESS:
                ESP_LOGW(TAG, "factory reset wifi");
                wifi_manager_factory_reset();
                break;

            default:
                break;
            }
        }
    }
}

void key_manager_start(void)
{
    // 初始化 BSP KEY
    key_init();

    xTaskCreate(
        key_manager_task,
        "key_manager",
        4096,
        NULL,
        5,
        NULL);
}