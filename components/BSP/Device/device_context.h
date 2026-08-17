#ifndef DEVICE_CONTEXT_H
#define DEVICE_CONTEXT_H

#include "esp_err.h"
#include <stdint.h>

#define DEVICE_ID_LEN 32
#define DEVICE_NAME_LEN 32
#define DEVICE_NAME "ESP32-MOS"
#define HARDWARE_VERSION "V1.0"
#define FIRMWARE_VERSION "1.0.0"

typedef struct
{

    char device_id[DEVICE_ID_LEN];

    char device_name[DEVICE_NAME_LEN];

    char product_id[32];

    char hardware_version[16];

    char firmware_version[16];

    uint8_t base_mac[6];

    uint8_t sta_mac[6];

    uint8_t ap_mac[6];

    uint8_t bt_mac[6];

} device_context_t;

esp_err_t device_context_init(void);

const device_context_t *device_context_get(void);

#endif