#include "sen_frame.h"
#include <string.h>

uint8_t sen_calc_crc(const uint8_t *buf, size_t len)
{
    uint8_t crc = 0u;
    for (size_t i = 0; i < len; ++i)
    {
        crc ^= buf[i];
    }
    return crc;
}

bool sen_frame_encode(uint8_t *out, size_t out_size, uint8_t addr, uint8_t cmd,
                      uint8_t seq, const void *data, uint16_t len, uint16_t *frame_len)
{
    if (out == NULL || frame_len == NULL || len > SEN_MAX_DATA_LEN)
    {
        return false;
    }

    size_t total_len = 7u + (size_t)len;
    if (out_size < total_len)
    {
        return false;
    }

    memset(out, 0, total_len);
    out[0] = SEN_FRAME_HEAD;
    out[1] = addr;
    out[2] = cmd;
    out[3] = seq;
    out[4] = (uint8_t)((len >> 8) & 0xFF); // LEN 高字节
    out[5] = (uint8_t)(len & 0xFF);        // LEN 低字节

    if (len > 0u && data != NULL)
    {
        memcpy(&out[6], data, len);
    }

    out[6u + (size_t)len] = sen_calc_crc(&out[1], 5u + (size_t)len);
    *frame_len = (uint16_t)total_len;
    return true;
}

bool sen_frame_decode(const uint8_t *in, size_t in_len, sen_frame_t *frame, uint16_t *decoded_len)
{
    if (in == NULL || frame == NULL || decoded_len == NULL || in_len < 6u)
    {
        return false;
    }

    memset(frame, 0, sizeof(*frame));

    frame->head = in[0];
    frame->addr = in[1];
    frame->cmd = in[2];
    frame->seq = in[3];
    frame->len = ((uint16_t)in[4] << 8) | in[5];

    if (frame->head != SEN_FRAME_HEAD)
    {
        return false;
    }

    if (frame->len > SEN_MAX_DATA_LEN)
    {
        return false;
    }

    size_t total_len = (size_t)frame->len + 7u;
    if (in_len != total_len)
    {
        return false;
    }

    if (frame->len > 0u)
    {
        memcpy(frame->data, &in[6], frame->len);
    }

    frame->crc = in[6u + (size_t)frame->len];
    uint8_t expected_crc = sen_calc_crc(&in[1], 5u + (size_t)frame->len);
    if (frame->crc != expected_crc)
    {
        return false;
    }

    *decoded_len = (uint16_t)total_len;
    return true;
}

bool sen_frame_is_valid(const sen_frame_t *frame, uint16_t frame_len)
{
    if (frame == NULL || frame_len < 6u)
    {
        return false;
    }

    if (frame->head != SEN_FRAME_HEAD)
    {
        return false;
    }

    if (frame->len > SEN_MAX_DATA_LEN)
    {
        return false;
    }

    if ((uint8_t)(frame_len - 6u) != frame->len)
    {
        return false;
    }

    uint8_t expected_crc = sen_calc_crc((const uint8_t *)&frame->addr, 4u + (size_t)frame->len);
    return (frame->crc == expected_crc);
}