#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

// ==================== MQTT Broker 配置 ====================
#define MQTT_BROKER_URI "mqtt://192.168.124.6:1883"
#define MQTT_USERNAME "MQTT1"
#define MQTT_PASSWORD "123456"
// ==================== MQTT 参数 ====================
#define WILL_QOS 1
#define WILL_RETAIN true
#define KEEPALIVE 30
#define TIMEOUT 5000
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
#define HEARTBEAT_INTERVAL_MS 5000
#endif