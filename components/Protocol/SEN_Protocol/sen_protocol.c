#include "sen_protocol.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "uart_drv.h"
#include <string.h>

static const char *TAG = "SEN_PROTOCOL";

static SemaphoreHandle_t s_protocol_mutex = NULL;
static sen_protocol_event_callback_t s_event_callback = NULL;
static void *s_event_ctx = NULL;
static uint16_t s_current_len = 0;

// 接收状态机
static uint8_t s_rx_buffer[SEN_MAX_FRAME_LEN];
static uint8_t s_rx_count = 0;
static enum {
    SEN_RX_WAIT_HEAD = 0,
    SEN_RX_WAIT_ADDR,
    SEN_RX_WAIT_CMD,
    SEN_RX_WAIT_SEQ,
    SEN_RX_WAIT_LEN_H,
    SEN_RX_WAIT_LEN_L,
    SEN_RX_WAIT_DATA,
    SEN_RX_WAIT_CRC,
} s_rx_state = SEN_RX_WAIT_HEAD;

// 发送状态
static bool s_tx_busy = false;
static bool s_ack_expected = false;
static uint8_t s_last_seq = 0;
static uint8_t s_last_addr = 0;
static uint8_t s_last_cmd = 0;
static uint32_t s_tx_deadline_ms = 0;

static void sen_protocol_reset_rx_state(void)
{
    memset(s_rx_buffer, 0, sizeof(s_rx_buffer));
    s_rx_count = 0;
    s_rx_state = SEN_RX_WAIT_HEAD;
}

static bool sen_protocol_lock(void)
{
    if (s_protocol_mutex == NULL)
    {
        return true;
    }
    return (xSemaphoreTake(s_protocol_mutex, pdMS_TO_TICKS(50)) == pdTRUE);
}

static void sen_protocol_unlock(void)
{
    if (s_protocol_mutex != NULL)
    {
        xSemaphoreGive(s_protocol_mutex);
    }
}

static void sen_protocol_emit_event(sen_protocol_event_t *event)
{
    if (s_event_callback != NULL)
    {
        s_event_callback(event, s_event_ctx);
    }
}

static void sen_protocol_handle_frame(const sen_frame_t *frame)
{
    sen_protocol_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = SEN_EVENT_RX_FRAME;
    event.addr = frame->addr;
    event.cmd = frame->cmd;
    event.seq = frame->seq;
    event.len = frame->len;
    memcpy(event.data, frame->data, frame->len);
    sen_protocol_emit_event(&event);

    // 自动应答
    switch (frame->cmd)
    {
    case SEN_CMD_ACK:
        if (s_ack_expected && frame->seq == s_last_seq)
        {
            s_ack_expected = false;
            s_tx_busy = false;
            event.type = SEN_EVENT_ACK;
            sen_protocol_emit_event(&event);
        }
        break;

    case SEN_CMD_NACK:
        if (s_ack_expected && frame->seq == s_last_seq)
        {
            s_ack_expected = false;
            s_tx_busy = false;
            event.type = SEN_EVENT_NACK;
            sen_protocol_emit_event(&event);
        }
        break;

    case SEN_CMD_QUERY_IDLE:
    {
        uint8_t idle = sen_protocol_is_idle() ? 1u : 0u;
        sen_protocol_send(frame->addr, SEN_CMD_IDLE_RESP, frame->seq, &idle, 1u, false);
        break;
    }

    case SEN_CMD_SYNC_REQ:
        ESP_LOGI(TAG, "Sync request from GD32");
        sen_protocol_send(frame->addr, SEN_CMD_SYNC_ACK, frame->seq, NULL, 0, false);
        break;

    default:
        if (frame->cmd != SEN_CMD_ACK && frame->cmd != SEN_CMD_NACK)
        {
            uint8_t ack = 0u;
            sen_protocol_send(frame->addr, SEN_CMD_ACK, frame->seq, &ack, 0u, false);
        }
        break;
    }
}

void sen_protocol_init(void)
{
    if (s_protocol_mutex == NULL)
    {
        s_protocol_mutex = xSemaphoreCreateMutex();
    }

    sen_protocol_reset_rx_state();
    s_tx_busy = false;
    s_ack_expected = false;
    s_last_seq = 0;
    s_last_addr = 0;
    s_last_cmd = 0;
    s_tx_deadline_ms = 0;

    uart_drv_register_sen_byte_callback(sen_protocol_on_rx_byte);
    ESP_LOGI(TAG, "SEN protocol initialized");
}

bool sen_protocol_is_idle(void)
{
    return (!s_tx_busy && !s_ack_expected);
}

bool sen_protocol_query_idle(void)
{
    return sen_protocol_is_idle();
}

esp_err_t sen_protocol_send(uint8_t addr, uint8_t cmd, uint8_t seq, const void *data, uint16_t len, bool expect_ack)
{
    uint8_t frame_buf[SEN_MAX_FRAME_LEN] = {0};
    uint16_t frame_len = 0;
    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (len > SEN_MAX_DATA_LEN)
    {
        return ESP_ERR_INVALID_SIZE;
    }

    if (!sen_frame_encode(frame_buf, sizeof(frame_buf), addr, cmd, seq, data, len, &frame_len))
    {
        return ESP_ERR_INVALID_SIZE;
    }

    if (!sen_protocol_lock())
    {
        return ESP_ERR_TIMEOUT;
    }

    if (!sen_protocol_is_idle() && expect_ack)
    {
        sen_protocol_unlock();
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = uart_drv_send(frame_buf, frame_len);
    if (ret != ESP_OK)
    {
        sen_protocol_unlock();
        return ret;
    }

    s_tx_busy = true;
    s_ack_expected = expect_ack;
    s_last_addr = addr;
    s_last_cmd = cmd;
    s_last_seq = seq;

    if (expect_ack)
    {
        s_tx_deadline_ms = now_ms + SEN_ACK_TIMEOUT_MS;
    }
    else
    {
        s_tx_busy = false;
        s_ack_expected = false;
        s_tx_deadline_ms = 0;
    }

    sen_protocol_unlock();

    sen_protocol_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = SEN_EVENT_TX_DONE;
    event.addr = addr;
    event.cmd = cmd;
    event.seq = seq;
    event.len = len;
    if (data != NULL)
    {
        memcpy(event.data, data, len);
    }
    sen_protocol_emit_event(&event);

    return ESP_OK;
}

void sen_protocol_process_tick(uint32_t now_ms)
{
    if (!s_ack_expected)
    {
        return;
    }

    if (now_ms >= s_tx_deadline_ms)
    {
        s_ack_expected = false;
        s_tx_busy = false;

        sen_protocol_event_t timeout_event;
        memset(&timeout_event, 0, sizeof(timeout_event));
        timeout_event.type = SEN_EVENT_TIMEOUT;
        timeout_event.addr = s_last_addr;
        timeout_event.cmd = s_last_cmd;
        timeout_event.seq = s_last_seq;
        sen_protocol_emit_event(&timeout_event);
    }
}

void sen_protocol_sync(void)
{
    ESP_LOGI(TAG, "Sending sync request to GD32");
    sen_protocol_send(SEN_ADDR_GD32, SEN_CMD_SYNC_REQ, 0, NULL, 0, true);
}

void sen_protocol_on_rx_byte(uint8_t ch)
{
    switch (s_rx_state)
    {
    case SEN_RX_WAIT_HEAD:
        if (ch == SEN_FRAME_HEAD)
        {
            s_rx_buffer[s_rx_count++] = ch;
            s_rx_state = SEN_RX_WAIT_ADDR;
        }
        break;

    case SEN_RX_WAIT_ADDR:
        s_rx_buffer[s_rx_count++] = ch;
        s_rx_state = SEN_RX_WAIT_CMD;
        break;

    case SEN_RX_WAIT_CMD:
        s_rx_buffer[s_rx_count++] = ch;
        s_rx_state = SEN_RX_WAIT_SEQ;
        break;

    case SEN_RX_WAIT_SEQ:
        s_rx_buffer[s_rx_count++] = ch;
        s_rx_state = SEN_RX_WAIT_LEN_H;
        break;

    case SEN_RX_WAIT_LEN_H:
        s_rx_buffer[s_rx_count++] = ch;
        s_rx_state = SEN_RX_WAIT_LEN_L; // ← 等待 LEN 低字节
        break;

    case SEN_RX_WAIT_LEN_L:
        s_rx_buffer[s_rx_count++] = ch;
        s_current_len = ((uint16_t)s_rx_buffer[4] << 8) | s_rx_buffer[5];
        if (s_current_len > SEN_MAX_DATA_LEN)
        {
            sen_protocol_reset_rx_state();
            break;
        }
        s_rx_state = (s_current_len == 0u) ? SEN_RX_WAIT_CRC : SEN_RX_WAIT_DATA;
        break;

    case SEN_RX_WAIT_DATA:
        s_rx_buffer[s_rx_count++] = ch;
        if (s_rx_count >= (6u + s_current_len))
        {
            s_rx_state = SEN_RX_WAIT_CRC;
        }
        break;
    case SEN_RX_WAIT_CRC:
    {
        sen_frame_t frame;
        uint16_t decoded_len = 0;
        s_rx_buffer[s_rx_count++] = ch;

        if (sen_frame_decode(s_rx_buffer, s_rx_count, &frame, &decoded_len))
        {
            ESP_LOGI(TAG, "CRC:YES");
            sen_protocol_handle_frame(&frame);
        }
        else
        {
            sen_protocol_event_t error_event;
            memset(&error_event, 0, sizeof(error_event));
            error_event.type = SEN_EVENT_ERROR;
            sen_protocol_emit_event(&error_event);
        }

        sen_protocol_reset_rx_state();
        break;
    }

    default:
        sen_protocol_reset_rx_state();
        break;
    }
}

void sen_protocol_set_event_callback(sen_protocol_event_callback_t callback, void *user_ctx)
{
    s_event_callback = callback;
    s_event_ctx = user_ctx;
}