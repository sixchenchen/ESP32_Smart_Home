#ifndef __MQTT_CONFIG_H__
#define __MQTT_CONFIG_H__

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
// ==================== MQTT Broker 配置 ====================
#define MQTT_BROKER_URI "mqtt://192.168.124.6:1883"
#define MQTT_USERNAME "MQTT1"
#define MQTT_PASSWORD "123456"
#define MQTT_CONFIG_NAMESPACE "mqtt_config"
// ==================== MQTT 参数 ====================
#define WILL_QOS 1
#define WILL_RETAIN true
#define KEEPALIVE 30
#define TIMEOUT 5000
// ==================== NVS Keys ====================
#define MQTT_KEY_BROKER_URI "broker_uri"
#define MQTT_KEY_CLIENT_ID "client_id"
#define MQTT_KEY_USERNAME "username"
#define MQTT_KEY_PASSWORD "password"
#define MQTT_KEY_WILL_TOPIC "will_topic"
#define MQTT_KEY_PROVISIONED "provisioned"
// ==================== JSON KEY ====================
#define JSON_DEVICE "device"
#define JSON_PRODUCT "product"
#define JSON_TYPE "type"
#define JSON_TIMESTAMP "timestamp"
#define JSON_DATA "data"
// data内部字段
#define JSON_EVENT "event"
#define JSON_REASON "reason"
#define JSON_UPTIME "uptime"
#define JSON_RSSI "wifi_rssi"
#define JSON_CHANNEL "channel"
#define JSON_STATE "state"
#define JSON_CODE "code"
#define JSON_MESSAGE "message"
#define JSON_SUCCESS "success"
// ==================== 消息类型 ====================
#define TYPE_HEARTBEAT "heartbeat"
#define TYPE_EVENT "event"
#define TYPE_STATE "state"
#define TYPE_ERROR "error"
// ==================== 设备状态值 ====================
#define STATE_ONLINE "online"
#define STATE_OFFLINE "offline"
// ==================== OFFLINE原因 ====================
#define REASON_MQTT_LWT "mqtt_lwt"
// ==================== 事件类型 ====================
#define EVENT_MOS_CHANGE "mos_change"
#define EVENT_FACTORY_RESET "factory_reset"
// ==================== MQTT控制命令 ====================
#define JSON_CMD "cmd"
#define CMD_MOS_SINGLE "mos"
#define CMD_MOS_ALL "mos_all"
#define CMD_MOS_QUERY "mos_query"
// ==================== 心跳 ====================
#define HEARTBEAT_INTERVAL_MS 30000

// ==================== MQTT 配置结构体 ====================
typedef struct
{
    char broker_uri[128]; // Broker 地址
    char client_id[64];   // 客户端 ID(MAC 地址)
    char username[64];    // 用户名
    char password[64];    // 密码
    char will_topic[128]; // 遗嘱主题
    bool is_provisioned;  // 是否已完成注册
} mqtt_config_t;

/*
    加载 MQTT 配置
 */
esp_err_t mqtt_config_load(mqtt_config_t *config);

/*
    保存 MQTT 配置到 NVS
 */
esp_err_t mqtt_config_save(const mqtt_config_t *config);

/*
     清除 MQTT 配置（恢复出厂时调用）
 */
esp_err_t mqtt_config_clear(void);

/*
     检查是否已完成注册
 */
bool mqtt_config_is_provisioned(void);

/*
     获取默认临时服务器配置（用于首次注册）
 */
void mqtt_config_get_default(mqtt_config_t *config);

#endif