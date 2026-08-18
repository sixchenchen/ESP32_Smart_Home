#ifndef MQTT_TOPIC_H
#define MQTT_TOPIC_H

void mqtt_topic_init(void);

const char *mqtt_topic_control(void);

const char *mqtt_topic_status(void);

const char *mqtt_topic_event(void);

const char *mqtt_topic_state(void);

const char *mqtt_topic_heart(void);

const char *mqtt_topic_will(void);

#endif