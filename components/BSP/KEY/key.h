#ifndef KEY_H
#define KEY_H

#include "esp_err.h"

#define KEY_LONG_PRESS_TIME_MS 2000

#define BOOT_INT_GPIO_PIN GPIO_NUM_0

esp_err_t key_init(void);

#endif