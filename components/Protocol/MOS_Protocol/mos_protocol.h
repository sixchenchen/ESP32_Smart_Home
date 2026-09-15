// mos_protocol.h
#ifndef mos_protocol_H
#define mos_protocol_H

#include <stdint.h>
#include <stdbool.h>
#include "mos.h"

// ==================== 协议常量 ====================
#define mos_FRAME_HEAD 0xAA
#define mos_DEVICE_ADDR 0x01
#define mos_MAX_DATA_LEN 8

// ==================== 命令码 ====================
#define CMD_mos_CONTROL 0x10     // 单路控制
#define CMD_mos_ALL_CONTROL 0x11 // 全部控制
#define CMD_mos_STATE_SET 0x12   // 多路设置
#define CMD_mos_GET 0x20         // 查询状态
#define CMD_mos_REPLY 0x21       // 查询回复

// ==================== 协议接口 ====================
void mos_protocol_Init(void);
void mos_protocol_RxByte(uint8_t ch);
void mos_protocol_RxBytes(const uint8_t *data, uint16_t len);

#endif // mos_protocol_H

/*
    功能	        发送
    mos0 ON	        AA 01 10 02 00 01 12
    mos0 OFF	    AA 01 10 02 00 00 13
    mos7 ON	        AA 01 10 02 07 01 15
    全部ON	        AA 01 11 01 01 10
    全部OFF	        AA 01 11 01 00 11
    设置多路:0,4,5 	AA 01 12 01 25 37
    查询状态	    AA 01 20 00 21
*/