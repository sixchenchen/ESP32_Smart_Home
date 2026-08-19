#include "key_manager.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "mqtt_message.h"
#include "mqtt_topic.h"
#include "key.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

static const char *TAG = "KEY_MANAGER";

// 发布恢复出厂事件
static void publish_factory_reset_event(void)
{
    char *msg = mqtt_message_create_factory_reset();
    if (msg == NULL)
    {
        ESP_LOGE(TAG, "create factory reset message failed");
        return;
    }
    // 尝试发送 3 次
    bool sent = false;
    for (int i = 0; i < 3; i++)
    {
        esp_err_t ret = mqtt_manager_publish(
            mqtt_topic_event(),
            msg,
            strlen(msg),
            1,    // QoS 1，确保送达
            false // 不保留
        );
        if (ret == ESP_OK)
        {
            sent = true;
            ESP_LOGI(TAG, "factory reset event published (try %d)", i + 1);
            break;
        }
        ESP_LOGW(TAG, "publish failed (try %d), retry...", i + 1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    free(msg);
    if (!sent)
    {
        ESP_LOGE(TAG, "factory reset event publish failed after 3 retries");
    }
}

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
                break;
            // 长按：恢复出厂设置
            case KEY_EVENT_LONG_PRESS:
                ESP_LOGW(TAG, "long press: factory reset");
                // 先发布恢复出厂事件（在线状态）
                publish_factory_reset_event();
                // 等待消息发送完成（100ms）
                vTaskDelay(pdMS_TO_TICKS(100));
                // 再执行恢复出厂（断开连接）
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