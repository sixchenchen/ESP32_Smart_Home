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
                int len = uart_read_bytes(UART_PORT_NUM, rx_buffer, length, 0);
                if (len > 0)
                {
                    // // 单字节控制mos回调
                    // if (rx_mos_byte_callback != NULL)
                    // {
                    //     for (int i = 0; i < len; i++)
                    //     {
                    //         rx_mos_byte_callback(rx_buffer[i]);
                    //     }
                    // }
                    // // 多字节控制mos回调
                    // if (rx_callback != NULL)
                    // {
                    //     rx_callback(rx_buffer, len);
                    // }
                    // 光栅传感器回调
                    if (rx_sen_byte_callback != NULL)
                    {
                        ESP_LOGW(TAG, "rx_sen_byte_callback->:%d\n",length);
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

    xTaskCreate(uart_task, "uart_task", 4096, NULL, 10, NULL);
    ESP_LOGI(TAG, "UART driver initialized");
    return ESP_OK;
}

esp_err_t uart_drv_send(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int ret = uart_write_bytes(UART_PORT_NUM, (const char *)data, len);
    return (ret >= 0) ? ESP_OK : ESP_FAIL;
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
