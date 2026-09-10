#include "uart_drv.h"
#include "uart_config.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "UART_DRV";

static QueueHandle_t uart_queue = NULL;
static uart_rx_callback_t rx_callback = NULL;
static uart_rx_mos_byte_callback_t rx_mos_byte_callback = NULL;
static uart_rx_sen_byte_callback_t rx_sen_byte_callback = NULL;

static void uart_task(void *arg)
{
    ESP_LOGI(TAG, "uart_task started");
    uart_event_t event;
    uint8_t rx_buffer[UART_BUF_SIZE];
    while (1)
    {
        if (xQueueReceive(uart_queue, &event, portMAX_DELAY))
        {
            switch (event.type)
            {
            case UART_DATA:
            {
                int length = event.size;
                if (length > UART_BUF_SIZE)
                {
                    length = UART_BUF_SIZE;
                }
                // timeout=0 非阻塞读，立即返回当前已有字节，避免等待
                int len = uart_read_bytes(UART_PORT_NUM, rx_buffer, length, 0);
                if (len > 0)
                {
                    // 光栅传感器回调
                    if (rx_sen_byte_callback != NULL)
                    {
                        for (int i = 0; i < len; i++)
                        {
                            rx_sen_byte_callback(rx_buffer[i]);
                        }
                    }
                }
                break;
            }
            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "FIFO overflow");
                uart_flush_input(UART_PORT_NUM);
                xQueueReset(uart_queue);
                break;
            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "Buffer full");
                uart_flush_input(UART_PORT_NUM);
                xQueueReset(uart_queue);
                break;
            default:
                break;
            }
        }
    }
}

esp_err_t uart_drv_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(
        UART_PORT_NUM,
        UART_TX_GPIO,
        UART_RX_GPIO,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE));

    ESP_ERROR_CHECK(uart_driver_install(
        UART_PORT_NUM,
        UART_BUF_SIZE,
        0,
        20,
        &uart_queue,
        0));

    // priority 从 10 降到 7：与 sensor_task(7) / esp-mqtt / WiFi 在同一区间，避免 UART 采集任务抢占网络栈调度
    xTaskCreate(uart_task, "uart_task", 8192, NULL, 7, NULL);
    ESP_LOGI(TAG, "UART driver initialized");
    return ESP_OK;
}

esp_err_t uart_drv_send(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }
  
    int written = uart_write_bytes(UART_PORT_NUM, (const char *)data, len);
    if (written < 0)
    {
        return ESP_FAIL;
    }
    if (written < len)
    {
        ESP_LOGW(TAG, "uart_tx partial write: %d/%d", written, len);
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

void uart_drv_register_callback(uart_rx_callback_t callback)
{
    rx_callback = callback;
}
void uart_drv_register_mos_byte_callback(uart_rx_mos_byte_callback_t callback)
{
    rx_mos_byte_callback = callback;
}
void uart_drv_register_sen_byte_callback(uart_rx_sen_byte_callback_t callback)
{
    rx_sen_byte_callback = callback;
}
