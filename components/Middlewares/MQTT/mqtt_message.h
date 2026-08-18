#ifndef MQTT_MESSAGE_H
#define MQTT_MESSAGE_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

char *mqtt_message_create_will(void);

char *mqtt_message_create_status(bool online);

char *mqtt_message_create_heartbeat(uint32_t uptime);

char *mqtt_message_create_mos_event(uint8_t channel, uint8_t state);

char *mqtt_message_create_error(uint16_t code, const char *msg);

#endif