#include "mqtt_manager.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "mqtt_service.h"
#include <string.h>
#include "mqtt_config.h"

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t mqtt_client = NULL;
static mqtt_rx_callback_t rx_callback = NULL;
static mqtt_status_callback_t status_callback = NULL;
/*
  静态函数声明
*/
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

/*
    MQTT事件处理
*/
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch (event_id)
    {
    // MQTT连接成功
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        esp_mqtt_client_subscribe(mqtt_client, TOPIC_CONTROL, 1);
        if (status_callback)
        {
            status_callback(MQTT_STATUS_CONNECTED);
        }
        break;
    // 收到数据
    case MQTT_EVENT_DATA:
    {
        char topic[128];
        memset(topic, 0, sizeof(topic));
        if (event->topic_len < sizeof(topic))
        {
            memcpy(topic, event->topic, event->topic_len);
        }
        if (rx_callback)
        {
            rx_callback(topic, (const uint8_t *)event->data, event->data_len);
        }
    }
    break;
    // 断开
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
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
        .session.last_will = {
            .topic = WILL_TOPIC,
            .msg = WILL_MSG,
            .msg_len = strlen(WILL_MSG),
            .qos = WILL_QOS,
            .retain = WILL_RETAIN,
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