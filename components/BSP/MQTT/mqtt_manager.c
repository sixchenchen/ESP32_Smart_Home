#include "mqtt_manager.h"

#include "mqtt_client.h"
#include "esp_log.h"
#include "mqtt_service.h"
#include <string.h>

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t mqtt_client = NULL;
static mqtt_rx_callback_t rx_callback = NULL;
static mqtt_status_callback_t status_callback = NULL;

/*
    MQTT事件处理
*/
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch (event_id)
    {
    /*
        MQTT连接成功
    */
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        esp_mqtt_client_subscribe(mqtt_client, TOPIC_CONTROL, 1);
        if (status_callback)
        {
            status_callback(MQTT_STATUS_CONNECTED);
        }
        break;
    /*
        收到数据
    */
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT DATA");
        if (rx_callback)
        {
            rx_callback(event->topic, event->data, event->data_len);
        }
        break;

    /*
        断开
    */
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        if (status_callback)
        {
            status_callback(MQTT_STATUS_DISCONNECTED);
        }
        break;
    default:
        break;
    }
}

esp_err_t mqtt_manager_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg =
        {
            .broker.address.uri = "mqtt://192.168.124.6:1883",
            .credentials.username = "MQTT1",
            .credentials.authentication.password = "123456",
            /*
                遗嘱消息
            */
            .session =
                {.last_will = {
                     .topic = "device/device001/offline",
                     .msg = "{\"device\":\"device001\",\"status\":\"offline\"}",
                     .msg_len =
                         strlen("{\"device\":\"device001\",\"status\":\"offline\"}"),
                     .qos = 1,
                     .retain = 1}

                }};

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL)
    {
        return ESP_FAIL;
    }
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    ESP_LOGI(TAG, "mqtt manager init");
    return ESP_OK;
}

esp_err_t mqtt_manager_start(void)
{
    return esp_mqtt_client_start(mqtt_client);
}

esp_err_t mqtt_manager_publish(const char *topic, const char *data, int len)
{
    if (mqtt_client == NULL)
        return ESP_FAIL;
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, data, len, 1, 0);
    if (msg_id < 0)
    {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "publish id=%d", msg_id);
    return ESP_OK;
}

void mqtt_manager_register_callback(mqtt_rx_callback_t callback)
{
    rx_callback = callback;
}
void mqtt_manager_register_status_callback(mqtt_status_callback_t callback)
{
    status_callback = callback;
}