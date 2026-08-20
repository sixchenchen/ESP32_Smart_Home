#ifndef __LED_H__
#define __LED_H__

#include "driver/gpio.h"

#define LED_GPIO_PIN GPIO_NUM_23

typedef enum
{
    PIN_RESET = 0,
    PIN_SET = 1,
} GPIO_OUTPUT_STATE;

#define LED_ON() gpio_set_level(LED_GPIO_PIN, PIN_RESET)
#define LED_OFF() gpio_set_level(LED_GPIO_PIN, PIN_SET)
#define LED_TOGGLE() led_toggle()

// LED 模式定义
typedef enum
{
    LED_MODE_OFF = 0,
    LED_MODE_ON,
    LED_MODE_SLOW_FLASH,
    LED_MODE_FAST_FLASH,
    LED_MODE_BLINK_3,
    LED_MODE_INVALID
} led_mode_t;

void led_init(void);
void led_set_mode(led_mode_t mode);
void led_set_mode_with_callback(led_mode_t mode, led_mode_t after_mode);
#endif