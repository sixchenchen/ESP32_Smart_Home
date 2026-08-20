#ifndef SENSOR_MQTT_BRIDGE_H
#define SENSOR_MQTT_BRIDGE_H

#include "esp_err.h"
#include "sensor_manager.h" // ✅ 必须包含，定义 sensor_data_t

#ifdef __cplusplus
extern "C"
{
#endif

// 批量缓存配置
#define BATCH_MAX_COUNT 32
#define BATCH_TIMEOUT_MS 100
#define ITEM_SIZE 7

    // ✅ batch_cache_t 结构体必须在头文件中定义
    typedef struct
    {
        sensor_data_t data[BATCH_MAX_COUNT];
        uint8_t count;
        uint32_t first_time_ms;
    } batch_cache_t;

    esp_err_t sensor_mqtt_bridge_init(void);
    void sensor_mqtt_bridge_poll(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif