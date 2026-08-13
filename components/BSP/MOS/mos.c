#include "mos.h"

/*
    8路MOS GPIO表

    下标与channel一一对应：

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
    保存8路MOS当前状态

    bit0 -> MOS0
    bit1 -> MOS1
    ...
    bit7 -> MOS7

    例如：

    00000101

    MOS0 = ON
    MOS1 = OFF
    MOS2 = ON
    其他 = OFF
*/
static uint8_t mos_state = 0;

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

    /*
        配置8个GPIO
    */
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    /*
        上电默认全部关闭
    */
    MOS_All_Control(MOS_OFF);
}

/*
    单路MOS控制
*/
void MOS_Control(uint8_t channel, MOS_State state)
{
    /*
        防止通道越界
    */
    if (channel >= MOS_CHANNEL_NUM)
    {
        return;
    }

    /*
        GPIO输出
    */
    ESP_ERROR_CHECK(
        gpio_set_level(
            mos_gpio[channel],
            (uint32_t)state));

    /*
        更新软件状态
    */
    if (state == MOS_ON)
    {
        mos_state |= (uint8_t)(1U << channel);
    }
    else
    {
        mos_state &= (uint8_t)~(1U << channel);
    }
}

/*
    全部MOS控制
*/
void MOS_All_Control(MOS_State state)
{
    for (uint8_t i = 0; i < MOS_CHANNEL_NUM; i++)
    {
        /*
            不直接操作GPIO，
            统一调用单路控制，
            保证GPIO状态和mos_state同步
        */
        MOS_Control(i, state);
    }
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