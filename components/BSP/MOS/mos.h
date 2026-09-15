#ifndef __mos_H
#define __mos_H

#include <stdint.h>
#include "driver/gpio.h"

/*
    ESP32 mos GPIO定义
    mos0 -> GPIO13
    mos1 -> GPIO12
    mos2 -> GPIO14
    mos3 -> GPIO27
    mos4 -> GPIO26
    mos5 -> GPIO25
    mos6 -> GPIO33
    mos7 -> GPIO32
*/
#define mos_CHANNEL_NUM 8
#define mos0_GPIO GPIO_NUM_13
#define mos1_GPIO GPIO_NUM_12
#define mos2_GPIO GPIO_NUM_14
#define mos3_GPIO GPIO_NUM_27
#define mos4_GPIO GPIO_NUM_26
#define mos5_GPIO GPIO_NUM_25
#define mos6_GPIO GPIO_NUM_33
#define mos7_GPIO GPIO_NUM_32

typedef enum
{
    mos_OFF = 0,
    mos_ON = 1
} mos_State;

typedef enum
{
    mos_ALL_OFF = 0x00,
    mos_ALL_ON = 0xFF,
} mos_AllState;

/*
    mos初始化
*/
void mos_Init(void);

/*
    单路mos控制
    channel: 0~7
    state:   mos_ON / mos_OFF
*/
uint8_t mos_Control(uint8_t channel, mos_State state);

/*
    全部mos控制
    state:
        mos_ON
        mos_OFF
*/
uint8_t mos_All_Control(mos_State state);

/*
    获取单路mos状态
    返回：
        mos_OFF
        mos_ON
*/
mos_State mos_Get_State(uint8_t channel);
/*
    获取全部mos状态
    bit0 -> mos0
    bit1 -> mos1
    ...
    bit7 -> mos7

    例如：
    00001111
    表示 mos0~mos3 ON
*/
uint8_t mos_Get_All(void);

#endif