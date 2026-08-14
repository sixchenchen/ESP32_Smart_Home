#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

// MQTT Broker 配置
#define MQTT_BROKER_URI "mqtt://192.168.124.6:1883"
#define MQTT_USERNAME "MQTT1"
#define MQTT_PASSWORD "123456"

// 设备信息
#define DEVICE_ID "device001"
#define DEVICE_NAME "ESP32_Smart_Home"

// MQTT 主题
#define TOPIC_CONTROL "device/" DEVICE_ID "/control"
#define TOPIC_STATUS "device/" DEVICE_ID "/status"
#define TOPIC_STATE "device/" DEVICE_ID "/state"
#define TOPIC_EVENT "device/" DEVICE_ID "/event"
#define TOPIC_ONLINE "device/" DEVICE_ID "/online"
#define TOPIC_OFFLINE "device/" DEVICE_ID "/offline"
#define TOPIC_HEART "device/" DEVICE_ID "/heart"

// 遗嘱消息
#define WILL_TOPIC TOPIC_OFFLINE
#define WILL_MSG "{\"device\":\"" DEVICE_ID "\",\"status\":\"offline\"}"
#define WILL_QOS 1
#define WILL_RETAIN 1

// MQTT 命令定义
#define CMD_MOS_SINGLE "mos"      // 单路控制
#define CMD_MOS_ALL "mos_all"     // 全部控制
#define CMD_MOS_QUERY "mos_query" // 查询状态

// MQTT 事件类型
#define EVENT_MOS_CHANGE "mos_change"

// 心跳配置
#define HEARTBEAT_INTERVAL_MS 5000
#endif