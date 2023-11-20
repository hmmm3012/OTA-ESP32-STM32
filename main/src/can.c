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

// static const twai_message_t start_message = {.identifier = ID_MASTER_START_CMD, .data_length_code = 8, .data = {'h','e','l','l','o','a','a','a'}};
// static QueueHandle_t tx_task_queue;

static QueueHandle_t rx_task_queue;
System_DataTypedef *System_Data = NULL;

/* --------------------------- Tasks and Functions -------------------------- */

static void twai_receive_task(void *arg)
{
    for (;;)
    {
        rx_task_action_t action;
        xQueueReceive(rx_task_queue, &action,(TickType_t) portMAX_DELAY);
        // Receive data messages from slave
        while(1){
        if(system_is_debug_mode(System_Data)){
            twai_message_t rx_msg;
            twai_receive(&rx_msg, portMAX_DELAY);
#if DEBUG
            ESP_LOGI(TAG, "DLC: %d", rx_msg.data_length_code);
#endif
            for (int i = 0; i < rx_msg.data_length_code; i++)
            {
                System_Data->CAN1_Data[i] = (rx_msg.data[i]);
            }
            system_set_sys_state(System_Data, MODE_DEBUG | MODE_CHANGE);
#if DEBUG
            ESP_LOGI(TAG, "Received data value");
#endif
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(NULL);
}

// static void twai_transmit_task(void *arg)
// {
//     while(1){
//         tx_task_action_t action;
//         xQueueReceive(tx_task_queue, &action, portMAX_DELAY);
//         while(1){
//         twai_transmit(&start_message, portMAX_DELAY);
//         #if DEBUG
//             ESP_LOGI(TAG, "Sending");
//         #endif
//             vTaskDelay(50 / portTICK_PERIOD_MS);
//         }
//     }
// }

void can_init(void *system_context)
{
    System_Data = (System_DataTypedef *) system_context;
    const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    const twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, TWAI_MODE_NORMAL);
    // Install TWAI driver    
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    // Create tasks, queues, and semaphores
    rx_task_queue = xQueueCreate(1, sizeof(rx_task_action_t));
    // tx_task_queue = xQueueCreate(1, sizeof(tx_task_action_t));
    // xTaskCreatePinnedToCore(twai_transmit_task, "TWAI_tx", 4096, NULL, TX_TASK_PRIO, NULL, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(twai_receive_task, "TWAI_rx", 1024*5, NULL, RX_TASK_PRIO, NULL, tskNO_AFFINITY); 
    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(TAG, "Driver started");
    rx_task_action_t rx_action = RX_RECEIVE_DATA;
    xQueueSend(rx_task_queue, &rx_action, portMAX_DELAY);
}
