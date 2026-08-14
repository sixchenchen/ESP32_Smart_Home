#ifndef __WIFI_MANAGER_H__
#define __WIFI_MANAGER_H__

#include "esp_err.h"

#define WIFI_MAX_RETRY 3

// 当前状态
typedef enum
{
    WIFI_MANAGER_IDLE,

    WIFI_MANAGER_AP_CONFIG,

    WIFI_MANAGER_CONNECTING,

    WIFI_MANAGER_CONNECTED,

    WIFI_MANAGER_ERROR,

} wifi_manager_state_t;

/*
    初始化wifi系统
*/
esp_err_t wifi_manager_init(void);

/*
    启动wifi管理
*/
void wifi_manager_start(void);

/*
    获取当前状态
*/
bool wifi_manager_is_connected(void);

/*
    配网完成
*/
void wifi_manager_set_wifi(const char *ssid, const char *password);
/*
    清除wifi配置
*/
void wifi_manager_clear_config(void);
bool wifi_manager_need_config(void);
void wifi_manager_factory_reset(void);

#endif
