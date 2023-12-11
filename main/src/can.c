#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_err.h"
#include "esp_log.h"

#include "driver/twai.h"
#include "app_internal.h"
#include "can.h"

System_DataTypedef *System_Data_CAN = NULL;

/* --------------------------- Tasks and Functions -------------------------- */

static void twai_receive_task(void *arg)
{
    for (;;)
    {
        // Receive data messages from slave
        if (system_is_debug_mode(System_Data_CAN))
        {
            twai_message_t rx_msg;
            twai_receive(&rx_msg, portMAX_DELAY);
#if DEBUG
            ESP_LOGI(TAG, "DLC: %d", rx_msg.data_length_code);
#endif
            for (int i = 0; i < rx_msg.data_length_code; i++)
            {
                System_Data_CAN->CAN1_Data[i] = (rx_msg.data[i]);
            }
            system_set_sys_change(System_Data_CAN);
        }
    }
    vTaskDelete(NULL);
}

void can_init(void *system_context)
{
    System_Data_CAN = (System_DataTypedef *)system_context;
    const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    const twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, TWAI_MODE_LISTEN_ONLY);
    // Install TWAI driver
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    // Create tasks, queues, and semaphores
    xTaskCreate(twai_receive_task, "TWAI_rx", 1024 * 5, NULL, RX_TASK_PRIO, NULL);
    ESP_ERROR_CHECK(twai_start());
}
