#include "mqtt_message.h"
#include "json_builder.h"
#include "device_context.h"
#include "mqtt_config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "esp_timer.h"

// 获取设备信息
static const device_context_t *get_device(void)
{
    return device_context_get();
}

// 创建基础消息（所有消息共用）
static cJSON *create_base_message(const char *type)
{
    const device_context_t *dev = get_device();
    cJSON *root = json_create_object();
    json_add_string(root, JSON_DEVICE, dev->device_id);
    json_add_string(root, JSON_PRODUCT, dev->product_id);
    json_add_string(root, JSON_TYPE, type);
    return root;
}

/*
    创建基础消息 + 数据
*/
static cJSON *create_message_with_data(const char *type, cJSON *data)
{
    cJSON *root = create_base_message(type);
    if (data != NULL)
    {
        cJSON_AddItemToObject(root, JSON_DATA, data);
    }
    return root;
}

/*
    LWT遗嘱消息，MQTT异常断开，Broker自动发布
    格式: {"device":"xxx","product":"xxx","type":"offline","data":{"reason":"mqtt_lwt"}}
*/
char *mqtt_message_create_will(void)
{
    cJSON *data = json_create_object();
    json_add_string(data, JSON_REASON, REASON_MQTT_LWT);
    cJSON *root = create_message_with_data(STATE_OFFLINE, data);
    return json_finish(root);
}

/*
    在线/离线状态消息
    格式: {"device":"xxx","product":"xxx","type":"online"}
    或: {"device":"xxx","product":"xxx","type":"offline","data":{"reason":"factory_reset"}}
*/
char *mqtt_message_create_state(const char *state, const char *reason)
{
    cJSON *root = create_base_message(TYPE_STATE);
    cJSON *data = json_create_object();
    json_add_string(data, "state", state);
    if (reason != NULL)
    {
        json_add_string(data, JSON_REASON, reason);
    }
    cJSON_AddItemToObject(root, JSON_DATA, data);
    return json_finish(root);
}

/*
    在线状态消息
*/
char *mqtt_message_create_online(void)
{
    return mqtt_message_create_state(STATE_ONLINE, NULL);
}

/*
    离线状态消息
*/
char *mqtt_message_create_offline(const char *reason)
{
    return mqtt_message_create_state(STATE_OFFLINE, reason);
}

/*
    恢复出厂事件
    格式: {"device":"xxx","product":"xxx","type":"event","data":{"event":"factory_reset"}}
*/
char *mqtt_message_create_factory_reset(void)
{
    cJSON *data = json_create_object();
    json_add_string(data, JSON_EVENT, EVENT_FACTORY_RESET);
    cJSON *root = create_message_with_data(TYPE_EVENT, data);
    return json_finish(root);
}

/*
    MOS状态变化事件
    格式: {"device":"xxx","product":"xxx","type":"event","data":{"event":"mos_change","channel":0,"state":1}}
*/
char *mqtt_message_create_mos_event(uint8_t channel, uint8_t state)
{
    cJSON *data = json_create_object();
    json_add_string(data, JSON_EVENT, EVENT_MOS_CHANGE);
    json_add_number(data, JSON_CHANNEL, channel);
    json_add_number(data, JSON_STATE, state);
    cJSON *root = create_message_with_data(TYPE_EVENT, data);
    return json_finish(root);
}

/*
    心跳消息
    格式: {"device":"xxx","product":"xxx","type":"heartbeat","data":{"uptime":1234}}
*/
char *mqtt_message_create_heartbeat(uint32_t uptime)
{
    cJSON *data = json_create_object();
    json_add_number(data, JSON_UPTIME, uptime);
    cJSON *root = create_message_with_data(TYPE_HEARTBEAT, data);
    return json_finish(root);
}

/*
    错误消息
    格式: {"device":"xxx","product":"xxx","type":"error","data":{"code":1001,"message":"xxx"}}
*/
char *mqtt_message_create_error(uint16_t code, const char *msg)
{
    cJSON *data = json_create_object();
    json_add_number(data, JSON_CODE, code);
    json_add_string(data, JSON_MESSAGE, msg);
    cJSON *root = create_message_with_data(TYPE_ERROR, data);
    return json_finish(root);
}

/*
    MOS状态消息
    格式: {"device":"xxx","product":"xxx","type":"state","data":{"mos0":0,"mos1":1,...}}
*/
char *mqtt_message_create_mos_state(uint8_t mos_state)
{
    cJSON *data = json_create_object();
    json_add_number(data, "mos0", (mos_state >> 0) & 1);
    json_add_number(data, "mos1", (mos_state >> 1) & 1);
    json_add_number(data, "mos2", (mos_state >> 2) & 1);
    json_add_number(data, "mos3", (mos_state >> 3) & 1);
    json_add_number(data, "mos4", (mos_state >> 4) & 1);
    json_add_number(data, "mos5", (mos_state >> 5) & 1);
    json_add_number(data, "mos6", (mos_state >> 6) & 1);
    json_add_number(data, "mos7", (mos_state >> 7) & 1);
    cJSON *root = create_message_with_data(TYPE_STATE, data);
    return json_finish(root);
}
/*
    传感器数据上报（单条）
    格式: {"device":"xxx","product":"xxx","type":"sensor_data","data":{"sensor_id":1,"timestamp":12345,"count":100}}
*/
char *mqtt_message_create_sensor_data(uint8_t sensor_id, uint32_t timestamp_ms, uint16_t count)
{
    cJSON *root = create_base_message("sensor_data");
    cJSON *data = json_create_object();
    json_add_number(data, "sensor_id", sensor_id);
    json_add_number(data, "timestamp", timestamp_ms);
    json_add_number(data, "count", count);
    cJSON_AddItemToObject(root, JSON_DATA, data);
    return json_finish(root);
}

/*
    传感器批量数据上报
    格式: {"device":"xxx","product":"xxx","type":"sensor_batch","data":[{"sensor_id":1,"timestamp":12345,"count":100},...]}
*/
char *mqtt_message_create_sensor_batch(const uint8_t *data, uint8_t count)
{
#define ITEM_SIZE 7 // sensor_id(1) + timestamp(4) + count(2)

    const device_context_t *dev = get_device();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, JSON_DEVICE, dev->device_id);
    cJSON_AddStringToObject(root, JSON_PRODUCT, dev->product_id);
    cJSON_AddStringToObject(root, "type", "sensor_batch");
    cJSON_AddNumberToObject(root, "timestamp", esp_timer_get_time() / 1000000);

    cJSON *batch = cJSON_CreateArray();

    for (uint8_t i = 0; i < count; i++)
    {
        uint8_t offset = i * ITEM_SIZE;

        uint8_t sensor_id = data[offset];
        uint32_t timestamp = ((uint32_t)data[offset + 1] << 0) |
                             ((uint32_t)data[offset + 2] << 8) |
                             ((uint32_t)data[offset + 3] << 16) |
                             ((uint32_t)data[offset + 4] << 24);
        uint16_t cnt = ((uint16_t)data[offset + 5] << 0) |
                       ((uint16_t)data[offset + 6] << 8);

        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "sensor_id", sensor_id);
        cJSON_AddNumberToObject(item, "timestamp", timestamp);
        cJSON_AddNumberToObject(item, "count", cnt);
        cJSON_AddItemToArray(batch, item);
    }

    cJSON_AddItemToObject(root, "data", batch);
    return json_finish(root);
}