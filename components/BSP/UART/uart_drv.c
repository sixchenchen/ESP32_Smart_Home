#include "uart_drv.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "UART";
static QueueHandle_t uart_queue = NULL;
static uart_rx_callback_t rx_callback = NULL;

/*
  静态函数声明
*/
static void uart_task(void *arg);

static void uart_task(void *arg)
{
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
                int16_t length = event.size;
                if (length > UART_BUF_SIZE)
                {
                    length = UART_BUF_SIZE;
                }
                // UART_DATA 表示接收到数据
                int len = uart_read_bytes(
                    UART_PORT_NUM,
                    rx_buffer,
                    length,
                    0);
                if (len > 0)
                {
                     ESP_LOGI(TAG,"UART RX: ");
                    for (int i = 0; i < len; i++)
                    {
                        ESP_LOGI(TAG, "%02X ", rx_buffer[i]);
                        // 一个字节一个字节,交给协议状态机
                        if (rx_callback != NULL)
                        {
                            rx_callback(rx_buffer[i]);
                        }
                    }
                }
            }
            break;
            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "UART FIFO overflow");
                uart_flush_input(UART_PORT_NUM);
                xQueueReset(uart_queue);
                break;
            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "UART RX buffer full");
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
    uart_config_t uart_config =
        {
            .baud_rate = UART_BAUD_RATE,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
        };

    // UART参数
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    //  GPIO
    ESP_ERROR_CHECK(
        uart_set_pin(
            UART_PORT_NUM,
            UART_TX_GPIO,
            UART_RX_GPIO,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE));

    // 安装UART驱动,内部创建RX ISR
    ESP_ERROR_CHECK(
        uart_driver_install(
            UART_PORT_NUM,
            UART_BUF_SIZE,
            0,
            20,
            &uart_queue,
            0));
    // 创建接收任务
    xTaskCreate(uart_task, "uart_task", 4096, NULL, 10, NULL);
    ESP_LOGI(TAG, "uart init");
    return ESP_OK;
}

/*
    发送,无中断
*/
esp_err_t uart_drv_send(uint8_t *data, uint16_t len)
{

    int ret = uart_write_bytes(UART_PORT_NUM, (const char *)data, len);
    if (ret < 0)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}
void uart_drv_register_callback(uart_rx_callback_t callback)
{
    rx_callback = callback;
}
