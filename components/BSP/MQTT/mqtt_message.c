#include "mqtt_message.h"
#include "json_builder.h"
#include "device_context.h"
#include "mqtt_config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

char *mqtt_message_create_will(void)
{
    const device_context_t *dev = device_context_get();
    cJSON *root = json_create_object();
    json_add_string(root, JSON_DEVICE, dev->device_id);
    json_add_string(root, JSON_PRODUCT, dev->product_id);
    json_add_string(root, JSON_STATUS, STATUS_OFFLINE);
    return json_finish(root);
}

char *mqtt_message_create_status(bool online)
{
    const device_context_t *dev = device_context_get();
    cJSON *root = json_create_object();
    json_add_string(root, JSON_DEVICE, dev->device_id);
    json_add_string(root, JSON_STATUS, online ? STATUS_ONLINE : STATUS_OFFLINE);
    return json_finish(root);
}

char *mqtt_message_create_mos_event(uint8_t channel, uint8_t state)
{
    cJSON *root = json_create_object();
    json_add_string(root, JSON_TYPE, EVENT_MOS_CHANGE);
    json_add_number(root, JSON_CHANNEL, channel);
    json_add_number(root, JSON_STATE, state);
    return json_finish(root);
}

char *mqtt_message_create_heartbeat(uint32_t uptime)
{
    const device_context_t *dev = device_context_get();
    char *msg = malloc(128);
    if (msg == NULL)
    {
        return NULL;
    }
    snprintf(msg, 128,
             "{\"%s\":\"%s\",\"%s\":%lu,\"%s\":\"%s\"}",
             JSON_DEVICE, dev->device_id,
             JSON_UPTIME, uptime,
             JSON_STATUS, STATUS_ONLINE);
    return msg;
}

char *mqtt_message_create_error(uint16_t code, const char *msg)
{
    cJSON *root = json_create_object();
    json_add_string(root, JSON_TYPE, EVENT_TYPE_ERROR);
    json_add_number(root, JSON_CODE, code);
    json_add_string(root, JSON_MSG, msg);
    return json_finish(root);
}