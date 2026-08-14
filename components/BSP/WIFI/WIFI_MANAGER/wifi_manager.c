#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

static const char *TAG = "wifi_manager";

static bool wifi_init_flag = false;
static bool wifi_connected = false;
static bool wifi_factory_reset_flag = false;
static bool mqtt_started = false;
static wifi_manager_state_t wifi_state = WIFI_MANAGER_IDLE;
uint16_t count;
static char g_connecting_ssid[32] = {0};
static char g_connecting_password[64] = {0};
static uint8_t retry_count = 0;

// 安全复制字符串
#define SAFE_STRNCPY(dst, src, size)    \
    do                                  \
    {                                   \
        snprintf(dst, size, "%s", src); \
    } while (0)

// 函数声明
static void wifi_manager_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void wifi_manager_handle_disconnect(wifi_event_sta_disconnected_t *reason);
static void wifi_manager_handle_connected(void);

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
            wifi_manager_handle_connected();
        }
    }
}

// 连接成功处理
static void wifi_manager_handle_connected(void)
{

    ESP_LOGI(TAG,
             "wifi connected");
    wifi_factory_reset_flag = false;
    wifi_connected = true;

    retry_count = 0;

    wifi_state =
        WIFI_MANAGER_CONNECTED;

    /*
        保存WiFi配置
    */

    if (strlen(g_connecting_ssid) > 0)
    {

        wifi_config_save(
            g_connecting_ssid,
            g_connecting_password);

        ESP_LOGI(
            TAG,
            "save wifi:%s",
            g_connecting_ssid);
    }

    /*
        清除临时数据
    */

    memset(
        g_connecting_ssid,
        0,
        sizeof(g_connecting_ssid));

    memset(
        g_connecting_password,
        0,
        sizeof(g_connecting_password));

    /*
        APSTA -> STA
    */

    wifi_mode_switch_sta();

    /*
        MQTT启动
    */

    if (!mqtt_started)
    {
        mqtt_service_init();

        mqtt_manager_start();

        mqtt_started = true;
    }
}

// 断开处理
static void wifi_manager_handle_disconnect(
    wifi_event_sta_disconnected_t *reason)
{

    if (wifi_factory_reset_flag)
    {
        ESP_LOGW(TAG, "factory reset ignore reconnect");
        return;
    }
    wifi_connected = false;

    ESP_LOGW(TAG, "disconnect reason=%d", reason->reason);

    /*
        密码错误
    */

    if (reason->reason == WIFI_REASON_AUTH_FAIL || reason->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT)
    {

        ESP_LOGE(
            TAG,
            "password error");

        /*
            清除错误缓存
        */

        memset(
            g_connecting_ssid,
            0,
            sizeof(g_connecting_ssid));

        memset(
            g_connecting_password,
            0,
            sizeof(g_connecting_password));

        wifi_state =
            WIFI_MANAGER_AP_CONFIG;

        wifi_mode_switch_apsta();

        wifi_mode_config_start();

        return;
    }

    retry_count++;

    /*
        自动重连
    */

    if (retry_count < WIFI_MAX_RETRY)
    {

        ESP_LOGI(
            TAG,
            "retry wifi %d/%d",
            retry_count,
            WIFI_MAX_RETRY);

        vTaskDelay(
            pdMS_TO_TICKS(3000));

        esp_wifi_connect();
    }

    else
    {

        ESP_LOGW(
            TAG,
            "retry failed");

        retry_count = 0;

        wifi_state =
            WIFI_MANAGER_AP_CONFIG;

        wifi_mode_switch_apsta();

        wifi_mode_config_start();
    }
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

    esp_wifi_set_mode(WIFI_MODE_APSTA);
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
        wifi_state = WIFI_MANAGER_CONNECTING;

        // 使用 snprintf 安全复制
        snprintf(g_connecting_ssid, sizeof(g_connecting_ssid), "%s", ssid);
        snprintf(g_connecting_password, sizeof(g_connecting_password), "%s", password);

        wifi_mode_sta_connect(ssid, password, NULL);
    }
    else
    {
        ESP_LOGI(TAG, "need config");
        wifi_state = WIFI_MANAGER_AP_CONFIG;
        wifi_mode_config_start();
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
    wifi_state = WIFI_MANAGER_CONNECTING;

    esp_err_t ret = wifi_mode_sta_connect(ssid, password, NULL);
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
    g_connecting_ssid[0] = '\0';
    g_connecting_password[0] = '\0';
}

void wifi_manager_factory_reset(void)
{

    ESP_LOGW(TAG, "factory reset");
    // 禁止自动重连
    wifi_factory_reset_flag = true;
    // 停止wifi
    esp_wifi_disconnect();
    // 删除NVS
    wifi_config_clear();
    // 清除当前STA配置
    wifi_config_t empty_config = {0};
    esp_wifi_set_config(WIFI_IF_STA, &empty_config);
    wifi_connected = false;
    retry_count = 0;
    // 开启配网模式
    wifi_state = WIFI_MANAGER_AP_CONFIG;
    wifi_mode_switch_apsta();
    wifi_mode_config_start();
    ESP_LOGI(TAG, "enter wifi config mode");
}