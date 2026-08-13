#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#define MQTT_CONTROL_TOPIC "device/device001/control"
#define DEVICE_ID "device001"
#define TOPIC_ONLINE "device/device001/online"
#define TOPIC_HEART "device/device001/heartbeat"
#define TOPIC_STATE "device/device001/state"
#define TOPIC_CONTROL "device/device001/control"
#define TOPIC_REPLY "device/device001/reply"

void mqtt_service_init(void);

void mqtt_service_publish_state(void);

#endif