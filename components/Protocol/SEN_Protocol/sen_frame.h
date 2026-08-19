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
#define SEN_MAX_DATA_LEN 512u                                              // DATA 最大长度（可根据实际调整）
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
bool sen_frame_encode(uint8_t *out, size_t out_size, uint8_t addr, uint8_t cmd, uint8_t seq, const void *data, uint16_t  len, uint16_t  *frame_len);
bool sen_frame_decode(const uint8_t *in, size_t in_len, sen_frame_t *frame, uint16_t  *decoded_len);
bool sen_frame_is_valid(const sen_frame_t *frame, uint16_t frame_len);

#endif