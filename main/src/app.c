// File Server
#include "app_internal.h"
// Freertos
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// ESP LOG
#include "esp_log.h"
#include "esp_err.h"
#include "sdkconfig.h"
// Wifi
#include "wifi.h"
// Oled
#include "oled.h"
// GPIO
#include "gpio.h"
// CAN
#include "can.h"
// UART
#include "uart.h"
// Global variable
System_DataTypedef *system_DATA = NULL;

static void print_Screen(void *pvParameters)
{
    while (1)
    {
        if (system_is_changed(system_DATA))
        {
            // system info screen
            if (!system_is_debug_mode(system_DATA))
            {
                oled_system_info(system_DATA->name, system_DATA->ssid, system_DATA->ip, system_DATA->mcu1_state, system_DATA->mcu2_state);
                system_set_sys_state(system_DATA, MODE_NOCHANGE | MODE_BOOT);
            }
            // debug UART and CAN screen
            else
            {
                oled_debug(system_DATA->CAN1_Data, system_DATA->uart_buffer);
                memset(system_DATA->CAN1_Data, 0, sizeof(uint8_t) * 8);
                memset(system_DATA->uart_buffer, 0, sizeof(uint8_t) * UART_RX_BUFFER_SIZE);
                system_set_sys_state(system_DATA, MODE_NOCHANGE | MODE_DEBUG);
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    system_DATA = system_init();
    /* Init and configure GPIO pin used in system*/
    gpio_init(system_DATA);
    /* Uart init*/
    uart_init(system_DATA);
    /* Init OLED*/
    oled_init(system_DATA);
    /*Init and start wifi as STATION and AP mode*/
    wifi_init();
    // Start file server and mount storage
    ESP_ERROR_CHECK(mount_storage(system_DATA));
    ESP_ERROR_CHECK(start_file_server(system_DATA));
    /*Init and start CAN bus*/
    can_init(system_DATA);
    xTaskCreate(print_Screen, "print_Screen", 1024 * 3, NULL, 15, NULL);
}