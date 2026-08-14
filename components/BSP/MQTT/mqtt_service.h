#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include <stdint.h>

// 初始化 MQTT 服务
void mqtt_service_init(void);

// 发布 MOS 状态
void mqtt_service_publish_state(void);

// 发布 MOS 事件
void mqtt_service_publish_mos_event(uint8_t channel, uint8_t state);

#endif