#include "mqtt_service.h"
#include "mqtt_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "cJSON.h"
#include "mos.h"
#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
#include "mqtt_config.h"

static const char *TAG = "MQTT_SERVICE";
static TaskHandle_t heartbeat_handle = NULL;

/*
  静态函数声明
*/
static void mqtt_online(void);
static void mqtt_start_heartbeat(void);
static void mqtt_stop_heartbeat(void);
static void heartbeat_task(void *arg);
static void mqtt_publish_mos_event(uint8_t channel, uint8_t state);
static void mqtt_handle_mos_control(cJSON *root);
static void mqtt_handle_mos_all_control(cJSON *root);
static void mqtt_publish_error(uint16_t code, const char *msg);
/*
    上线
*/
static void mqtt_online(void)
{
    char msg[128];
    sprintf(msg, "{\"device\":\"%s\",\"status\":\"online\"}", DEVICE_ID);
    mqtt_manager_publish(TOPIC_ONLINE, msg, strlen(msg));
}
/*
    MQTT收到数据回调
*/
static void mqtt_control_callback(const char *topic, const uint8_t *data, int len)
{
    ESP_LOGI(TAG, "topic: %s, data: %.*s", topic, len, data);

    if (strcmp(topic, TOPIC_CONTROL) != 0)
    {
        return;
    }

    // 解析 JSON
    char json[256];
    if (len >= (int)sizeof(json))
    {
        len = sizeof(json) - 1;
    }
    memcpy(json, data, len);
    json[len] = '\0';

    cJSON *root = cJSON_Parse(json);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "JSON 解析失败: %s", json);
        mqtt_publish_error(1001, "json parse failed");
        return;
    }

    // 获取命令字段
    cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
    if (!cmd || !cJSON_IsString(cmd))
    {
        ESP_LOGE(TAG, "缺少 cmd 字段");
        mqtt_publish_error(1002, "missing cmd field");
        cJSON_Delete(root);
        return;
    }

    const char *cmd_str = cmd->valuestring;

    // 根据命令分发
    if (strcmp(cmd_str, CMD_MOS_SINGLE) == 0)
    {
        mqtt_handle_mos_control(root);
    }
    else if (strcmp(cmd_str, CMD_MOS_ALL) == 0)
    {
        mqtt_handle_mos_all_control(root);
    }
    else if (strcmp(cmd_str, CMD_MOS_QUERY) == 0)
    {
        mqtt_service_publish_state();
    }
    else
    {
        ESP_LOGW(TAG, "未知命令: %s", cmd_str);
        mqtt_publish_error(1003, "Unknown command");
    }

    cJSON_Delete(root);
}

static void mqtt_handle_mos_control(cJSON *root)
{
    cJSON *channel = cJSON_GetObjectItem(root, "channel");
    cJSON *state = cJSON_GetObjectItem(root, "state");

    if (!channel || !state)
    {
        ESP_LOGE(TAG, "Missing the channel or state fields");
        mqtt_publish_error(1004, "Unknown command");
        return;
    }

    if (!cJSON_IsNumber(channel) || !cJSON_IsNumber(state))
    {
        ESP_LOGE(TAG, "channel 或 state 不是数字");
        mqtt_publish_error(1005, "The channel or state is not a number.");
        return;
    }

    uint8_t ch = (uint8_t)channel->valueint;
    uint8_t st = (uint8_t)state->valueint;

    if (ch >= MOS_CHANNEL_NUM)
    {
        ESP_LOGE(TAG, "通道越界: %d", ch);
        mqtt_publish_error(1006, "Channel boundary crossing");
        return;
    }
    if (MOS_Control(ch, st ? MOS_ON : MOS_OFF))
    {
        mqtt_publish_mos_event(ch, st);
    }
    else
    {
        mqtt_publish_error(1007, "control fail");
    }
}

/*
    全部 MOS 控制
*/
static void mqtt_handle_mos_all_control(cJSON *root)
{
    cJSON *state = cJSON_GetObjectItem(root, "state");

    if (!state || !cJSON_IsNumber(state))
    {
        ESP_LOGE(TAG, "缺少 state 字段");
        mqtt_publish_error(1007, "Missing state field");
        return;
    }

    MOS_All_Control(state->valueint ? MOS_ON : MOS_OFF);
    mqtt_service_publish_state();
}

/*
    状态回调
*/
static void mqtt_status_callback(mqtt_status_t status)
{
    switch (status)
    {
    case MQTT_STATUS_CONNECTED:
        ESP_LOGI(TAG, "MQTT 已连接");
        mqtt_online();
        mqtt_service_publish_state();
        mqtt_start_heartbeat();
        break;

    case MQTT_STATUS_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT 已断开");
        mqtt_stop_heartbeat();
        break;

    default:
        break;
    }
}

/*
    心跳任务
*/
static void heartbeat_task(void *arg)
{
    char msg[128];
    while (1)
    {
        snprintf(msg, sizeof(msg),
                 "{\"device\":\"%s\",\"uptime\":%lld,\"status\":\"online\"}",
                 DEVICE_ID,
                 esp_timer_get_time() / 1000000);

        mqtt_manager_publish(TOPIC_HEART, msg, strlen(msg));
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
    }
}

/*
    启动心跳
*/
static void mqtt_start_heartbeat(void)
{
    if (heartbeat_handle == NULL)
    {
        xTaskCreate(heartbeat_task, "mqtt_heartbeat", 4096, NULL, 5, &heartbeat_handle);
    }
}
/*
    停止心跳
*/
static void mqtt_stop_heartbeat(void)
{
    if (heartbeat_handle != NULL)
    {
        vTaskDelete(heartbeat_handle);
        heartbeat_handle = NULL;
        ESP_LOGI(TAG, "心跳已停止");
    }
}

/*
    发布 MOS 事件
*/
static void mqtt_publish_mos_event(uint8_t channel, uint8_t state)
{
    char msg[64];
    snprintf(msg, sizeof(msg),
             "{\"type\":\"%s\",\"channel\":%d,\"state\":%d}",
             EVENT_MOS_CHANGE, channel, state);

    mqtt_manager_publish(TOPIC_EVENT, msg, strlen(msg));
}

/*
    发布错误事件
*/
static void mqtt_publish_error(uint16_t code, const char *msg)
{
    char data[128];
    snprintf(
        data,
        sizeof(data),
        "{"
        "\"type\":\"error\","
        "\"code\":%d,"
        "\"msg\":\"%s\""
        "}",
        code,
        msg);

    mqtt_manager_publish(TOPIC_EVENT, data, strlen(data));

    ESP_LOGW(TAG, "MQTT ERROR:%s", data);
}

/*
    发布 MOS 状态
*/
void mqtt_service_publish_state(void)
{
    char msg[128];
    uint8_t state = MOS_Get_All();
    // 使用循环动态生成
    snprintf(msg, sizeof(msg),
             "{\"mos0\":%d,\"mos1\":%d,\"mos2\":%d,\"mos3\":%d,"
             "\"mos4\":%d,\"mos5\":%d,\"mos6\":%d,\"mos7\":%d}",
             (state >> 0) & 1,
             (state >> 1) & 1,
             (state >> 2) & 1,
             (state >> 3) & 1,
             (state >> 4) & 1,
             (state >> 5) & 1,
             (state >> 6) & 1,
             (state >> 7) & 1);
    mqtt_manager_publish(TOPIC_STATE, msg, strlen(msg));
}

/*
    发布 MOS 事件（外部调用）
*/
void mqtt_service_publish_mos_event(uint8_t channel, uint8_t state)
{
    mqtt_publish_mos_event(channel, state);
}

/*
    服务初始化
*/
void mqtt_service_init(void)
{
    mqtt_manager_register_callback(mqtt_control_callback);
    mqtt_manager_register_status_callback(mqtt_status_callback);
    ESP_LOGI(TAG, "MQTT Service 初始化完成");
}