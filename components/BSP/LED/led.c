#include "led.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "LED";

static bool s_led_state = false;
// LED 任务句柄
static TaskHandle_t s_led_task_handle = NULL;

// LED 模式队列
static QueueHandle_t s_led_queue = NULL;

// 当前 LED 模式
static led_mode_t s_current_mode = LED_MODE_OFF;

// 闪烁 3 次状态
static bool s_blink_3_active = false;
static uint8_t s_blink_count = 0;
static led_mode_t s_after_mode = LED_MODE_OFF;

static void led_toggle(void);

// LED 模式命令
typedef struct
{
    led_mode_t mode;
    led_mode_t after_mode;
} led_cmd_t;

static void led_toggle(void)
{
    s_led_state = !s_led_state;

    if (s_led_state)
    {
        LED_ON();
    }
    else
    {
        LED_OFF();
    }
}
/**
 * @brief LED 任务
 */
static void led_task_impl(void *arg)
{
    led_cmd_t cmd;
    uint32_t blink_interval = 0;
    uint32_t last_toggle = 0;
    uint32_t blink_start = 0;
    led_mode_t last_mode = LED_MODE_INVALID;
    ESP_LOGI(TAG, "LED task started");
    while (1)
    {
        uint32_t now = esp_timer_get_time() / 1000;
        /*
            处理LED命令
        */
        if (xQueueReceive(s_led_queue, &cmd, 0) == pdTRUE)
        {
            if (cmd.mode == LED_MODE_BLINK_3)
            {
                s_blink_3_active = true;
                s_blink_count = 0;
                s_after_mode = cmd.after_mode;
                blink_start = now;
                LED_ON();
            }
            else
            {
                s_blink_3_active = false;
                s_current_mode = cmd.mode;
            }
        }
        /*
            三次闪烁处理
        */
        if (s_blink_3_active)
        {
            if (now - blink_start >= 200)
            {
                blink_start = now;
                s_blink_count++;
                LED_TOGGLE();
                if (s_blink_count >= 6)
                {
                    s_blink_3_active = false;
                    s_current_mode = s_after_mode;
                    last_mode = LED_MODE_INVALID;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        /*
            普通模式
            只有模式变化才执行一次
        */
        if (s_current_mode != last_mode)
        {
            switch (s_current_mode)
            {
            case LED_MODE_OFF:
                LED_OFF();
                break;
            case LED_MODE_ON:
                LED_ON();
                break;
            case LED_MODE_SLOW_FLASH:
                last_toggle = now;
                break;
            case LED_MODE_FAST_FLASH:
                last_toggle = now;
                break;
            default:
                LED_OFF();
                break;
            }
            last_mode = s_current_mode;
        }
        /*
            闪烁模式时间处理
        */
        switch (s_current_mode)
        {
        case LED_MODE_SLOW_FLASH:
            blink_interval = 500;
            if (now - last_toggle >= blink_interval)
            {
                last_toggle = now;
                LED_TOGGLE();
            }
            break;
        case LED_MODE_FAST_FLASH:
            blink_interval = 200;
            if (now - last_toggle >= blink_interval)
            {
                last_toggle = now;
                LED_TOGGLE();
            }
            break;
        default:
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief 初始化 LED
 */
void led_init(void)
{
    // GPIO 配置
    gpio_config_t gpio_cfg =
        {
            .pin_bit_mask = (1ULL << LED_GPIO_PIN),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };

    ESP_ERROR_CHECK(gpio_config(&gpio_cfg));
    // 默认熄灭
    LED_OFF();

    // 创建消息队列
    s_led_queue = xQueueCreate(5, sizeof(led_cmd_t));
    if (s_led_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create LED queue");
        return;
    }

    // 创建 LED 任务
    xTaskCreate(led_task_impl, "led_task", 2048, NULL, 3, &s_led_task_handle);
    ESP_LOGI(TAG, "LED initialized");
}

/**
 * @brief 设置 LED 模式
 * @param mode LED 模式
 */
void led_set_mode(led_mode_t mode)
{
    ESP_LOGI(TAG, "led_set_mode called: %d", mode);
    led_cmd_t cmd = {
        .mode = mode,
        .after_mode = LED_MODE_OFF,
    };

    if (s_led_queue != NULL)
    {
        xQueueSend(s_led_queue, &cmd, 0);
    }
}

/**
 * @brief 设置 LED 模式（闪烁 3 次后切换）
 * @param mode 要执行的闪烁模式
 * @param after_mode 闪烁完成后切换到的模式
 */
void led_set_mode_with_callback(led_mode_t mode, led_mode_t after_mode)
{
    if (mode != LED_MODE_BLINK_3)
    {
        led_set_mode(mode);
        return;
    }

    led_cmd_t cmd = {
        .mode = LED_MODE_BLINK_3,
        .after_mode = after_mode,
    };

    if (s_led_queue != NULL)
    {
        xQueueSend(s_led_queue, &cmd, 0);
    }
}