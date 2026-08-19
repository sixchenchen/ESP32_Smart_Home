#ifndef MQTT_MESSAGE_H
#define MQTT_MESSAGE_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

char *mqtt_message_create_will(void);

char *mqtt_message_create_state(const char *state, const char *reason);

char *mqtt_message_create_online(void);

char *mqtt_message_create_offline(const char *reason);

char *mqtt_message_create_mos_state(uint8_t mos_state);

char *mqtt_message_create_factory_reset(void);

char *mqtt_message_create_heartbeat(uint32_t uptime);

char *mqtt_message_create_mos_event(uint8_t channel, uint8_t state);

char *mqtt_message_create_error(uint16_t code, const char *msg);

char *mqtt_message_create_sensor_data(uint8_t sensor_id, uint32_t timestamp_ms, uint16_t count);

char *mqtt_message_create_sensor_batch(const uint8_t *data, uint8_t count);

#endif