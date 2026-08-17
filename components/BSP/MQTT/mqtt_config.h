#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

// ==================== MQTT Broker 配置 ====================
#define MQTT_BROKER_URI "mqtt://192.168.124.6:1883"
#define MQTT_USERNAME "MQTT1"
#define MQTT_PASSWORD "123456"

// ==================== 遗嘱配置 ====================
#define WILL_QOS 1
#define WILL_RETAIN true
#define KEEPALIVE 30
#define TIMEOUT 5000

// ==================== JSON 字段名 ====================
#define JSON_DEVICE "device"
#define JSON_PRODUCT "product"
#define JSON_STATUS "status"
#define JSON_ONLINE "online"
#define JSON_OFFLINE "offline"
#define JSON_TYPE "type"
#define JSON_CHANNEL "channel"
#define JSON_STATE "state"
#define JSON_CODE "code"
#define JSON_MSG "msg"
#define JSON_UPTIME "uptime"
#define JSON_CMD "cmd"

// ==================== 状态值 ====================
#define STATUS_ONLINE "online"
#define STATUS_OFFLINE "offline"
#define STATUS_ERROR "error"

// ==================== 事件类型 ====================
#define EVENT_MOS_CHANGE "mos_change"
#define EVENT_TYPE_ERROR "error"

// ==================== MQTT 命令定义 ====================
#define CMD_MOS_SINGLE "mos"      // 单路控制
#define CMD_MOS_ALL "mos_all"     // 全部控制
#define CMD_MOS_QUERY "mos_query" // 查询状态

// ==================== 心跳配置 ====================
#define HEARTBEAT_INTERVAL_MS 5000

#endif