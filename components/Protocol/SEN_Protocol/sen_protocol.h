#ifndef SEN_PROTOCOL_H
#define SEN_PROTOCOL_H

#include "esp_err.h"
#include "sen_frame.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    SEN_EVENT_RX_FRAME = 0,
    SEN_EVENT_TX_DONE,
    SEN_EVENT_ACK,
    SEN_EVENT_NACK,
    SEN_EVENT_TIMEOUT,
    SEN_EVENT_ERROR,
} sen_protocol_event_type_t;

typedef struct
{
    sen_protocol_event_type_t type;
    uint8_t addr;
    uint8_t cmd;
    uint8_t seq;
    uint8_t len;
    uint8_t data[SEN_MAX_DATA_LEN];
} sen_protocol_event_t;

// 接收回调函数类型
typedef void (*sen_protocol_event_callback_t)(const sen_protocol_event_t *event, void *user_ctx);

void sen_protocol_init(void);
bool sen_protocol_is_idle(void);
bool sen_protocol_query_idle(void);
esp_err_t sen_protocol_send(uint8_t addr, uint8_t cmd, uint8_t seq, const void *data, uint16_t len, bool expect_ack);
void sen_protocol_on_rx_byte(uint8_t ch);
void sen_protocol_process_tick(uint32_t now_ms);
void sen_protocol_sync(void);

/**
 * @brief 注册光栅传感器回调
 * @param callback 回调函数
 */
void sen_protocol_set_event_callback(sen_protocol_event_callback_t callback, void *user_ctx);

#endif