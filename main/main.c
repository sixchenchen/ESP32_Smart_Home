#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "http_server.h"
#include "led.h"
#include "wifi_manager.h"
#include "uart_drv.h"
#include "mos.h"
#include "mos_protocol.h"
#include "mqtt_manager.h"
#include "string.h"
#include "device_context.h"
#include "key_manager.h"
#include "sen_protocol.h"
#include "sensor_manager.h"
#include "sensor_mqtt_bridge.h"
#include "esp_timer.h"

void app_main(void)
{
    led_init();
    key_manager_start();
    device_context_init();
    nvs_flash_init();
    wifi_manager_init();
    wifi_manager_start();
    MOS_Init();
    uart_drv_init();
    MOS_Protocol_Init();
    sen_protocol_init();
    sensor_manager_init();
    sensor_mqtt_bridge_init();

    while (1)
    {
        uint32_t now_ms = esp_timer_get_time() / 1000;

        sensor_manager_poll(now_ms);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}