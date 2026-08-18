#ifndef __KEY_H__
#define __KEY_H__

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/*
    按键事件
*/
typedef enum
{

    KEY_EVENT_NONE = 0,

    KEY_EVENT_SHORT_PRESS,

    KEY_EVENT_LONG_PRESS,

} key_event_t;

/*
    初始化KEY
*/
esp_err_t key_init(void);

/*
    获取KEY事件队列
*/
QueueHandle_t key_get_queue(void);

#endif