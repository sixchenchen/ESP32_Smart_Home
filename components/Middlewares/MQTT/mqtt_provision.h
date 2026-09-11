
// mqtt_provision.h
#ifndef MQTT_PROVISION_H
#define MQTT_PROVISION_H

#include <stdbool.h>
#include "esp_err.h"
#include "mqtt_config.h"

// ==================== 注册状态 ====================
typedef enum
{
    PROV_STATE_IDLE = 0,   // 空闲
    PROV_STATE_CONNECTING, // 连接临时服务器
    PROV_STATE_SENDING,    // 发送注册请求
    PROV_STATE_WAITING,    // 等待响应
    PROV_STATE_SUCCESS,    // 注册成功
    PROV_STATE_FAILED,     // 注册失败
    PROV_STATE_TIMEOUT,    // 超时
} provision_state_t;

// ==================== 注册配置 ====================
#define PROV_MAX_RETRY 10          // 最大重试次数
#define PROV_TIMEOUT_MS 30000     // 等待响应超时时间(ms)
#define PROV_TASK_STACK_SIZE 4096 // 注册任务栈大小
#define PROV_TASK_PRIORITY 5      // 注册任务优先级

/**
 * @brief 启动设备注册流程
 *        在 WiFi 连接成功后调用
 * @return ESP_OK 成功启动
 */
esp_err_t mqtt_provision_start(void);

/**
 * @brief 处理来自临时服务器的配置响应
 *        在 MQTT 数据回调中调用
 * @param data 消息数据
 * @param len 数据长度
 *
    主题：/provision/device/B4BFE90CDBA0/config/response
    数据：
    {
        "status": "success",
        "config": {
            "broker_uri": "mqtt://192.168.124.6:1883",
            "client_id": "B4BFE90CDBA0",
            "username": "MQTT1",
            "password": "123456",
            "will_topic": "device/B4BFE90CDBA0/will"
        }
    }
 */
void mqtt_provision_handle_response(const uint8_t *data, int len);

/**
 * @brief 检查注册是否完成
 * @return true 注册成功，false 未完成
 */
bool mqtt_provision_is_done(void);

/**
 * @brief 获取注册状态
 */
provision_state_t mqtt_provision_get_state(void);

/**
 * @brief 获取注册状态字符串
 */
const char *mqtt_provision_get_state_string(void);

/**
 * @brief 重置注册状态（恢复出厂时调用）
 */
void mqtt_provision_reset(void);

/**
 * @brief 注册任务初始化（创建任务）
 */
void mqtt_provision_init(void);

#endif // MQTT_PROVISION_H