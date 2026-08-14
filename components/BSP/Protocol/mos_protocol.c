#include "mos_protocol.h"
#include "mos.h"
#include "uart_drv.h"

// 当前状态
static MOS_RX_STATE state = WAIT_HEAD;
// 当前最大帧数：HEAD ADDRESS CMD LEN DATA(8) 共12字节 CRC单独接收
static uint8_t frame[4 + MOS_MAX_DATA_LEN];
static uint8_t frame_index = 0;
// DATA长度
static uint8_t data_len = 0;
/*
  静态函数声明
*/
static uint8_t MOS_Calc_CRC(const uint8_t *buf, uint16_t len);
static void MOS_Send_Reply(uint8_t cmd, const uint8_t *data, uint8_t len);
static void MOS_Protocol_Handle(const uint8_t *buf, uint8_t frame_len);

// CRC计算(范围)： ADDRESS CMD LEN DATA 不包含HEAD
static uint8_t MOS_Calc_CRC(const uint8_t *buf, uint16_t len)
{
    uint8_t crc = 0;
    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= buf[i];
    }
    return crc;
}

// 发送回复
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
    // CRC计算： ADDRESS  CMD LEN DATA
    tx[index] = MOS_Calc_CRC(&tx[1], index - 1);
    index++;
    // UART发送
    uart_drv_send(tx, index);
}

/*
    执行完整协议帧
    frame格式：
    frame[0] = HEAD
    frame[1] = ADDRESS
    frame[2] = CMD
    frame[3] = LEN
    frame[4...] = DATA
    frame_len不包含CRC
*/
static void MOS_Protocol_Handle(const uint8_t *buf, uint8_t frame_len)
{
    uint8_t addr;
    uint8_t cmd;
    uint8_t len;
    // 最小：HEAD + ADDRESS + CMD + LEN = 4
    if (frame_len < 4)
    {
        return;
    }
    // HEAD
    if (buf[0] != MOS_FRAME_HEAD)
    {
        return;
    }
    // ADDRESS
    addr = buf[1];
    if (addr != MOS_DEVICE_ADDR)
    {
        return;
    }
    //  CMD
    cmd = buf[2];
    // LEN
    len = buf[3];
    // 防止DATA越界
    if (len > MOS_MAX_DATA_LEN)
    {
        return;
    }
    // 判断: HEAD ADDRESS CMD LEN DATA， 总长度 = 4 + LEN
    if (frame_len != (uint8_t)(4 + len))
    {
        return;
    }
    // 根据CMD执行
    switch (cmd)
    {
    /*
        单路MOS控制
        DATA[0] = channel
        DATA[1] = state
    */
    case CMD_MOS_CONTROL:
    {
        uint8_t channel;
        uint8_t mos_state;
        // 必须有两个DATA
        if (len != 2)
        {
            return;
        }
        channel = buf[4];
        mos_state = buf[5];
        // 通道检查
        if (channel >= MOS_CHANNEL_NUM)
        {
            return;
        }
        // 状态只允许： 0 = OFF, 1 = ON
        if (mos_state != MOS_OFF && mos_state != MOS_ON)
        {
            return;
        }
        // 执行控制
        MOS_Control(channel, (MOS_State)mos_state);
    }
    break;
    // 全部MOS控制DATA[0]: 0 = OFF,1 = ON
    case CMD_MOS_ALL_CONTROL:
    {
        uint8_t mos_state;
        // 必须1个DATA
        if (len != 1)
        {
            return;
        }
        mos_state = buf[4];
        // 状态检查
        if (mos_state != MOS_OFF && mos_state != MOS_ON)
        {
            return;
        }
        //  全部控制
        MOS_All_Control((MOS_State)mos_state);
    }
    break;
    /*
        8路状态设置
        DATA[0]:
        bit0 -> MOS0
        bit1 -> MOS1
        ...
        bit7 -> MOS7
        例如：0x05-> 00000101
        MOS0 ON
        MOS1 OFF
        MOS2 ON
    */
    case CMD_MOS_STATE_SET:
    {
        uint8_t new_state;
        // 必须1个DATA
        if (len != 1)
        {
            return;
        }
        new_state = buf[4];
        //  根据bit控制8路
        for (uint8_t i = 0; i < MOS_CHANNEL_NUM; i++)
        {
            if (new_state & (1U << i))
            {
                MOS_Control(i, MOS_ON);
            }
            else
            {
                MOS_Control(i, MOS_OFF);
            }
        }
    }
    break;

    // 查询MOS状态 LEN = 0
    case CMD_MOS_GET:
    {
        uint8_t mos_state;
        // 查询命令不能带DATA
        if (len != 0)
        {
            return;
        }
        // 获取全部状态
        mos_state = MOS_Get_All();
        // 回复： CMD = 0x21,DATA[0] = MOS状态
        MOS_Send_Reply(CMD_MOS_REPLY, &mos_state, 1);
    }
    break;

    //  未知命令
    default:
    {
        return;
    }
    }
}

/*
    初始化
*/
void MOS_Protocol_Init(void)
{
    /*
        UART接收一个字节后，调用MOS_Protocol_RxByte()
    */
    uart_drv_register_callback(MOS_Protocol_RxByte);
    //  初始化状态机
    state = WAIT_HEAD;
    frame_index = 0;
    data_len = 0;
}

/*
    协议接收状态机
*/
void MOS_Protocol_RxByte(uint8_t ch)
{
    switch (state)
    {
    /*
        等待帧头
    */
    case WAIT_HEAD:
        if (ch == MOS_FRAME_HEAD)
        {
            // 保存HEAD
            frame[0] = ch;
            // 下一字节写入frame[1]
            frame_index = 1;
            // 清空长度
            data_len = 0;
            //  等待ADDRESS
            state = WAIT_ADDR;
        }
        break;
        // ADDRESS
    case WAIT_ADDR:
        // 保存ADDRESS
        frame[frame_index++] = ch;
        // 下一步CMD
        state = WAIT_CMD;
        break;
    // CMD
    case WAIT_CMD:
        // 保存CMD
        frame[frame_index++] = ch;
        // LEN
        state = WAIT_LEN;
        break;
    // LEN
    case WAIT_LEN:
        // 保存LEN
        frame[frame_index++] = ch;
        // 保存DATA长度
        data_len = ch;
        // 判断数据长度是否合法
        if (data_len > MOS_MAX_DATA_LEN)
        {
            // 非法长度，丢弃当前帧
            state = WAIT_HEAD;
            frame_index = 0;
            data_len = 0;
            break;
        }
        // LEN=0，没有DATA，下一字节就是CRC
        if (data_len == 0)
        {
            state = WAIT_CRC;
        }
        else
        {
            // 有DATA
            state = WAIT_DATA;
        }
        break;
    //  DATA
    case WAIT_DATA:
        // 保存DATA
        frame[frame_index++] = ch;
        /*
            判断DATA是否收完
            frame_index：
            HEAD      1
            ADDRESS   1
            CMD       1
            LEN       1
            DATA      N
            所以：frame_index = 4 + data_len
        */
        if (frame_index >= (uint8_t)(4 + data_len))
        {
            // DATA收完，下一字节就是CRC
            state = WAIT_CRC;
        }
        break;

    // CRC
    case WAIT_CRC:
    {
        uint8_t crc;
        /*
            CRC计算范围：
            frame[1]
            ADDRESS
            frame[2]
            CMD
            frame[3]
            LEN
            frame[4...]
            DATA
        */
        crc = MOS_Calc_CRC(&frame[1], frame_index - 1);

        // 比较收到的CRC
        if (crc == ch)
        {
            // CRC正确，执行完整帧
            MOS_Protocol_Handle(frame, frame_index);
        }
        // 当前帧结束，无论CRC对错，都回到等待下一个AA
        state = WAIT_HEAD;
        frame_index = 0;
        data_len = 0;
    }
    break;
    // 异常状态
    default:
        state = WAIT_HEAD;
        frame_index = 0;
        data_len = 0;
        break;
    }
}