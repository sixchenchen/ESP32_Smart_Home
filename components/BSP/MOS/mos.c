#include "mos.h"
#include "esp_log.h"
/*
    8路MOS GPIO表
    0 -> GPIO13
    1 -> GPIO12
    2 -> GPIO14
    3 -> GPIO27
    4 -> GPIO26
    5 -> GPIO25
    6 -> GPIO33
    7 -> GPIO32
*/
static const gpio_num_t mos_gpio[MOS_CHANNEL_NUM] =
    {
        MOS0_GPIO,
        MOS1_GPIO,
        MOS2_GPIO,
        MOS3_GPIO,
        MOS4_GPIO,
        MOS5_GPIO,
        MOS6_GPIO,
        MOS7_GPIO};

/*
    保存8路MOS当前状: 00000101
    MOS0 = ON
    MOS2 = ON
    其他 = OFF
*/
static uint8_t mos_state = 0;

static const char *TAG = "MOS";

/*
    初始化MOS
*/
void MOS_Init(void)
{
    gpio_config_t io_conf =
        {
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
            .intr_type = GPIO_INTR_DISABLE};
    // 配置8个GPIO
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    // 上电默认全部关闭
    MOS_All_Control(MOS_OFF);
}

/**
 * @brief 控制 MOS 管
 * @param channel 通道号 (0~7)
 * @param state MOS_ON 或 MOS_OFF
 * @return 1=成功, 0=失败
 */
uint8_t MOS_Control(uint8_t channel, MOS_State state)
{
    if (channel >= MOS_CHANNEL_NUM)
    {
        return 0;
    }
    // 设置 GPIO
    gpio_set_level(mos_gpio[channel], (uint32_t)state);
    // 更新 mos_state
    if (state == MOS_ON)
    {
        mos_state |= (1U << channel);
    }
    else
    {
        mos_state &= ~(1U << channel);
    }
    return 1;
}

/*
    全部MOS控制
*/
uint8_t MOS_All_Control(MOS_State state)
{
    uint8_t old_state = mos_state; // 保存旧状态
    uint8_t new_state = (state == MOS_ON) ? MOS_ALL_ON : MOS_ALL_OFF;
    uint8_t success_count = 0;
    // 尝试设置所有通道
    for (uint8_t i = 0; i < MOS_CHANNEL_NUM; i++)
    {
        if (gpio_set_level(mos_gpio[i], (uint32_t)state) == ESP_OK)
        {
            success_count++;
        }
        else
        {
            // 有通道失败，回滚所有已成功的通道
            ESP_LOGW(TAG, "MOS%d 控制失败，回滚到旧状态", i);
            for (uint8_t j = 0; j < i; j++)
            {
                MOS_State old_state_j = (old_state >> j) & 0x01 ? MOS_ON : MOS_OFF;
                gpio_set_level(mos_gpio[j], (uint32_t)old_state_j);
            }
            // mos_state 保持旧状态不变
            mos_state = old_state;
            return 0;
        }
    }
    // 全部成功，更新 mos_state
    mos_state = new_state;
    return 1;
}

/*
    获取单路MOS状态
*/
MOS_State MOS_Get_State(uint8_t channel)
{
    /*
        通道越界，按照关闭处理
    */
    if (channel >= MOS_CHANNEL_NUM)
    {
        return MOS_OFF;
    }

    if (mos_state & (uint8_t)(1U << channel))
    {
        return MOS_ON;
    }
    return MOS_OFF;
}

/*
    获取8路MOS全部状态
*/
uint8_t MOS_Get_All(void)
{
    return mos_state;
}