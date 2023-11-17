#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/uart.h"

#include "uart.h"
#include "app_internal.h"

#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
#define TXD_PIN (17)
#define RXD_PIN (16)

static const char *TAG = "UART";
static QueueHandle_t uart0_queue;
System_DataTypedef* system_DATA_UART ;

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    for (;;)
    {
        // Waiting for UART event.
        if (xQueueReceive(uart0_queue, (void *)&event, (TickType_t)portMAX_DELAY))
        {
            if (system_is_debug_mode(system_DATA_UART))
            {
                // bzero(system_DATA_UART->uart_buffer, RD_BUF_SIZE);
                switch (event.type)
                {
                // Event of UART receving data
                case UART_DATA:
                    uart_read_bytes(UART, system_DATA_UART->uart_buffer, event.size, portMAX_DELAY);
                    ESP_LOGI(TAG, "[READ DATA]: %s", system_DATA_UART->uart_buffer);
                    // uart_flush(UART);
                    system_set_sys_state(system_DATA_UART,MODE_CHANGE|MODE_DEBUG);
                    break;
                // Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    // ESP_LOGI(TAG, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(UART);
                    xQueueReset(uart0_queue);
                    break;
                // Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    // ESP_LOGI(TAG, "ring buffer full");
                    // If buffer full happened, you should consider encreasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(UART);
                    xQueueReset(uart0_queue);
                    break;
                // Event of UART RX break detected
                case UART_BREAK:
                    system_set_sys_state(system_DATA_UART,MODE_CHANGE|MODE_DEBUG);
                    ESP_LOGI(TAG, "uart rx break");
                    break;
                // Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGI(TAG, "uart parity error");
                    break;
                // Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "uart frame error");
                    break;
                default:
                    ESP_LOGI(TAG, "uart event type: %d", event.type);
                    break;
                }
            }
        }
    }
    vTaskDelete(NULL);
}
void uart_init(void* system_data)
{
    system_DATA_UART = (System_DataTypedef*) system_data;
    esp_log_level_set(TAG, ESP_LOG_INFO);
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART, &uart_config);
    uart_set_pin(UART, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // Install UART driver, and get the queue.
    uart_driver_install(UART, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart0_queue, 0);
    // Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    // Create a task to handler UART event from ISR
    TaskHandle_t uart_rx = NULL;
    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 12, &uart_rx);
}
