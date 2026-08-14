#ifndef __MOS_H
#define __MOS_H

#include <stdint.h>
#include "driver/gpio.h"

/*
    ESP32 MOS GPIO定义
    MOS0 -> GPIO13
    MOS1 -> GPIO12
    MOS2 -> GPIO14
    MOS3 -> GPIO27
    MOS4 -> GPIO26
    MOS5 -> GPIO25
    MOS6 -> GPIO33
    MOS7 -> GPIO32
*/
#define MOS_CHANNEL_NUM 8
#define MOS0_GPIO GPIO_NUM_13
#define MOS1_GPIO GPIO_NUM_12
#define MOS2_GPIO GPIO_NUM_14
#define MOS3_GPIO GPIO_NUM_27
#define MOS4_GPIO GPIO_NUM_26
#define MOS5_GPIO GPIO_NUM_25
#define MOS6_GPIO GPIO_NUM_33
#define MOS7_GPIO GPIO_NUM_32

typedef enum
{
    MOS_OFF = 0,
    MOS_ON = 1
} MOS_State;

typedef enum
{
    MOS_ALL_OFF = 0x00,
    MOS_ALL_ON = 0xFF,
} MOS_AllState;

/*
    MOS初始化
*/
void MOS_Init(void);

/*
    单路MOS控制
    channel: 0~7
    state:   MOS_ON / MOS_OFF
*/
uint8_t MOS_Control(uint8_t channel, MOS_State state);

/*
    全部MOS控制
    state:
        MOS_ON
        MOS_OFF
*/
uint8_t MOS_All_Control(MOS_State state);

/*
    获取单路MOS状态
    返回：
        MOS_OFF
        MOS_ON
*/
MOS_State MOS_Get_State(uint8_t channel);
/*
    获取全部MOS状态
    bit0 -> MOS0
    bit1 -> MOS1
    ...
    bit7 -> MOS7

    例如：
    00001111
    表示 MOS0~MOS3 ON
*/
uint8_t MOS_Get_All(void);

#endif