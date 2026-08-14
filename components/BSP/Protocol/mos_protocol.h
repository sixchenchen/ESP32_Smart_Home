#ifndef __MOS_PROTOCOL_H
#define __MOS_PROTOCOL_H

#include <stdint.h>

/*
    协议格式： HEAD | ADDRESS | CMD | LEN | DATA | CRC
    HEAD    : 1Byte
    ADDRESS : 1Byte
    CMD     : 1Byte
    LEN     : 1Byte
    DATA    : NByte
    CRC     : 1Byte
*/
#define MOS_FRAME_HEAD 0xAA
#define MOS_DEVICE_ADDR 0x01 // TODO 开发阶段写死了的
#define MOS_MAX_DATA_LEN 8

/*
    MOS命令
*/
typedef enum
{
    /*
        单路控制
        DATA[0] = channel
        DATA[1] = state
        channel: 0 ~ 7
        state:
            0 = OFF
            1 = ON
    */
    CMD_MOS_CONTROL = 0x10,
    /*
        全部控制
        DATA[0]:
         0 = 全部关闭
         1 = 全部打开
    */
    CMD_MOS_ALL_CONTROL = 0x11,
    /*
        8路状态设置
        DATA[0]:
            bit0 -> MOS0
            bit1 -> MOS1
            ...
            bit7 -> MOS7
    */
    CMD_MOS_STATE_SET = 0x12,
    // 查询MOS状态  LEN = 0
    CMD_MOS_GET = 0x20,
    // MOS状态回复  DATA[0] = 8路状态
    CMD_MOS_REPLY = 0x21
} MOS_CMD;
/*
    协议接收状态机
*/
typedef enum
{
    WAIT_HEAD = 0,
    WAIT_ADDR,
    WAIT_CMD,
    WAIT_LEN,
    WAIT_DATA,
    WAIT_CRC
} MOS_RX_STATE;

/*
    初始化协议
*/
void MOS_Protocol_Init(void);
/*
    接收一个字节，UART每收到一个字节调用一次
*/
void MOS_Protocol_RxByte(uint8_t ch);

#endif

/*
    功能	        发送
    MOS0 ON	        AA 01 10 02 00 01 12
    MOS0 OFF	    AA 01 10 02 00 00 13
    MOS7 ON	        AA 01 10 02 07 01 15
    全部ON	        AA 01 11 01 01 10
    全部OFF	        AA 01 11 01 00 11
    设置状态0x25	AA 01 12 01 25 37
    查询状态	    AA 01 20 00 21
*/