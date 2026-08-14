#include "mos.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const gpio_num_t mos_gpio[MOS_CHANNEL_NUM] = {
    MOS0_GPIO, MOS1_GPIO, MOS2_GPIO, MOS3_GPIO,
    MOS4_GPIO, MOS5_GPIO, MOS6_GPIO, MOS7_GPIO};

static uint8_t mos_state = 0;

// 互斥锁
static SemaphoreHandle_t mos_mutex = NULL;

// 初始化
void MOS_Init(void)
{
    // 创建互斥锁
    mos_mutex = xSemaphoreCreateMutex();

    gpio_config_t io_conf = {
        .pin_bit_mask =
            (1ULL << MOS0_GPIO) |
            (1ULL << MOS1_GPIO) |
            (1ULL << MOS2_GPIO) |
            (1ULL << MOS3_GPIO) |
            (1ULL << MOS4_GPIO) |
            (1ULL << MOS5_GPIO) |
            (1ULL << MOS6_GPIO) |
            (1ULL << MOS7_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    MOS_All_Control(MOS_OFF);
}

// 单路控制 
uint8_t MOS_Control(uint8_t channel, MOS_State state)
{
    if (channel >= MOS_CHANNEL_NUM)
    {
        return 0;
    }

    // 加锁
    if (mos_mutex != NULL && xSemaphoreTake(mos_mutex, portMAX_DELAY) == pdTRUE)
    {
        gpio_set_level(mos_gpio[channel], (uint32_t)state);

        if (state == MOS_ON)
        {
            mos_state |= (1U << channel);
        }
        else
        {
            mos_state &= ~(1U << channel);
        }

        // 解锁
        xSemaphoreGive(mos_mutex);
    }
    else
    {
        // 互斥锁不可用，直接操作（降级方案）
        gpio_set_level(mos_gpio[channel], (uint32_t)state);
        if (state == MOS_ON)
        {
            mos_state |= (1U << channel);
        }
        else
        {
            mos_state &= ~(1U << channel);
        }
    }
    return 1;
}

// 全部控制
uint8_t MOS_All_Control(MOS_State state)
{
    // 加锁
    if (mos_mutex != NULL && xSemaphoreTake(mos_mutex, portMAX_DELAY) == pdTRUE)
    {
        for (uint8_t i = 0; i < MOS_CHANNEL_NUM; i++)
        {
            gpio_set_level(mos_gpio[i], (uint32_t)state);
        }
        mos_state = (state == MOS_ON) ? MOS_ALL_ON : MOS_ALL_OFF;

        // 解锁
        xSemaphoreGive(mos_mutex);
    }
    else
    {
        // 降级方案
        for (uint8_t i = 0; i < MOS_CHANNEL_NUM; i++)
        {
            gpio_set_level(mos_gpio[i], (uint32_t)state);
        }
        mos_state = (state == MOS_ON) ? MOS_ALL_ON : MOS_ALL_OFF;
    }
    return 1;
}

// 获取单路状态
MOS_State MOS_Get_State(uint8_t channel)
{
    if (channel >= MOS_CHANNEL_NUM)
    {
        return MOS_OFF;
    }

    uint8_t state;
    // 加锁读取
    if (mos_mutex != NULL && xSemaphoreTake(mos_mutex, portMAX_DELAY) == pdTRUE)
    {
        state = (mos_state & (1U << channel)) ? MOS_ON : MOS_OFF;
        xSemaphoreGive(mos_mutex);
    }
    else
    {
        state = (mos_state & (1U << channel)) ? MOS_ON : MOS_OFF;
    }
    return state;
}

// 获取全部状态
uint8_t MOS_Get_All(void)
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