#ifndef __SENSOR_MQTT_BRIDGE_H__
#define __SENSOR_MQTT_BRIDGE_H__

#include "esp_err.h"
#include "sen_protocol.h"
#include <stdbool.h>
#include <stdint.h>
#include "sensor_manager.h"

#define SENSOR_CACHE_SIZE 64u
#define SENSOR_ITEM_SIZE 7u // sensor_id(1) + timestamp(4) + count(2)

typedef void (*sensor_data_callback_t)(const sensor_data_t *data, void *user_ctx);

esp_err_t sensor_mqtt_bridge_init(void);
void sensor_manager_poll(uint32_t now_ms);
uint32_t sensor_manager_get_cache_count(void);
void sensor_manager_set_data_callback(sensor_data_callback_t callback, void *user_ctx);

#endif