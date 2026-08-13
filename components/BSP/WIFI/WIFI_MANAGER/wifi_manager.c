#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "wifi_manager.h"
#include "wifi_mode.h"
#include "wifi_config.h"
#include "http_server.h"
#include "wifi_scan.h"
#include "mqtt_manager.h"
#include "mqtt_service.h"

static const char *TAG = "wifi_manager";

/*
    wifi初始化标志
*/
static bool wifi_init_flag = false;

/*
    wifi连接状态
*/
static bool wifi_connected = false;

/*
    当前状态
*/
typedef enum
{
    WIFI_MANAGER_IDLE,
    // 等待用户配置
    WIFI_MANAGER_AP_CONFIG,
    // 正在连接路由器
    WIFI_MANAGER_CONNECTING,
    // 已连接
    WIFI_MANAGER_CONNECTED,
    // 连接失败
    WIFI_MANAGER_FAILED,
} wifi_manager_state_t;
wifi_scan_result_t wifi_list[20];
uint16_t count;
static wifi_manager_state_t wifi_state = WIFI_MANAGER_IDLE;

/*
    重连次数
*/
static uint8_t retry_count = 0;

/*
    WIFI事件
*/
static void wifi_manager_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    /*
        WIFI事件
    */
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        /*
            STA启动
        */
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "STA START");
            break;
        /*
            连接失败
        */
        case WIFI_EVENT_STA_DISCONNECTED:
        {
            if (wifi_state == WIFI_MANAGER_CONNECTING) // 已经有WiFi账号，正在连接路由器
            {
                retry_count++;
                ESP_LOGW(TAG, "retry connect %d", retry_count);
                if (retry_count < WIFI_MAX_RETRY)
                {
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    esp_wifi_connect();
                }
                else
                {
                    ESP_LOGW(TAG, "connect failed");
                    retry_count = 0;
                    wifi_state = WIFI_MANAGER_AP_CONFIG;
                    wifi_mode_config_start();
                }
            }
            else if (wifi_state == WIFI_MANAGER_CONNECTED) // 已经连接路由器，正常工作的断开
            {
                wifi_connected = false;
                retry_count++;
                if (retry_count < WIFI_MAX_RETRY)
                {
                    esp_wifi_connect();
                }
                else
                {
                    retry_count = 0;
                    /*
                        重新进入配置模式
                    */
                    wifi_state = WIFI_MANAGER_AP_CONFIG;
                    wifi_mode_config_start();
                }
            }
        }
        break;
            /*
                AP连接
            */
        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "PHONE CONNECT AP");
            break;
            /*
                AP断开
            */
        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "PHONE DISCONNECT AP");
            break;
        default:
            break;
        }
    }
    /*
        IP事件
    */
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
        {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "GOT IP:" IPSTR, IP2STR(&event->ip_info.ip));
            /*
                真正联网
            */
            wifi_connected = true;
            retry_count = 0;
            wifi_state = WIFI_MANAGER_CONNECTED;
            /*
                连接成功
                关闭AP
            */
            wifi_mode_switch_sta();

            mqtt_service_init();
            mqtt_manager_start();
            break;
        }
        default:
            break;
        }
    }
}

/*
    初始化wifi
*/
esp_err_t wifi_manager_init(void)
{
    if (wifi_init_flag)
    {
        return ESP_OK;
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /*
        创建网络接口
    */
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    /*
        注册事件
    */
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            wifi_manager_event_handler,
            NULL,
            NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            wifi_manager_event_handler,
            NULL,
            NULL));
    /*
        Null mode模式：
    */
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_init_flag = true;
    ESP_LOGI(TAG, "wifi manager init finish");
    return ESP_OK;
}

/*
    启动wifi管理
*/
void wifi_manager_start(void)
{
    char ssid[32] = {0};
    char password[64] = {0};
    if (wifi_config_load(ssid, password) == ESP_OK)
    {
        ESP_LOGI(TAG, "found wifi");
        wifi_state = WIFI_MANAGER_CONNECTING;
        wifi_mode_sta_connect(ssid, password);
    }
    else
    {
        ESP_LOGI(TAG, "need config");
        wifi_state = WIFI_MANAGER_AP_CONFIG;
        wifi_mode_config_start();
    }
    http_server_start();
}

/*
    HTTP配网调用
*/
void wifi_manager_set_wifi(const char *ssid, const char *password)
{
    wifi_config_save(ssid, password);
    wifi_state = WIFI_MANAGER_CONNECTING;
    wifi_mode_sta_connect(ssid, password);
}

/*
    查询连接状态
*/
bool wifi_manager_is_connected(void)
{
    return wifi_connected;
}
/*
    是否需要配网
*/
bool wifi_manager_need_config(void)
{
    char ssid[32] = {0};
    char password[64] = {0};
    if (
        wifi_config_load(ssid, password) == ESP_OK)
    {
        return false;
    }
    else
    {
        return true;
    }
}

/*
    清除配置
*/

void wifi_manager_clear_config(void)
{
    wifi_config_clear();
    wifi_connected = false;
    wifi_state = WIFI_MANAGER_IDLE;
}

void wifi_manager_factory_reset(void)
{
    ESP_LOGW(TAG, "factory reset");
    /*
        1.停止STA
    */
    esp_wifi_disconnect();
    /*
        2.删除wifi配置
    */
    wifi_config_clear();
    /*
        3.状态清零
    */
    wifi_connected = false;
    retry_count = 0;
    wifi_state = WIFI_MANAGER_AP_CONFIG;
    /*
        4.进入配网模式
    */
    wifi_mode_config_start();
    ESP_LOGI(TAG, "enter config mode");
}
