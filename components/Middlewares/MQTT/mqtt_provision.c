// mqtt_provision.c
#include "mqtt_provision.h"
#include "mqtt_manager.h"
#include "mqtt_message.h"
#include "mqtt_topic.h"
#include "mqtt_message.h"
#include "device_context.h"
#include "mqtt_topic.h"
#include "mqtt_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "mqtt_service.h"

static const char *TAG = "provision";

// ==================== 全局状态 ====================
static provision_state_t g_prov_state = PROV_STATE_IDLE;
static int g_retry_count = 0;
static uint32_t g_wait_start_ms = 0;
static bool g_config_received = false;
static mqtt_config_t g_received_config;
static TaskHandle_t g_prov_task_handle = NULL;

// ==================== 状态转字符串 ====================
const char *mqtt_provision_get_state_string(void)
{
    switch (g_prov_state)
    {
    case PROV_STATE_IDLE:
        return "IDLE";
    case PROV_STATE_CONNECTING:
        return "CONNECTING";
    case PROV_STATE_SENDING:
        return "SENDING";
    case PROV_STATE_WAITING:
        return "WAITING";
    case PROV_STATE_SUCCESS:
        return "SUCCESS";
    case PROV_STATE_FAILED:
        return "FAILED";
    case PROV_STATE_TIMEOUT:
        return "TIMEOUT";
    default:
        return "UNKNOWN";
    }
}

provision_state_t mqtt_provision_get_state(void)
{
    return g_prov_state;
}

// ==================== 状态变更 ====================
static void prov_set_state(provision_state_t new_state)
{
    if (g_prov_state != new_state)
    {
        ESP_LOGI(TAG, "State: %s -> %s",
                 mqtt_provision_get_state_string(),
                 new_state == PROV_STATE_IDLE ? "IDLE" : new_state == PROV_STATE_CONNECTING ? "CONNECTING"
                                                     : new_state == PROV_STATE_SENDING      ? "SENDING"
                                                     : new_state == PROV_STATE_WAITING      ? "WAITING"
                                                     : new_state == PROV_STATE_SUCCESS      ? "SUCCESS"
                                                     : new_state == PROV_STATE_FAILED       ? "FAILED"
                                                     : new_state == PROV_STATE_TIMEOUT      ? "TIMEOUT"
                                                                                            : "UNKNOWN");
        g_prov_state = new_state;
    }
}

// ==================== 发送注册请求 ====================
static esp_err_t send_register_request(void)
{
    char *request = mqtt_message_create_register_request();
    if (request == NULL)
    {
        ESP_LOGE(TAG, "Failed to build register request");
        return ESP_FAIL;
    }

    const char *topic = mqtt_topic_provision_register();

    ESP_LOGI(TAG, "Sending register request to: %s", topic);
    ESP_LOGD(TAG, "Request: %s", request);

    esp_err_t ret = mqtt_manager_publish(topic, request, strlen(request), 1, false);
    free(request);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to publish register request");
        return ret;
    }

    return ESP_OK;
}

// ==================== 处理注册响应 ====================
void mqtt_provision_handle_response(const uint8_t *data, int len)
{
    if (g_prov_state != PROV_STATE_WAITING)
    {
        ESP_LOGW(TAG, "Not in WAITING state, ignoring response");
        return;
    }

    if (data == NULL || len <= 0)
    {
        ESP_LOGE(TAG, "Invalid response data");
        return;
    }

    // 解析 JSON
    char json[512];
    if (len >= (int)sizeof(json))
    {
        ESP_LOGE(TAG, "Response too large: %d", len);
        prov_set_state(PROV_STATE_FAILED);
        return;
    }
    memcpy(json, data, len);
    json[len] = '\0';

    ESP_LOGI(TAG, "Received config response: %s", json);

    cJSON *root = cJSON_Parse(json);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "Failed to parse JSON");
        prov_set_state(PROV_STATE_FAILED);
        return;
    }

    // 检查状态
    cJSON *status = cJSON_GetObjectItem(root, "status");
    if (!status || !cJSON_IsString(status))
    {
        ESP_LOGE(TAG, "Missing status field");
        cJSON_Delete(root);
        prov_set_state(PROV_STATE_FAILED);
        return;
    }

    if (strcmp(status->valuestring, "success") != 0)
    {
        cJSON *msg = cJSON_GetObjectItem(root, "message");
        ESP_LOGE(TAG, "Registration failed: %s", msg ? msg->valuestring : "unknown");
        cJSON_Delete(root);
        prov_set_state(PROV_STATE_FAILED);
        return;
    }

    // 提取配置
    cJSON *config = cJSON_GetObjectItem(root, "config");
    if (!config || !cJSON_IsObject(config))
    {
        ESP_LOGE(TAG, "Missing config object");
        cJSON_Delete(root);
        prov_set_state(PROV_STATE_FAILED);
        return;
    }

    memset(&g_received_config, 0, sizeof(mqtt_config_t));
    g_received_config.is_provisioned = true;

    cJSON *item;

    item = cJSON_GetObjectItem(config, "broker_uri");
    if (item && cJSON_IsString(item))
    {
        strlcpy(g_received_config.broker_uri, item->valuestring, sizeof(g_received_config.broker_uri));
    }

    item = cJSON_GetObjectItem(config, "client_id");
    if (item && cJSON_IsString(item))
    {
        strlcpy(g_received_config.client_id, item->valuestring, sizeof(g_received_config.client_id));
    }
    else
    {
        // 如果服务器没下发，使用设备ID
        const device_context_t *dev = device_context_get();
        strlcpy(g_received_config.client_id, dev->device_id, sizeof(g_received_config.client_id));
    }

    item = cJSON_GetObjectItem(config, "username");
    if (item && cJSON_IsString(item))
    {
        strlcpy(g_received_config.username, item->valuestring, sizeof(g_received_config.username));
    }

    item = cJSON_GetObjectItem(config, "password");
    if (item && cJSON_IsString(item))
    {
        strlcpy(g_received_config.password, item->valuestring, sizeof(g_received_config.password));
    }

    item = cJSON_GetObjectItem(config, "will_topic");
    if (item && cJSON_IsString(item))
    {
        strlcpy(g_received_config.will_topic, item->valuestring, sizeof(g_received_config.will_topic));
    }
    else
    {
        strlcpy(g_received_config.will_topic, mqtt_topic_will(), sizeof(g_received_config.will_topic));
    }

    cJSON_Delete(root);

    g_config_received = true;
    prov_set_state(PROV_STATE_SUCCESS);

    ESP_LOGI(TAG, "Registration successful!");
}

// ==================== 注册任务 ====================
static void mqtt_provision_task(void *arg)
{
    ESP_LOGI(TAG, "Provision task started");

    // 等待 MQTT 连接建立
    prov_set_state(PROV_STATE_CONNECTING);

    int wait_count = 0;
    while (!mqtt_manager_is_running() && wait_count < 100)
    {
        // 最多等 10 秒
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_count++;
    }

    if (!mqtt_manager_is_running())
    {
        ESP_LOGE(TAG, "MQTT not connected, provision failed");
        prov_set_state(PROV_STATE_FAILED);
        g_prov_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "MQTT connected, sending register request");

    // 循环：发送请求 + 等待响应
    g_retry_count = 0;
    g_config_received = false;

    while (g_retry_count < PROV_MAX_RETRY)
    {
        // 发送注册请求
        prov_set_state(PROV_STATE_SENDING);

        esp_err_t ret = send_register_request();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to send register request");
            g_retry_count++;
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        // 等待响应
        prov_set_state(PROV_STATE_WAITING);
        g_wait_start_ms = esp_timer_get_time() / 1000;

        // 非阻塞等待
        while (1)
        {
            uint32_t now = esp_timer_get_time() / 1000;

            // 收到配置
            if (g_config_received)
            {
                ESP_LOGI(TAG, "Config received, saving to NVS...");

                esp_err_t save_ret = mqtt_config_save(&g_received_config);
                if (save_ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to save config: %d", save_ret);
                    prov_set_state(PROV_STATE_FAILED);
                }
                else
                {
                    ESP_LOGI(TAG, "Provision complete! Reconnecting to production broker...");
                    mqtt_manager_stop();
                    mqtt_manager_destroy();
                    mqtt_service_init();              
                    mqtt_manager_on_wifi_connected(); 

                    prov_set_state(PROV_STATE_SUCCESS);
                }
                goto prov_done;
            }

            // 超时检查
            if (now - g_wait_start_ms > PROV_TIMEOUT_MS)
            {
                ESP_LOGW(TAG, "Wait response timeout");
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // 超时，重试
        g_retry_count++;
        ESP_LOGW(TAG, "Retry %d/%d", g_retry_count, PROV_MAX_RETRY);

        if (g_retry_count >= PROV_MAX_RETRY)
        {
            ESP_LOGE(TAG, "Provision failed after %d retries", PROV_MAX_RETRY);
            prov_set_state(PROV_STATE_TIMEOUT);
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

prov_done:
    g_prov_task_handle = NULL;
    vTaskDelete(NULL);
}

// ==================== 启动注册 ====================
esp_err_t mqtt_provision_start(void)
{
    // 检查任务是否已运行
    if (g_prov_task_handle != NULL)
    {
        ESP_LOGW(TAG, "Provision task already running");
        return ESP_OK;
    }

    // 重置状态
    g_retry_count = 0;
    g_config_received = false;
    memset(&g_received_config, 0, sizeof(g_received_config));

    // 创建注册任务
    BaseType_t ret = xTaskCreate(
        mqtt_provision_task,
        "provision",
        PROV_TASK_STACK_SIZE,
        NULL,
        PROV_TASK_PRIORITY,
        &g_prov_task_handle);

    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create provision task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Provision started");
    return ESP_OK;
}

// ==================== 检查是否完成 ====================
bool mqtt_provision_is_done(void)
{
    return g_prov_state == PROV_STATE_SUCCESS;
}

// ==================== 重置 ====================
void mqtt_provision_reset(void)
{
    // 停止任务（如果有）
    if (g_prov_task_handle != NULL)
    {
        vTaskDelete(g_prov_task_handle);
        g_prov_task_handle = NULL;
    }

    g_prov_state = PROV_STATE_IDLE;
    g_retry_count = 0;
    g_config_received = false;
    memset(&g_received_config, 0, sizeof(g_received_config));

    ESP_LOGI(TAG, "Provision reset");
}

// ==================== 初始化 ====================
void mqtt_provision_init(void)
{
    ESP_LOGI(TAG, "Provision module initialized");
    g_prov_state = PROV_STATE_IDLE;
}