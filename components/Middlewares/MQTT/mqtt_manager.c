#include "mqtt_manager.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include <string.h>
#include "mqtt_config.h"
#include "mqtt_topic.h"
#include "mqtt_message.h"
#include "device_context.h"
#include "wifi_manager.h"

static const char *TAG = "mqtt_manager";

/* ─────────────── 模块常量 ─────────────── */

#define MQTT_KEEPALIVE_SEC    15    /* MQTT keepalive 心跳间隔 (秒) */
#define MQTT_CONN_TIMEOUT_MS  5000  /* MQTT 连接超时 (ms) */
#define MQTT_PUBLISH_RETRY    3     /* publish 重试次数 */
#define MQTT_PUBLISH_DELAY_MS 20    /* publish 重试间隔 (ms) */

static esp_mqtt_client_handle_t mqtt_client = NULL;
static mqtt_rx_callback_t rx_callback = NULL;
static mqtt_status_callback_t status_callback = NULL;
static char *will_message = NULL;
static bool s_is_provisioned = false;

// MQTT 状态
static mqtt_state_t mqtt_state = MQTT_STATE_UNINIT;

//  状态转字符串
static const char *state_to_string(mqtt_state_t state)
{
    switch (state)
    {
    case MQTT_STATE_UNINIT:
        return "UNINIT";
    case MQTT_STATE_INIT:
        return "INIT";
    case MQTT_STATE_STARTING:
        return "STARTING";
    case MQTT_STATE_RUNNING:
        return "RUNNING";
    case MQTT_STATE_STOPPING:
        return "STOPPING";
    case MQTT_STATE_STOPPED:
        return "STOPPED";
    case MQTT_STATE_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

//  状态变更
static void mqtt_set_state(mqtt_state_t new_state)
{
    if (mqtt_state != new_state)
    {
        mqtt_state = new_state;
        ESP_LOGI(TAG, "状态变更: %s", state_to_string(new_state));
        if (status_callback)
        {
            status_callback(new_state);
        }
    }
}

//  MQTT事件处理
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch (event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, " MQTT 连接成功");
        wifi_manager_set_mqtt_ready(true);
        mqtt_set_state(MQTT_STATE_RUNNING);

        if (s_is_provisioned)
        {
            // ===== 正式运行阶段：订阅所有业务 topic =====
            mqtt_manager_subscribe(mqtt_topic_control(), 1);
            esp_err_t r = mqtt_manager_subscribe(mqtt_topic_ota(), 1);
            ESP_LOGI(TAG, "subscribe ota topic: %s → %s",
                     mqtt_topic_ota(), r == ESP_OK ? "OK" : "FAIL");
        }
        else
        {
            // ===== Provisioning 阶段：只订阅注册响应 =====
            ESP_LOGI(TAG, "Not provisioned, subscribe provision config only");
        }
        // provision config response 始终订阅（正式阶段也可能收到 config 更新）
        mqtt_manager_subscribe(mqtt_topic_provision_config(), 1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT 断开");
        // 状态由 WiFi 驱动改变
        break;

    case MQTT_EVENT_DATA:
    {
        char topic[128] = {0};
        if (event->topic_len < sizeof(topic))
        {
            memcpy(topic, event->topic, event->topic_len);
        }
        if (rx_callback)
        {
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
    if (mqtt_client != NULL)
    {
        return ESP_OK;
    }

    // 静默 ESP-IDF 原生 mqtt_client 的瞬时 ERROR 日志
    // （OTA/下载时抢 WiFi 会触发 "Publish message cannot be created"，
    //  这是瞬时资源紧张，重试能自动恢复，不值得打 ERROR）
    esp_log_level_set("mqtt_client", ESP_LOG_NONE);

    // 从 NVS 加载配置
    mqtt_config_t config;
    mqtt_config_load(&config);
    s_is_provisioned = config.is_provisioned;
    mqtt_topic_init();

    will_message = mqtt_message_create_will();
    if (will_message == NULL)
    {
        return ESP_FAIL;
    }

    esp_mqtt_client_config_t mqtt_cfg =
        {
            .broker.address.uri = config.broker_uri, // ← 动态
            .credentials =
                {
                    .client_id = config.client_id,              // ← 动态
                    .username = config.username,                // ← 动态
                    .authentication.password = config.password, // ← 动态
                },
            .session =
                {
                    .last_will =
                        {
                            .topic = config.will_topic, // ← 动态
                            .msg = will_message,
                            .msg_len = strlen(will_message),
                            .qos = WILL_QOS,
                            .retain = WILL_RETAIN,
                        },
                    .keepalive = MQTT_KEEPALIVE_SEC,
                },
            .network = {
                .timeout_ms = MQTT_CONN_TIMEOUT_MS,
                .disable_auto_reconnect = false,
            }

    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL)
    {
        free(will_message);
        will_message = NULL;
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(
        mqtt_client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL);

    mqtt_set_state(MQTT_STATE_INIT);
    return ESP_OK;
}

// 启动
esp_err_t mqtt_manager_start(void)
{
    if (mqtt_client == NULL)
    {
        ESP_LOGE(TAG, "MQTT 未初始化");
        return ESP_FAIL;
    }

    if (mqtt_state == MQTT_STATE_RUNNING || mqtt_state == MQTT_STATE_STARTING)
    {
        ESP_LOGW(TAG, "MQTT 已启动或正在启动");
        return ESP_OK;
    }

    mqtt_set_state(MQTT_STATE_STARTING);
    esp_err_t ret = esp_mqtt_client_start(mqtt_client);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "MQTT 启动成功，等待连接...");
    }
    else
    {
        ESP_LOGE(TAG, "MQTT 启动失败: %d", ret);
        mqtt_set_state(MQTT_STATE_ERROR);
    }
    return ret;
}

// 停止
esp_err_t mqtt_manager_stop(void)
{
    if (mqtt_client == NULL)
    {
        return ESP_OK;
    }

    if (mqtt_state == MQTT_STATE_STOPPED || mqtt_state == MQTT_STATE_UNINIT)
    {
        return ESP_OK;
    }

    mqtt_set_state(MQTT_STATE_STOPPING);
    esp_err_t ret = esp_mqtt_client_stop(mqtt_client);

    if (ret == ESP_OK)
    {
        mqtt_set_state(MQTT_STATE_STOPPED);
        ESP_LOGI(TAG, "MQTT 已停止");
    }
    else
    {
        ESP_LOGE(TAG, "MQTT 停止失败: %d", ret);
    }
    return ret;
}

// 销毁
esp_err_t mqtt_manager_destroy(void)
{
    if (mqtt_client)
    {
        if (mqtt_state != MQTT_STATE_STOPPED && mqtt_state != MQTT_STATE_UNINIT)
        {
            esp_mqtt_client_stop(mqtt_client);
        }
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
    }
    if (will_message)
    {
        free(will_message);
        will_message = NULL;
    }
    mqtt_set_state(MQTT_STATE_UNINIT);
    return ESP_OK;
}
// 状态查询
mqtt_state_t mqtt_manager_get_state(void)
{
    return mqtt_state;
}

const char *mqtt_manager_get_state_string(void)
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
    if (mqtt_state == MQTT_STATE_INIT || mqtt_state == MQTT_STATE_STOPPED)
    {
        mqtt_manager_start();
    }
    else if (mqtt_state == MQTT_STATE_ERROR)
    {
        ESP_LOGW(TAG, "MQTT 错误状态，重新初始化");
        mqtt_manager_destroy();
        mqtt_manager_init();
        mqtt_manager_start();
    }
    else
    {
        ESP_LOGI(TAG, "MQTT 已运行，无需操作");
    }
}

void mqtt_manager_on_wifi_disconnected(void)
{
    if (mqtt_state == MQTT_STATE_RUNNING || mqtt_state == MQTT_STATE_STARTING)
    {
        ESP_LOGW(TAG, "WiFi 断开，停止 MQTT");
        mqtt_manager_stop();
    }
    else
    {
        ESP_LOGI(TAG, "MQTT 未运行，无需停止");
    }
}

// 发布消息
esp_err_t mqtt_manager_publish(const char *topic, const char *data, int len, int qos, bool retain)
{
    if (topic == NULL || data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (mqtt_client == NULL)
    {
        ESP_LOGD(TAG, "MQTT 客户端为空");
        return ESP_FAIL;
    }
    if (mqtt_state != MQTT_STATE_RUNNING)
    {
        ESP_LOGD(TAG, "MQTT 未运行 (状态: %s)", state_to_string(mqtt_state));
        return ESP_FAIL;
    }

    // 带重试的 publish：瞬时资源紧张（如 OTA 启动抢 WiFi）时重试
    int msg_id = -1;
    for (int attempt = 0; attempt < MQTT_PUBLISH_RETRY; attempt++)
    {
        msg_id = esp_mqtt_client_publish(mqtt_client, topic, data, len, qos, retain);
        if (msg_id >= 0) break;
        if (attempt < MQTT_PUBLISH_RETRY - 1)
            vTaskDelay(pdMS_TO_TICKS(MQTT_PUBLISH_DELAY_MS));
    }

    if (msg_id < 0)
    {
        // 静默失败：publish 经常是瞬时资源紧张（OTA/下载时）造成的，
        // 不需要打 ERROR/WARN 级别的日志打扰用户
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "发布 id=%d topic=%s", msg_id, topic);
    return ESP_OK;
}

esp_err_t mqtt_manager_subscribe(const char *topic, int qos)
{
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "subscribe FAIL: client is NULL, topic=%s", topic);
        return ESP_FAIL;
    }
    int id = esp_mqtt_client_subscribe(mqtt_client, topic, qos);
    if (id >= 0) {
        ESP_LOGI(TAG, "subscribe OK: topic=%s qos=%d mid=%d", topic, qos, id);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "subscribe FAIL: topic=%s qos=%d ret=%d (see ESP_ERR_MQTT_*)", topic, qos, id);
        return ESP_FAIL;
    }
}

bool mqtt_manager_is_production(void)
{
    if (mqtt_state != MQTT_STATE_RUNNING)
    {
        return false;
    }
    return s_is_provisioned;
}

/*
    MQTT 恢复出厂设置
 */
void mqtt_manager_factory_reset(void)
{
    ESP_LOGW(TAG, "MQTT factory reset");

    // 1. 停止 MQTT
    mqtt_manager_stop();

    // 2. 销毁客户端
    mqtt_manager_destroy();

    // 3. 清除 NVS 里的 MQTT 配置
    mqtt_config_clear();

    ESP_LOGI(TAG, "MQTT config cleared");
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