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

static const char *TAG = "MQTT_SERVICE";



static TaskHandle_t heartbeat_handle = NULL;

/*
    静态函数声明
*/

static void mqtt_start_heartbeat(void);

static void heartbeat_task(void *arg);

/*
    上线
*/
static void mqtt_online(void)
{

    char msg[128];

    sprintf(
        msg,
        "{\"device\":\"%s\",\"status\":\"online\"}",
        DEVICE_ID);

    mqtt_manager_publish(
        TOPIC_ONLINE,
        msg,
        strlen(msg));
}
/*
    MQTT收到数据回调
*/
static void mqtt_control_callback(
    char *topic,
    char *data,
    int len)
{

    ESP_LOGI(TAG,"topic:%s", topic);

    /*
        判断是不是控制主题
    */
    if (strcmp(topic, TOPIC_CONTROL) != 0)
    {
        return;
    }

    /*
        JSON解析
    */
    char json[128];

    memset(json, 0, sizeof(json));

    if (len >= sizeof(json))
    {
        len = sizeof(json) - 1;
    }

    memcpy( json,  data,len);
    cJSON *root;
    root = cJSON_Parse(json);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "json error");
        return;
    }
    cJSON *cmd = cJSON_GetObjectItem(root, "cmd"); // TODO  这里指令字符串写死了
    if (!cmd)
    {
        cJSON_Delete(root);
        return;
    }
    /*
        MOS单路控制
    */
    if (strcmp(cmd->valuestring, "mos") == 0)
    {

        cJSON *channel = cJSON_GetObjectItem(root, "channel");

        cJSON *state = cJSON_GetObjectItem(root, "state");

        if (channel && state)
        {
            MOS_Control(
                channel->valueint,
                state->valueint ? MOS_ON : MOS_OFF);

            ESP_LOGI(
                TAG,
                "MOS%d=%d",
                channel->valueint,
                state->valueint);
        }
    }

    /*
        全部MOS控制
    */

    else if (strcmp(cmd->valuestring, "mos_all") == 0)
    {

        cJSON *state =
            cJSON_GetObjectItem(
                root,
                "state");

        if (state)
        {

            MOS_All_Control(
                state->valueint ? MOS_ON : MOS_OFF);
        }
    }

    cJSON_Delete(root);

    /*
        回复
    */

    mqtt_manager_publish(
        TOPIC_REPLY,
        "{\"result\":1}",
        strlen("{\"result\":1}"));
}

static void mqtt_status_callback(
    mqtt_status_t status)
{

    switch (status)
    {

    case MQTT_STATUS_CONNECTED:

        ESP_LOGI(
            TAG,
            "mqtt online");

        mqtt_online();
        mqtt_start_heartbeat();
        break;

    case MQTT_STATUS_DISCONNECTED:

        ESP_LOGW(
            TAG,
            "mqtt offline");

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

        sprintf(
            msg,
            "{\"device\":\"%s\",\"uptime\":%lld,\"status\":\"online\"}",
            DEVICE_ID,
            esp_timer_get_time() / 1000000);

        mqtt_manager_publish(
            TOPIC_HEART,
            msg,
            strlen(msg));

        vTaskDelay(
            pdMS_TO_TICKS(5000));
    }
}

static void mqtt_start_heartbeat(void)
{

    if (heartbeat_handle == NULL)
    {

        xTaskCreate(
            heartbeat_task,
            "mqtt_heartbeat",
            4096,
            NULL,
            5,
            &heartbeat_handle);
    }
}
/*
状态上传
*/
void mqtt_service_publish_state(void)
{

    char msg[64];

    uint8_t state;

    state = MOS_Get_All();

    sprintf(
        msg,
        "{\"mos\":%d}",
        state);

    mqtt_manager_publish(
        TOPIC_STATE,
        msg,
        strlen(msg));
}

void mqtt_service_init(void)
{
    mqtt_manager_register_callback(mqtt_control_callback);
    mqtt_manager_register_status_callback(mqtt_status_callback);
}
