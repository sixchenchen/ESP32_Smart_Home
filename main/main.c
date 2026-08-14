#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "http_server.h"
#include "led.h"
#include "wifi_manager.h"
#include "key.h"
#include "uart_drv.h"
#include "mos.h"
#include "mos_protocol.h"
#include "mqtt_manager.h"
#include "string.h"
#include "device_info.h"

void app_main(void)
{
    led_init();
    nvs_flash_init();
    wifi_manager_init();
    wifi_manager_start();
    key_init();
    MOS_Init();
    uart_drv_init();
    MOS_Protocol_Init();
    mqtt_manager_init();
    device_print_info();
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}