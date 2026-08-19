#include "mos_protocol.h"
#include "mos.h"
#include "uart_drv.h"
#include "esp_log.h"

static const char *TAG = "MOS_PROTOCOL";

//   状态机枚举
typedef enum
{
    WAIT_HEAD = 0,
    WAIT_ADDR,
    WAIT_CMD,
    WAIT_LEN,
    WAIT_DATA,
    WAIT_CRC,
} MOS_RX_STATE;

//   静态变量
static MOS_RX_STATE state = WAIT_HEAD;
static uint8_t frame[4 + MOS_MAX_DATA_LEN];
static uint8_t frame_index = 0;
static uint8_t data_len = 0;

//   静态函数声明
static uint8_t MOS_Calc_CRC(const uint8_t *buf, uint16_t len);
static void MOS_Send_Reply(uint8_t cmd, const uint8_t *data, uint8_t len);
static void MOS_Protocol_Handle(const uint8_t *buf, uint8_t frame_len);

//   CRC计算
static uint8_t MOS_Calc_CRC(const uint8_t *buf, uint16_t len)
{
    uint8_t crc = 0;
    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= buf[i];
    }
    return crc;
}

//   发送回复
static void MOS_Send_Reply(uint8_t cmd, const uint8_t *data, uint8_t len)
{
    uint8_t tx[16];
    uint8_t index = 0;

    // HEAD
    tx[index++] = MOS_FRAME_HEAD;
    // ADDRESS
    tx[index++] = MOS_DEVICE_ADDR;
    // CMD
    tx[index++] = cmd;
    // LEN
    tx[index++] = len;
    // DATA
    for (uint8_t i = 0; i < len; i++)
    {
        tx[index++] = data[i];
    }
    // CRC计算（ADDRESS + CMD + LEN + DATA）
    tx[index] = MOS_Calc_CRC(&tx[1], index - 1);
    index++;
    uart_drv_send(tx, index);
}

//   执行完整协议帧
static void MOS_Protocol_Handle(const uint8_t *buf, uint8_t frame_len)
{
    uint8_t addr, cmd, len;
    ESP_LOGI(TAG, "Handle: len=%d, cmd=0x%02X", frame_len, buf[2]);
    // 最小长度检查：HEAD + ADDRESS + CMD + LEN = 4
    if (frame_len < 4)
    {
        ESP_LOGW(TAG, "帧太短: %d", frame_len);
        return;
    }

    // HEAD检查
    if (buf[0] != MOS_FRAME_HEAD)
    {
        ESP_LOGW(TAG, "帧头错误: 0x%02X", buf[0]);
        return;
    }

    // ADDRESS检查
    addr = buf[1];
    if (addr != MOS_DEVICE_ADDR)
    {
        ESP_LOGW(TAG, "地址错误: 0x%02X", addr);
        return;
    }

    // 解析命令和长度
    cmd = buf[2];
    len = buf[3];

    if (len > MOS_MAX_DATA_LEN)
    {
        ESP_LOGW(TAG, "数据长度超限: %d", len);
        return;
    }

    if (frame_len != (uint8_t)(4 + len))
    {
        ESP_LOGW(TAG, "帧长度不匹配: 期望%d, 实际%d", 4 + len, frame_len);
        return;
    }

    // 根据CMD执行
    switch (cmd)
    {

    //   单路MOS控制
    case CMD_MOS_CONTROL:
    {
        if (len != 2)
        {
            ESP_LOGW(TAG, "单路控制数据长度错误: %d", len);
            return;
        }
        uint8_t channel = buf[4];
        uint8_t mos_state = buf[5];

        if (channel >= MOS_CHANNEL_NUM)
        {
            ESP_LOGE(TAG, "通道越界: %d", channel);
            return;
        }
        if (mos_state != MOS_OFF && mos_state != MOS_ON)
        {
            ESP_LOGE(TAG, "状态错误: %d", mos_state);
            return;
        }

        ESP_LOGI(TAG, "单路控制: CH%d -> %s", channel, mos_state ? "ON" : "OFF");
        MOS_Control(channel, (MOS_State)mos_state);
        break;
    }

    //   全部MOS控制
    case CMD_MOS_ALL_CONTROL:
    {
        if (len != 1)
        {
            ESP_LOGW(TAG, "全部控制数据长度错误: %d", len);
            return;
        }
        uint8_t mos_state = buf[4];
        if (mos_state != MOS_OFF && mos_state != MOS_ON)
        {
            ESP_LOGE(TAG, "状态错误: %d", mos_state);
            return;
        }
        ESP_LOGI(TAG, "全部控制: %s", mos_state ? "ON" : "OFF");
        MOS_All_Control((MOS_State)mos_state);
        break;
    }

    //   8路控制多个设置
    case CMD_MOS_STATE_SET:
    {
        if (len != 1)
        {
            ESP_LOGW(TAG, "状态设置数据长度错误: %d", len);
            return;
        }
        uint8_t new_state = buf[4];
        ESP_LOGI(TAG, "状态设置: 0x%02X", new_state);
        for (uint8_t i = 0; i < MOS_CHANNEL_NUM; i++)
        {
            MOS_Control(i, (new_state & (1U << i)) ? MOS_ON : MOS_OFF);
        }
        break;
    }

    //   查询MOS状态
    case CMD_MOS_GET:
    {
        if (len != 0)
        {
            ESP_LOGW(TAG, "查询命令不应带数据: %d", len);
            return;
        }
        uint8_t mos_state = MOS_Get_All();
        MOS_Send_Reply(CMD_MOS_REPLY, &mos_state, 1);
        break;
    }

    //  未知命令
    default:
        ESP_LOGW(TAG, "未知命令: 0x%02X", cmd);
        break;
    }
}

// 协议初始化
void MOS_Protocol_Init(void)
{
    // 注册UART接收回调
    uart_drv_register_mos_byte_callback(MOS_Protocol_RxByte);
    // 初始化状态机
    state = WAIT_HEAD;
    frame_index = 0;
    data_len = 0;
    ESP_LOGI(TAG, "MOS Protocol initialized");
}

//  逐字节接收
void MOS_Protocol_RxByte(uint8_t ch)
{
    switch (state)
    {

    // 等待帧头
    case WAIT_HEAD:
        if (ch == MOS_FRAME_HEAD)
        {
            frame[0] = ch;
            frame_index = 1;
            data_len = 0;
            state = WAIT_ADDR;
        }
        break;

    //   ADDRESS
    case WAIT_ADDR:
        frame[frame_index++] = ch;
        state = WAIT_CMD;
        break;

    //   CMD
    case WAIT_CMD:
        frame[frame_index++] = ch;
        state = WAIT_LEN;
        break;

    //   LEN
    case WAIT_LEN:
        frame[frame_index++] = ch;
        data_len = ch;

        if (data_len > MOS_MAX_DATA_LEN)
        {
            // 非法长度，丢弃当前帧
            ESP_LOGW(TAG, "非法数据长度: %d", data_len);
            state = WAIT_HEAD;
            frame_index = 0;
            data_len = 0;
            break;
        }

        if (data_len == 0)
        {
            state = WAIT_CRC;
        }
        else
        {
            state = WAIT_DATA;
        }
        break;

    //   DATA
    case WAIT_DATA:
        frame[frame_index++] = ch;
        // frame_index = 4(HEAD+ADDR+CMD+LEN) + data_len
        if (frame_index >= (uint8_t)(4 + data_len))
        {
            state = WAIT_CRC;
        }
        break;

    //   CRC
    case WAIT_CRC:
    {
        // 计算CRC（ADDRESS + CMD + LEN + DATA）
        uint8_t crc = MOS_Calc_CRC(&frame[1], frame_index - 1);
        if (crc == ch)
        {
            // CRC正确，执行完整帧
            MOS_Protocol_Handle(frame, frame_index);
        }
        else
        {
            ESP_LOGW(TAG, "CRC错误: 计算0x%02X, 收到0x%02X", crc, ch);
        }

        // 无论CRC对错，回到等待帧头
        state = WAIT_HEAD;
        frame_index = 0;
        data_len = 0;
        break;
    }

    //   异常状态
    default:
        state = WAIT_HEAD;
        frame_index = 0;
        data_len = 0;
        break;
    }
}

//   批量接收
void MOS_Protocol_RxBytes(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        MOS_Protocol_RxByte(data[i]);
    }
}