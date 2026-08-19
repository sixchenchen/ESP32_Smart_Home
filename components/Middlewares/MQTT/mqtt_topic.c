#include "mqtt_topic.h"

#include "device_context.h"

#include <stdio.h>

static char control[80];
static char status[80];
static char event[80];
static char state[80];
static char mos_state[80];
static char heart[80];
static char will[80];
static char sensor[80];

void mqtt_topic_init(void)
{
    const device_context_t *dev = device_context_get();
    snprintf(control, sizeof(control), "device/%s/control", dev->device_id);
    snprintf(status, sizeof(status), "device/%s/status", dev->device_id);
    snprintf(event, sizeof(event), "device/%s/event", dev->device_id);
    snprintf(mos_state, sizeof(mos_state), "device/%s/mos_state", dev->device_id);
    snprintf(heart, sizeof(heart), "device/%s/heart", dev->device_id);
    snprintf(will, sizeof(will), "device/%s/status", dev->device_id);
    snprintf(sensor, sizeof(sensor), "device/%s/sensor", dev->device_id);
}

const char *mqtt_topic_control(void)
{
    return control;
}

const char *mqtt_topic_status(void)
{
    return status;
}

const char *mqtt_topic_event(void)
{
    return event;
}

const char *mqtt_topic_state(void)
{
    return state;
}

const char *mqtt_topic_mos_state(void)
{
    return mos_state;
}

const char *mqtt_topic_heart(void)
{
    return heart;
}

const char *mqtt_topic_will(void)
{
    return will;
}

const char *mqtt_topic_sensor(void)
{
    return sensor;
}
