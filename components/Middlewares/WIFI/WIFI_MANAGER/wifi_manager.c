#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "wifi_manager.h"
#include "wifi_mode.h"
#include "wifi_config.h"
#include "http_server.h"
#include "mqtt_manager.h"
#include "mqtt_service.h"
#include "esp_timer.h"
#include "esp_random.h"

static const char *TAG = "wifi_manager";
bool mqtt_ready_for_mode_switch = false;
static bool wifi_init_flag = false;
static bool wifi_connected = false;
static bool wifi_factory_reset_flag = false;
static char g_connecting_ssid[32] = {0};
static char g_connecting_password[64] = {0};
static char g_ip_str[16] = {0};
static uint8_t retry_count = 0;
static uint32_t current_retry_delay_ms = MIN_RETRY_DELAY_MS; // 当前重试延时，初始为最小值
static esp_timer_handle_t reconnect_timer = NULL;
static wifi_manager_state_t wifi_state = WIFI_MANAGER_IDLE;
static TaskHandle_t delayed_switch_task_handle = NULL;

// 函数声明
static void wifi_manager_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void wifi_manager_handle_disconnect(wifi_event_sta_disconnected_t *reason);
static void wifi_manager_handle_connected(void);
static void delayed_switch_to_sta_task(void *arg);
static void wifi_reconnect_timer_cb(void *arg);
static uint32_t wifi_reconnect_calc_delay(void);

// 计算下次重连延时
static uint32_t wifi_reconnect_calc_delay(void)
{
    uint32_t delay = current_retry_delay_ms;

    // 指数增长: 1s → 2s → 4s → 8s → 16s → 32s → 60s
    current_retry_delay_ms = current_retry_delay_ms * RETRY_DELAY_MULTIPLIER;
    if (current_retry_delay_ms > MAX_RETRY_DELAY_MS)
    {
        current_retry_delay_ms = MAX_RETRY_DELAY_MS;
    }

    // 添加随机抖动 (±20%)，避免多设备同时重连
    if (delay > 1000)
    {
        uint32_t jitter_range = delay / 5; // 20%
        uint32_t jitter = (esp_random() % jitter_range) - (jitter_range / 2);
        delay += jitter;
    }

    return delay;
}

// 重置退避延时
static void wifi_reconnect_reset(void)
{
    current_retry_delay_ms = MIN_RETRY_DELAY_MS;
    ESP_LOGD(TAG, "retry delay reset to %dms", current_retry_delay_ms);
}

/*
    调度重连
 */
static void wifi_reconnect_schedule(void)
{
    if (retry_count >= WIFI_MAX_RETRY)
    {
        ESP_LOGW(TAG, "max retry reached, enter AP config mode");
        wifi_factory_reset_flag = false;
        retry_count = 0;
        wifi_reconnect_reset();
        wifi_state = WIFI_MANAGER_AP_CONFIG;
        wifi_mode_switch_apsta();
        wifi_mode_config_start();
        return;
    }

    wifi_state = WIFI_MANAGER_RECONNECTING;
    uint32_t delay_ms = wifi_reconnect_calc_delay();

    ESP_LOGI(TAG, "schedule reconnect in %dms (retry %d/%d)", delay_ms, retry_count + 1, WIFI_MAX_RETRY);
    // 创建定时器
    if (reconnect_timer == NULL)
    {
        esp_timer_create_args_t timer_args = {
            .callback = wifi_reconnect_timer_cb,
            .name = "wifi_reconnect"};
        esp_timer_create(&timer_args, &reconnect_timer);
    }

    // 启动定时器
    esp_timer_start_once(reconnect_timer, delay_ms * 1000);
}

// 定时器回调
static void wifi_reconnect_timer_cb(void *arg)
{
    if (wifi_connected)
    {
        ESP_LOGD(TAG, "already connected, skip reconnect");
        return;
    }

    if (wifi_factory_reset_flag)
    {
        ESP_LOGD(TAG, "factory reset in progress, skip reconnect");
        return;
    }

    if (wifi_state != WIFI_MANAGER_RECONNECTING)
    {
        ESP_LOGD(TAG, "not in reconnecting state, skip");
        return;
    }

    ESP_LOGI(TAG, "reconnect attempt (retry %d/%d)", retry_count + 1, WIFI_MAX_RETRY);
    retry_count++;
    esp_wifi_connect();
}
// WIFI事件处理
static void wifi_manager_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "STA START");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
            wifi_manager_handle_disconnect(event);
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "PHONE CONNECT AP");
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "PHONE DISCONNECT AP");
            break;
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "GOT IP:" IPSTR, IP2STR(&event->ip_info.ip));
            snprintf(g_ip_str, sizeof(g_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
            wifi_manager_handle_connected();
        }
    }
}

/*
    连接成功处理
*/
static void wifi_manager_handle_connected(void)
{
    ESP_LOGI(TAG, "wifi connected");
    wifi_factory_reset_flag = false;
    wifi_connected = true;
    retry_count = 0;
    wifi_state = WIFI_MANAGER_CONNECTED;
    wifi_reconnect_reset();
    // 清除定时器
    if (reconnect_timer != NULL)
    {
        esp_timer_stop(reconnect_timer);
    }
    // 保存WiFi配置
    if (strlen(g_connecting_ssid) > 0)
    {
        wifi_config_save(g_connecting_ssid, g_connecting_password);
        ESP_LOGI(TAG, "save wifi:%s", g_connecting_ssid);
    }
    //  清除临时数据
    memset(g_connecting_ssid, 0, sizeof(g_connecting_ssid));
    memset(g_connecting_password, 0, sizeof(g_connecting_password));
    mqtt_ready_for_mode_switch = false;
    //  MQTT启动
    mqtt_service_init();
    mqtt_manager_on_wifi_connected();
    // 创建连接成功延时4s
    xTaskCreate(delayed_switch_to_sta_task, "delay_sta", 2048, NULL, 5, &delayed_switch_task_handle);
}

// 断开处理
static void wifi_manager_handle_disconnect(wifi_event_sta_disconnected_t *reason)
{
    if (wifi_factory_reset_flag)
    {
        ESP_LOGW(TAG, "factory reset ignore reconnect");
        return;
    }

    wifi_connected = false;
    mqtt_manager_on_wifi_disconnected();
    ESP_LOGW(TAG, "disconnect reason=%d", reason->reason);

    // 密码错误 → 立即进入配网模式
    if (reason->reason == WIFI_REASON_AUTH_FAIL || reason->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT)
    {
        wifi_factory_reset_flag = false;
        wifi_reconnect_reset();
        mqtt_ready_for_mode_switch = false;
        ESP_LOGE(TAG, "password error, enter AP config mode");
        memset(g_connecting_ssid, 0, sizeof(g_connecting_ssid));
        memset(g_connecting_password, 0, sizeof(g_connecting_password));
        wifi_state = WIFI_MANAGER_AP_CONFIG;
        wifi_mode_switch_apsta();
        wifi_mode_config_start();
        return;
    }

    wifi_reconnect_schedule();
}

// 迟切换 STA 任务，给前端留 4 秒时间
static void delayed_switch_to_sta_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(4000));
    int wait_count = 0;
    while (!mqtt_ready_for_mode_switch && wait_count < 20)
    {
        ESP_LOGI(TAG, "waiting for MQTT ready... (%d/20)", wait_count + 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        wait_count++;
    }
    if (mqtt_ready_for_mode_switch)
    {
        ESP_LOGI(TAG, "MQTT ready, now switch to STA mode");
    }
    else
    {
        ESP_LOGW(TAG, "MQTT not ready after 10s, force switch to STA mode");
    }
    wifi_mode_switch_sta();

    delayed_switch_task_handle = NULL;
    vTaskDelete(NULL);
}
// WiFi 初始化
esp_err_t wifi_manager_init(void)
{
    if (wifi_init_flag)
    {
        return ESP_OK;
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        wifi_manager_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
        wifi_manager_event_handler, NULL, NULL));

    // esp_wifi_set_mode(WIFI_MODE_APSTA);
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_init_flag = true;
    ESP_LOGI(TAG, "wifi manager init finish");
    return ESP_OK;
}

// 启动 WiFi 管理
void wifi_manager_start(void)
{
    char ssid[32] = {0};
    char password[64] = {0};
    if (wifi_config_load(ssid, password) == ESP_OK)
    {
        ESP_LOGI(TAG, "found wifi: %s", ssid);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        wifi_state = WIFI_MANAGER_CONNECTING;
        // 使用 snprintf 安全复制
        snprintf(g_connecting_ssid, sizeof(g_connecting_ssid), "%s", ssid);
        snprintf(g_connecting_password, sizeof(g_connecting_password), "%s", password);
        wifi_mode_sta_connect(ssid, password);
    }
    else
    {
        ESP_LOGI(TAG, "need config");
        esp_wifi_set_mode(WIFI_MODE_AP);
        ESP_ERROR_CHECK(wifi_mode_config_start());
        wifi_state = WIFI_MANAGER_AP_CONFIG;
    }
    http_server_start();
}

// HTTP 配网调用
void wifi_manager_set_wifi(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "收到 WiFi 配置: SSID=%s", ssid);

    // 使用 snprintf 安全复制
    snprintf(g_connecting_ssid, sizeof(g_connecting_ssid), "%s", ssid);
    snprintf(g_connecting_password, sizeof(g_connecting_password), "%s", password);

    retry_count = 0;
    wifi_reconnect_reset();
    wifi_state = WIFI_MANAGER_CONNECTING;

    esp_err_t ret = wifi_mode_sta_connect(ssid, password);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "WiFi 连接启动失败");
        wifi_state = WIFI_MANAGER_AP_CONFIG;
        g_connecting_ssid[0] = '\0';
        g_connecting_password[0] = '\0';
    }
}

// 查询状态
bool wifi_manager_is_connected(void)
{
    return wifi_connected;
}

bool wifi_manager_need_config(void)
{
    char ssid[32] = {0};
    char password[64] = {0};
    return (wifi_config_load(ssid, password) != ESP_OK);
}

void wifi_manager_clear_config(void)
{
    wifi_config_clear();
    wifi_connected = false;
    wifi_state = WIFI_MANAGER_IDLE;
    retry_count = 0;
    wifi_reconnect_reset();
    g_connecting_ssid[0] = '\0';
    g_connecting_password[0] = '\0';
}

void wifi_manager_factory_reset(void)
{
    ESP_LOGW(TAG, "factory reset");
    // 清理延时切换任务
    if (delayed_switch_task_handle != NULL)
    {
        vTaskDelete(delayed_switch_task_handle);
        delayed_switch_task_handle = NULL;
    }
    // 重置 MQTT 就绪标志
    mqtt_ready_for_mode_switch = false;
    // 停止 MQTT
    mqtt_manager_stop();
    // 禁止自动重连
    wifi_factory_reset_flag = true;
    // 停止wifi
    esp_wifi_disconnect();
    // 停止定时器
    if (reconnect_timer != NULL)
    {
        esp_timer_stop(reconnect_timer);
    }
    // 删除NVS
    wifi_config_clear();
    // 清除当前STA配置
    wifi_config_t empty_config = {0};
    esp_wifi_set_config(WIFI_IF_STA, &empty_config);
    wifi_connected = false;
    retry_count = 0;
    // 开启配网模式
    wifi_state = WIFI_MANAGER_AP_CONFIG;
    ESP_ERROR_CHECK(wifi_mode_switch_apsta());
    wifi_mode_config_start();
    vTaskDelay(pdMS_TO_TICKS(500));
    wifi_factory_reset_flag = false;
    ESP_LOGI(TAG, "enter wifi config mode");
}
// 状态查询接口
wifi_manager_state_t wifi_manager_get_state(void)
{
    return wifi_state;
}

const char *wifi_manager_get_ip_str(void)
{
    return (g_ip_str[0] != '\0') ? g_ip_str : NULL;
}

void wifi_manager_set_mqtt_ready(bool ready)
{
    mqtt_ready_for_mode_switch = ready;
    ESP_LOGD(TAG, "mqtt_ready_for_mode_switch = %d", ready);
}
bool wifi_manager_get_mqtt_ready(void)
{
    return mqtt_ready_for_mode_switch;
}