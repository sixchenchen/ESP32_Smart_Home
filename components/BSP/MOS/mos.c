#include "mos.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const gpio_num_t mos_gpio[mos_CHANNEL_NUM] = {
    mos0_GPIO, mos1_GPIO, mos2_GPIO, mos3_GPIO,
    mos4_GPIO, mos5_GPIO, mos6_GPIO, mos7_GPIO};

static uint8_t mos_state = 0;

// 互斥锁
static SemaphoreHandle_t mos_mutex = NULL;

// 初始化
void mos_Init(void)
{
    // 创建互斥锁
    mos_mutex = xSemaphoreCreateMutex();

    gpio_config_t io_conf = {
        .pin_bit_mask =
            (1ULL << mos0_GPIO) |
            (1ULL << mos1_GPIO) |
            (1ULL << mos2_GPIO) |
            (1ULL << mos3_GPIO) |
            (1ULL << mos4_GPIO) |
            (1ULL << mos5_GPIO) |
            (1ULL << mos6_GPIO) |
            (1ULL << mos7_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    mos_All_Control(mos_OFF);
}

// 单路控制
uint8_t mos_Control(uint8_t channel, mos_State state)
{
    if (channel >= mos_CHANNEL_NUM)
        return 0;

    if (xSemaphoreTake(mos_mutex, portMAX_DELAY) != pdTRUE)
        return 0;

    gpio_set_level(mos_gpio[channel], (uint32_t)state);
    if (state == mos_ON)
        mos_state |= (1U << channel);
    else
        mos_state &= ~(1U << channel);

    xSemaphoreGive(mos_mutex);
    return 1;
}

// 全部控制
uint8_t mos_All_Control(mos_State state)
{
    // 加锁
    if (mos_mutex != NULL && xSemaphoreTake(mos_mutex, portMAX_DELAY) == pdTRUE)
    {
        for (uint8_t i = 0; i < mos_CHANNEL_NUM; i++)
        {
            gpio_set_level(mos_gpio[i], (uint32_t)state);
        }
        mos_state = (state == mos_ON) ? mos_ALL_ON : mos_ALL_OFF;

        // 解锁
        xSemaphoreGive(mos_mutex);
    }
    else
    {
        // 降级方案
        for (uint8_t i = 0; i < mos_CHANNEL_NUM; i++)
        {
            gpio_set_level(mos_gpio[i], (uint32_t)state);
        }
        mos_state = (state == mos_ON) ? mos_ALL_ON : mos_ALL_OFF;
    }
    return 1;
}

// 获取单路状态
mos_State mos_Get_State(uint8_t channel)
{
    if (channel >= mos_CHANNEL_NUM)
    {
        return mos_OFF;
    }

    uint8_t state;
    // 加锁读取
    if (mos_mutex != NULL && xSemaphoreTake(mos_mutex, portMAX_DELAY) == pdTRUE)
    {
        state = (mos_state & (1U << channel)) ? mos_ON : mos_OFF;
        xSemaphoreGive(mos_mutex);
    }
    else
    {
        state = (mos_state & (1U << channel)) ? mos_ON : mos_OFF;
    }
    return state;
}

// 获取全部状态
uint8_t mos_Get_All(void)
{
    uint8_t state;
    // 加锁读取
    if (mos_mutex != NULL && xSemaphoreTake(mos_mutex, portMAX_DELAY) == pdTRUE)
    {
        state = mos_state;
        xSemaphoreGive(mos_mutex);
    }
    else
    {
        state = mos_state;
    }
    return state;
}