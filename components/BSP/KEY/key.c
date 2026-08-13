#include "key.h"

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "wifi_manager.h"

static const char *TAG = "KEY";

static QueueHandle_t key_queue = NULL;

/*
    GPIO中断

    只发送事件
*/
static void IRAM_ATTR key_isr_handler(void *arg)
{

    uint32_t gpio_num =
        (uint32_t)arg;

    BaseType_t pxHigherPriorityTaskWoken = pdFALSE;

    xQueueSendFromISR(
        key_queue,
        &gpio_num,
        &pxHigherPriorityTaskWoken);

    if (pxHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

/*
    按键任务

*/
static void key_task(void *arg)
{

    uint32_t gpio_num;

    while (1)
    {

        if (
            xQueueReceive(
                key_queue,
                &gpio_num,
                portMAX_DELAY))
        {

            ESP_LOGI(TAG,
                     "key press");

            /*
                消抖
            */

            vTaskDelay(
                pdMS_TO_TICKS(50));

            /*
                确认按下
            */

            if (
                gpio_get_level(
                    BOOT_INT_GPIO_PIN) == 0)
            {

                uint32_t press_time = 0;

                /*
                    开始检测长按

                    每10ms检测一次

                */

                while (
                    gpio_get_level(
                        BOOT_INT_GPIO_PIN) == 0)
                {

                    vTaskDelay(
                        pdMS_TO_TICKS(10));

                    press_time += 10;

                    /*
                        长按2秒

                    */

                    if (
                        press_time >=
                        KEY_LONG_PRESS_TIME_MS)
                    {

                        ESP_LOGW(
                            TAG,
                            "long press reset wifi");

                        wifi_manager_factory_reset();

                        /*
                            等待释放

                        */

                        while (
                            gpio_get_level(
                                BOOT_INT_GPIO_PIN) == 0)
                        {

                            vTaskDelay(
                                pdMS_TO_TICKS(50));
                        }

                        break;
                    }
                }

                ESP_LOGI(TAG,
                         "key release");
            }
        }
    }
}

esp_err_t key_init(void)
{
    /*
        创建消息队列
    */

    key_queue = xQueueCreate(5, sizeof(uint32_t));

    if (key_queue == NULL)
    {
        return ESP_FAIL;
    }
    gpio_config_t io_conf =
        {
            .pin_bit_mask = 1ULL << BOOT_INT_GPIO_PIN,
            .mode = GPIO_MODE_INPUT,
            /*
                上拉
                松开=1
                按下=0
            */
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            /*
                下降沿中断
            */
            .intr_type = GPIO_INTR_NEGEDGE,

        };

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    /*
        安装GPIO ISR
    */
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(
        gpio_isr_handler_add(
            BOOT_INT_GPIO_PIN,
            key_isr_handler,
            (void *)BOOT_INT_GPIO_PIN));

    /*
        创建KEY任务
    */
    xTaskCreate(
        key_task,
        "key_task",
        4096,
        NULL,
        5,
        NULL);
    ESP_LOGI(TAG, "key init success");
    return ESP_OK;
}
