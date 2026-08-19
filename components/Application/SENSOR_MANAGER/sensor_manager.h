#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "esp_err.h"
#include "sen_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#define SENSOR_CACHE_SIZE 64u
#define SENSOR_ITEM_SIZE 7u

typedef struct
{
    uint8_t sensor_id;
    uint32_t timestamp_ms;
    uint16_t count;
    uint32_t receive_time_ms;
} sensor_data_t;

typedef void (*sensor_data_callback_t)(const sensor_data_t *data, void *user_ctx);
typedef void (*sensor_batch_callback_t)(const uint8_t *data, uint8_t count, void *user_ctx);

esp_err_t sensor_manager_init(void);
void sensor_manager_poll(uint32_t now_ms);
uint32_t sensor_manager_get_cache_count(void);
void sensor_manager_set_data_callback(sensor_data_callback_t callback, void *user_ctx);

#endif