#include "key.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define BOOT_INT_GPIO_PIN GPIO_NUM_0
#define KEY_LONG_PRESS_TIME_MS 3000

static const char *TAG = "KEY";

// GPIO中断队列, ISR -> queue -> task
static QueueHandle_t key_gpio_queue = NULL;

// KEY事件队列, BSP输出给Application
static QueueHandle_t key_event_queue = NULL;

// GPIO ISR, 不能处理业务, 只发送GPIO号
static void IRAM_ATTR key_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    BaseType_t high_task_wakeup = pdFALSE;
    xQueueSendFromISR(key_gpio_queue, &gpio_num, &high_task_wakeup);
    if (high_task_wakeup) {
        portYIELD_FROM_ISR();
    }
}

// KEY检测任务
static void key_task(void *arg)
{
    uint32_t gpio_num;
    while (1) {
        if (xQueueReceive(key_gpio_queue, &gpio_num, portMAX_DELAY)) {
            // 消抖
            vTaskDelay(pdMS_TO_TICKS(50));

            // 确认按下
            if (gpio_get_level(BOOT_INT_GPIO_PIN) == 0) {
                uint32_t press_time = 0;

                while (gpio_get_level(BOOT_INT_GPIO_PIN) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                    press_time += 10;

                    // 长按
                    if (press_time >= KEY_LONG_PRESS_TIME_MS) {
                        ESP_LOGI(TAG, "long press");
                        key_event_t event = KEY_EVENT_LONG_PRESS;
                        xQueueSend(key_event_queue, &event, 0);

                        // 等待释放
                        while (gpio_get_level(BOOT_INT_GPIO_PIN) == 0) {
                            vTaskDelay(pdMS_TO_TICKS(10));
                        }
                        break;
                    }
                }

                // 短按
                if (press_time < KEY_LONG_PRESS_TIME_MS) {
                    ESP_LOGI(TAG, "short press");
                    key_event_t event = KEY_EVENT_SHORT_PRESS;
                    xQueueSend(key_event_queue, &event, 0);
                }
            }
        }
    }
}

esp_err_t key_init(void)
{
    // GPIO中断队列
    key_gpio_queue = xQueueCreate(5, sizeof(uint32_t));
    if (key_gpio_queue == NULL) {
        return ESP_FAIL;
    }

    // KEY事件队列
    key_event_queue = xQueueCreate(5, sizeof(key_event_t));
    if (key_event_queue == NULL) {
        return ESP_FAIL;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << BOOT_INT_GPIO_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,  // 按下: 高->低
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(
        BOOT_INT_GPIO_PIN,
        key_isr_handler,
        (void *)BOOT_INT_GPIO_PIN));

    xTaskCreate(key_task, "key_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "key init success");
    return ESP_OK;
}

QueueHandle_t key_get_queue(void)
{
    return key_event_queue;
}