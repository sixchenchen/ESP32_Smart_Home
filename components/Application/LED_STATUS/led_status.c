#include "led_status.h"
#include "led.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LED_STATUS";

/**
 * @brief LED 状态监控任务
 */
static void led_status_task(void *arg)
{
    led_mode_t last_mode = LED_MODE_OFF;
    ESP_LOGI(TAG, "LED status task started");

    while (1)
    {
        wifi_manager_state_t state = wifi_manager_get_state();
        led_mode_t new_mode;
        switch (state)
        {
        case WIFI_MANAGER_AP_CONFIG:
            new_mode = LED_MODE_FAST_FLASH;
            break;
        case WIFI_MANAGER_CONNECTING:
        case WIFI_MANAGER_RECONNECTING:
            new_mode = LED_MODE_SLOW_FLASH;
            break;
        case WIFI_MANAGER_CONNECTED:
            new_mode = LED_MODE_ON;
            break;
        default:
            new_mode = LED_MODE_OFF;
            break;
        }

        /*
            只有模式变化才通知LED
        */
        if (new_mode != last_mode)
        {
            ESP_LOGI(TAG, "LED mode change %d -> %d", last_mode, new_mode);
            led_set_mode(new_mode);
            last_mode = new_mode;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
/**
 * @brief 启动 LED 状态监控
 */
void led_status_start(void)
{
    xTaskCreate(led_status_task, "led_status", 2048, NULL, 4, NULL);
}