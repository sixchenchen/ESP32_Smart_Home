#include "mqtt_manager.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include <string.h>
#include "mqtt_config.h"

static const char *TAG = "MQTT";

static esp_mqtt_client_handle_t mqtt_client = NULL;
static mqtt_rx_callback_t rx_callback = NULL;
static mqtt_status_callback_t status_callback = NULL;

// MQTT 状态
static mqtt_state_t mqtt_state = MQTT_STATE_UNINIT;

//  状态转字符串 
static const char* state_to_string(mqtt_state_t state)
{
    switch (state) {
        case MQTT_STATE_UNINIT:   return "UNINIT";
        case MQTT_STATE_INIT:     return "INIT";
        case MQTT_STATE_STARTING: return "STARTING";
        case MQTT_STATE_RUNNING:  return "RUNNING";
        case MQTT_STATE_STOPPING: return "STOPPING";
        case MQTT_STATE_STOPPED:  return "STOPPED";
        case MQTT_STATE_ERROR:    return "ERROR";
        default:                  return "UNKNOWN";
    }
}

//  状态变更 
static void mqtt_set_state(mqtt_state_t new_state)
{
    if (mqtt_state != new_state) {
        mqtt_state = new_state;
        ESP_LOGI(TAG, "状态变更: %s", state_to_string(new_state));
        if (status_callback) {
            status_callback(new_state);
        }
    }
}

//  MQTT事件处理 
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event_id) {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, " MQTT 连接成功");
        esp_mqtt_client_subscribe(mqtt_client, TOPIC_CONTROL, 1);
        ESP_LOGI(TAG, "已订阅: %s", TOPIC_CONTROL);
        mqtt_set_state(MQTT_STATE_RUNNING);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT 断开");
        // 状态由 WiFi 驱动改变
        break;

    case MQTT_EVENT_DATA: {
        char topic[128] = {0};
        if (event->topic_len < sizeof(topic)) {
            memcpy(topic, event->topic, event->topic_len);
        }
        if (rx_callback) {
            rx_callback(topic, (const uint8_t *)event->data, event->data_len);
        }
        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT 错误");
        mqtt_set_state(MQTT_STATE_ERROR);
        break;

    default:
        break;
    }
}

// 初始化 
esp_err_t mqtt_manager_init(void)
{
    if (mqtt_client != NULL) {
        ESP_LOGW(TAG, "MQTT 已初始化");
        return ESP_OK;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
        .session = {
            .last_will = {
                .topic = WILL_TOPIC,
                .msg = WILL_MSG,
                .msg_len = strlen(WILL_MSG),
                .qos = WILL_QOS,
                .retain = WILL_RETAIN,
            },
            .keepalive = 60,
        },
        .network = {
            .timeout_ms = 10000,
            .disable_auto_reconnect = false,
        }
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT 初始化失败");
        mqtt_set_state(MQTT_STATE_ERROR);
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    mqtt_set_state(MQTT_STATE_INIT);
    ESP_LOGI(TAG, "MQTT 初始化完成");
    return ESP_OK;
}

// 启动 
esp_err_t mqtt_manager_start(void)
{
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT 未初始化");
        return ESP_FAIL;
    }

    if (mqtt_state == MQTT_STATE_RUNNING || mqtt_state == MQTT_STATE_STARTING) {
        ESP_LOGW(TAG, "MQTT 已启动或正在启动");
        return ESP_OK;
    }

    mqtt_set_state(MQTT_STATE_STARTING);
    esp_err_t ret = esp_mqtt_client_start(mqtt_client);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "MQTT 启动成功，等待连接...");
    } else {
        ESP_LOGE(TAG, "MQTT 启动失败: %d", ret);
        mqtt_set_state(MQTT_STATE_ERROR);
    }
    return ret;
}

// 停止
esp_err_t mqtt_manager_stop(void)
{
    if (mqtt_client == NULL) {
        return ESP_OK;
    }

    if (mqtt_state == MQTT_STATE_STOPPED || mqtt_state == MQTT_STATE_UNINIT) {
        return ESP_OK;
    }

    mqtt_set_state(MQTT_STATE_STOPPING);
    esp_err_t ret = esp_mqtt_client_stop(mqtt_client);

    if (ret == ESP_OK) {
        mqtt_set_state(MQTT_STATE_STOPPED);
        ESP_LOGI(TAG, "MQTT 已停止");
    } else {
        ESP_LOGE(TAG, "MQTT 停止失败: %d", ret);
    }
    return ret;
}

// 销毁
esp_err_t mqtt_manager_destroy(void)
{
    if (mqtt_client == NULL) {
        mqtt_set_state(MQTT_STATE_UNINIT);
        return ESP_OK;
    }

    if (mqtt_state == MQTT_STATE_RUNNING || mqtt_state == MQTT_STATE_STARTING) {
        esp_mqtt_client_stop(mqtt_client);
    }

    esp_mqtt_client_destroy(mqtt_client);
    mqtt_client = NULL;
    mqtt_set_state(MQTT_STATE_UNINIT);
    ESP_LOGI(TAG, "MQTT 已销毁");
    return ESP_OK;
}

// 状态查询 
mqtt_state_t mqtt_manager_get_state(void)
{
    return mqtt_state;
}

const char* mqtt_manager_get_state_string(void)
{
    return state_to_string(mqtt_state);
}

bool mqtt_manager_is_running(void)
{
    return mqtt_state == MQTT_STATE_RUNNING;
}

// WiFi 状态驱动
void mqtt_manager_on_wifi_connected(void)
{
    ESP_LOGI(TAG, "WiFi 已连接，当前 MQTT 状态: %s", state_to_string(mqtt_state));

    if (mqtt_state == MQTT_STATE_INIT || mqtt_state == MQTT_STATE_STOPPED) {
        mqtt_manager_start();
    } else if (mqtt_state == MQTT_STATE_ERROR) {
        ESP_LOGW(TAG, "MQTT 错误状态，重新初始化");
        mqtt_manager_destroy();
        mqtt_manager_init();
        mqtt_manager_start();
    } else {
        ESP_LOGI(TAG, "MQTT 已运行，无需操作");
    }
}

void mqtt_manager_on_wifi_disconnected(void)
{
    if (mqtt_state == MQTT_STATE_RUNNING || mqtt_state == MQTT_STATE_STARTING) {
        ESP_LOGW(TAG, "WiFi 断开，停止 MQTT");
        mqtt_manager_stop();
    } else {
        ESP_LOGI(TAG, "MQTT 未运行，无需停止");
    }
}

// 发布消息 
esp_err_t mqtt_manager_publish(const char *topic, const char *data, int len, int qos, bool retain)
{
    if (mqtt_client == NULL) {
        ESP_LOGW(TAG, "MQTT 客户端为空");
        return ESP_FAIL;
    }

    if (mqtt_state != MQTT_STATE_RUNNING) {
        ESP_LOGW(TAG, "MQTT 未运行 (状态: %s)", state_to_string(mqtt_state));
        return ESP_FAIL;
    }

    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, data, len, qos, retain);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "发布失败: %d", msg_id);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "发布 id=%d", msg_id);
    return ESP_OK;
}

// 回调注册 
void mqtt_manager_register_callback(mqtt_rx_callback_t callback)
{
    rx_callback = callback;
}

void mqtt_manager_register_status_callback(mqtt_status_callback_t callback)
{
    status_callback = callback;
}