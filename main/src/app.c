// File Server
#include "app.h"
// Freertos
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// ESP LOG
#include "esp_log.h"
#include "esp_err.h"
#include "sdkconfig.h"
// Wifi
#include "STA_AP.h"
// Oled
#include "I2C_OLED.h"
// GPIO
#include "GPIO_OTA.h"
// CAN
#include "CAN_NODE.h"
// UART
#include "UART_NODE.h"
// Global
// void printServerData();
System_DataTypedef *system_data = NULL;
void initSystem();

static void print_Screen(void *pvParameters)
{
    uint8_t fScreen;
    while (1)
    {
        fScreen = system_data->sys_state;
        if ((fScreen & 0b10) != 0)
        {
            // Flashing screen
            if (fScreen == 2)
            {
                print_OTA_Screen(system_data->ssid, system_data->ip, system_data->STATE1, system_data->STATE2);
                system_data->sys_state &= 0b01;
            }
            // CAN screen
            else if (fScreen == 3)
            {
                print_CAN_Screen();
                memset(system_data->CAN1_Data, 0, sizeof(uint8_t) * 8);
                memset(system_data->CAN2_Data, 0, sizeof(uint8_t) * 8);
                memset(system_data->uart_buffer, 0, sizeof(uint8_t) * UART_RX_BUFFER_SIZE);
                system_data->sys_state &= 0b01;
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    initSystem();
    /* Init and configure GPIO pin used in system*/
    GPIO_OTA_Init(&system_data->sys_state);
    /* Uart init*/
    initUart(system_data->uart_buffer, &system_data->sys_state);
    /* Init OLED*/
    SH1106_init(system_data);
    /*Init and start wifi as STATION and AP mode*/
    Sta_AP_Start();
    // Start file server and mount storage
    ESP_ERROR_CHECK(mount_storage(system_data));
    ESP_ERROR_CHECK(start_file_server(system_data));
    /*Init and start CAN bus*/
    // CAN_Init(system_data);
    xTaskCreate(print_Screen, "print_Screen", 2048, NULL, 15, NULL);
}

void initSystem()
{
    /*Init system data*/
    system_data = calloc(1, sizeof(System_DataTypedef));
    system_data->STATE1 = 1; // MCU 1 run
    system_data->STATE2 = 1; // MCU 2 run
    system_data->sys_state = 2; // Boot mode, change : 0011
    strlcpy(system_data->base_path, "/data", sizeof(system_data->base_path));
}