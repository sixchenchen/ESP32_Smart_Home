#ifndef SEN_FRAME_H
#define SEN_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SEN_FRAME_HEAD 0x55u
#define SEN_ADDR_BROADCAST 0xFFu
#define SEN_ADDR_ESP32 0x01u
#define SEN_ADDR_GD32 0x02u
#define SEN_ITEM_SIZE 9u                                                   // 每条数据 9 字节：sensor_id(1) + timestamp(4) + value(4)
#define SEN_MAX_DATA_LEN 1024u                                             // DATA 最大长度（可根据实际调整）
#define SEN_MAX_FRAME_LEN (1u + 1u + 1u + 1u + 2u + SEN_MAX_DATA_LEN + 1u) // 帧最大长度：HEAD(1) + ADDR(1) + CMD(1) + SEQ(1) + LEN(2) + DATA(N) + CRC(1)
#define SEN_ACK_TIMEOUT_MS 50u
#define SEN_MAX_RETRY 3u
#define SEN_ITEM_SIZE 9u // 每条数据 9 字节：sensor_id(1) + timestamp(4) + value(4)

typedef enum
{
    SEN_CMD_SENSOR_DATA = 0x01, // 传感器数据上报
    SEN_CMD_STATUS = 0x02,      // 状态上报
    SEN_CMD_QUERY_IDLE = 0x10,  // 查询空闲
    SEN_CMD_IDLE_RESP = 0x11,   // 空闲响应
    SEN_CMD_ACK = 0x20,         // 确认应答
    SEN_CMD_NACK = 0x21,        // 否定应答
    SEN_CMD_HEARTBEAT = 0x30,   // 心跳
    SEN_CMD_CACHE_SYNC = 0x31,  // 缓存同步
    SEN_CMD_SYNC_REQ = 0x60,    // 同步请求
    SEN_CMD_SYNC_ACK = 0x61,    // 同步应答
} sen_cmd_t;
// HEAD  │  ADDR  │  CMD   │  SEQ   │ LEN(2位)  │ DATA │ CRC
typedef struct
{
    uint8_t head;                   // 帧头
    uint8_t addr;                   // 设备地址
    uint8_t cmd;                    // 命令码
    uint8_t seq;                    // 序列号
    uint16_t len;                   // 数据长度
    uint8_t data[SEN_MAX_DATA_LEN]; // 数据负载
    uint8_t crc;                    // CRC 校验
} sen_frame_t;

uint8_t sen_calc_crc(const uint8_t *buf, size_t len);
bool sen_frame_encode(uint8_t *out, size_t out_size, uint8_t addr, uint8_t cmd, uint8_t seq, const void *data, uint16_t len, uint16_t *frame_len);
bool sen_frame_decode(const uint8_t *in, size_t in_len, sen_frame_t *frame, uint16_t *decoded_len);
bool sen_frame_is_valid(const sen_frame_t *frame, uint16_t frame_len);

#endif

// 55 02 01 01 00 D9 01 E8 03 00 00 01 00 02 D0 07 00 00 02 00 03 B8 0B 00 00 03 00 04 A0 0F 00 00 04 00 05 88 13 00 00 05 00 06 70 17 00 00 06 00 07 58 1B 00 00 07 00 08 40 1F 00 00 08 00 09 28 23 00 00 09 00 0A 10 27 00 00 0A 00 0B F8 2A 00 00 0B 00 0C E0 2E 00 00 0C 00 0D C8 32 00 00 0D 00 0E B0 36 00 00 0E 00 0F 98 3A 00 00 0F 00 10 80 3E 00 00 10 00 11 68 42 00 00 11 00 12 50 46 00 00 12 00 13 38 4A 00 00 13 00 14 20 4E 00 00 14 00 15 08 52 00 00 15 00 16 F0 55 00 00 16 00 17 D8 59 00 00 17 00 18 C0 5D 00 00 18 00 19 A8 61 00 00 19 00 1A 90 65 00 00 1A 00 1B 78 69 00 00 1B 00 1C 60 6D 00 00 1C 00 1D 48 71 00 00 1D 00 1E 30 75 00 00 1E 00 1F 18 79 00 00 1F 00 A5