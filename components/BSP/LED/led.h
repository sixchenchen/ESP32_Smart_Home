#ifndef __LED_H__
#define __LED_H__

#include "driver/gpio.h"

#define LED_GPIO_PIN GPIO_NUM_23

typedef enum
{
    PIN_RESET = 0,
    PIN_SET

} GPIO_OUTPUT_STATE;

#define LED(X)                                     \
    do                                             \
    {                                              \
        gpio_set_level(LED_GPIO_PIN,               \
                       (X) ? PIN_SET : PIN_RESET); \
    } while (0)

#define LED_TOGGLE()                                   \
    do                                                 \
    {                                                  \
        gpio_set_level(LED_GPIO_PIN,                   \
                       !gpio_get_level(LED_GPIO_PIN)); \
    } while (0)

void led_init(void);

#endif