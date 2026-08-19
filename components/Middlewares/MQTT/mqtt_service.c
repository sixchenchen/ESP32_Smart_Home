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
#include "mqtt_manager.h"
#include "mqtt_topic.h"
#include "mqtt_message.h"

static const char *TAG = "MQTT_SERVICE";
static TaskHandle_t heartbeat_handle = NULL;

/*
  静态函数声明
*/
static void mqtt_start_heartbeat(void);
static void mqtt_stop_heartbeat(void);
static void heartbeat_task(void *arg);
static void mqtt_publish_mos_event(uint8_t channel, uint8_t state);
static void mqtt_handle_mos_control(cJSON *root);
static void mqtt_handle_mos_all_control(cJSON *root);
static void mqtt_publish_error(uint16_t code, const char *msg);
static void mqtt_publish_status(bool online);

/*
    发布状态
*/
static void mqtt_publish_status(bool online)
{
    const char *reason = online ? NULL : REASON_MQTT_LWT;
    char *msg = mqtt_message_create_online();
    mqtt_manager_publish(mqtt_topic_status(), msg, strlen(msg), 1, true);
    free(msg);
}

/*
    MQTT收到数据回调
*/
static void mqtt_control_callback(const char *topic, const uint8_t *data, int len)
{
    ESP_LOGI(TAG, "topic: %s, data: %.*s", topic, len, data);
    if (strcmp(topic, mqtt_topic_control()) != 0)
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
    cJSON *cmd = cJSON_GetObjectItem(root, JSON_CMD);
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
        mqtt_publish_mos_state();
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
    cJSON *channel = cJSON_GetObjectItem(root, JSON_CHANNEL);
    cJSON *state = cJSON_GetObjectItem(root, JSON_STATE);

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
    cJSON *state = cJSON_GetObjectItem(root, JSON_STATE);
    if (!state || !cJSON_IsNumber(state))
    {
        ESP_LOGE(TAG, "缺少 state 字段");
        mqtt_publish_error(1007, "Missing state field");
        return;
    }
    MOS_All_Control(state->valueint ? MOS_ON : MOS_OFF);
    mqtt_publish_mos_state();
}

/*
    状态回调
*/
static void mqtt_status_callback(mqtt_state_t state)
{
    switch (state)
    {
    case MQTT_STATE_RUNNING:
        ESP_LOGI(TAG, "MQTT 已连接");
        mqtt_publish_status(true);
        mqtt_publish_mos_state();
        mqtt_start_heartbeat();
        break;

    case MQTT_STATE_STOPPED:
        ESP_LOGW(TAG, "MQTT 已停止");
        mqtt_stop_heartbeat();
        break;

    case MQTT_STATE_ERROR:
        ESP_LOGE(TAG, "MQTT 错误状态");
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
    while (1)
    {
        char *msg = mqtt_message_create_heartbeat(esp_timer_get_time() / 1000000);
        if (msg != NULL)
        {
            mqtt_manager_publish(
                mqtt_topic_heart(),
                msg,
                strlen(msg),
                0,
                false);
            free(msg);
        }
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
    }
}

/*
    启动心跳
*/
static void mqtt_start_heartbeat(void)
{
    if (heartbeat_handle)
        return;
    xTaskCreate(heartbeat_task, "mqtt_heart", 4096, NULL, 5, &heartbeat_handle);
}
/*
    停止心跳
*/
static void mqtt_stop_heartbeat(void)
{

    if (heartbeat_handle)
    {
        vTaskDelete(heartbeat_handle);
        heartbeat_handle = NULL;
    }
}

/*
    发布 MOS 事件
*/
static void mqtt_publish_mos_event(uint8_t ch, uint8_t state)
{
    char *msg = mqtt_message_create_mos_event(ch, state);
    mqtt_manager_publish(mqtt_topic_event(), msg, strlen(msg), 1, false);
    free(msg);
}

/*
    发布错误事件
*/

static void mqtt_publish_error(uint16_t code, const char *msg)
{
    char *data = mqtt_message_create_error(code, msg);
    if (data != NULL)
    {
        mqtt_manager_publish(mqtt_topic_event(), data, strlen(data), 1, false);
        free(data);
    }
    ESP_LOGW(TAG, "MQTT ERROR: code=%d, msg=%s", code, msg);
}

/*
    发布 MOS 状态
*/
void mqtt_publish_mos_state(void)
{
    // 主题
    const char *topic = mqtt_topic_mos_state();
    // 创建消息
    uint8_t state = MOS_Get_All();
    char *msg = mqtt_message_create_mos_state(state);
    // 发布
    esp_err_t ret = mqtt_manager_publish(topic, msg, strlen(msg), 1, true);
    free(msg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "发布 MOS 状态失败: %d", ret);
    }
    else
    {
        ESP_LOGI(TAG, "发布 MOS 状态成功");
    }
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
    esp_err_t ret = mqtt_manager_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT 初始化失败");
        return;
    }
    mqtt_manager_register_callback(mqtt_control_callback);
    mqtt_manager_register_status_callback(mqtt_status_callback);
    ESP_LOGI(TAG, "MQTT Service 初始化完成");
}