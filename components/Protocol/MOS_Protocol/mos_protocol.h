// mos_protocol.h
#ifndef MOS_PROTOCOL_H
#define MOS_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "mos.h"

// ==================== 协议常量 ====================
#define MOS_FRAME_HEAD 0xAA
#define MOS_DEVICE_ADDR 0x01
#define MOS_MAX_DATA_LEN 8

// ==================== 命令码 ====================
#define CMD_MOS_CONTROL 0x10     // 单路控制
#define CMD_MOS_ALL_CONTROL 0x11 // 全部控制
#define CMD_MOS_STATE_SET 0x12   // 多路设置
#define CMD_MOS_GET 0x20         // 查询状态
#define CMD_MOS_REPLY 0x21       // 查询回复

// ==================== 协议接口 ====================
void MOS_Protocol_Init(void);
void MOS_Protocol_RxByte(uint8_t ch);
void MOS_Protocol_RxBytes(const uint8_t *data, uint16_t len);

#endif // MOS_PROTOCOL_H

/*
    功能	        发送
    MOS0 ON	        AA 01 10 02 00 01 12
    MOS0 OFF	    AA 01 10 02 00 00 13
    MOS7 ON	        AA 01 10 02 07 01 15
    全部ON	        AA 01 11 01 01 10
    全部OFF	        AA 01 11 01 00 11
    设置多路:0,4,5 	AA 01 12 01 25 37
    查询状态	    AA 01 20 00 21
*/