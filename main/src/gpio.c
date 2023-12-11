#include <stdio.h>
#include "driver/gpio.h"
#include "gpio.h"
#include "driver/uart.h"

#include "app_internal.h"
#if DEBUG
#include "esp_log.h"
#endif

#define BOOT1 23
#define BOOT2 18
#define RESET1 14
#define RESET2 19
#define GPIO_OUTPUT_SEL (1 << BOOT1 | 1 << BOOT2 | 1 << RESET1 | 1 << RESET2)
#define BUTTON_1 32
#define BUTTON_2 33
#define GPIO_BTN_SEL (1ULL << BUTTON_1 | 1ULL << BUTTON_2)
#define LEDR 25
#define LEDG 26
#define LEDB 27
#define LED_SEL (1ULL << LEDR | 1ULL << LEDG | 1ULL << LEDB)
#define TX_PAD 17
#define RX_PAD 16
#define UART_PAD (1ULL << TX_PAD | 1ULL << RX_PAD)

static const char *TAG = "GPIO";
System_DataTypedef *system_DATA_GPIO;

static IRAM_ATTR void BTN1_handler()
{
    uart_set_parity(UART_NUM_1, UART_PARITY_EVEN);
    system_set_sys_state(system_DATA_GPIO, MODE_BOOT | MODE_CHANGE);
}
static IRAM_ATTR void BTN2_handler()
{
    uart_set_parity(UART_NUM_1, UART_PARITY_DISABLE);
    system_set_sys_state(system_DATA_GPIO, MODE_DEBUG | MODE_CHANGE);
}
void gpio_init(void *PARA_system_DATA)
{
    system_DATA_GPIO = (System_DataTypedef *)PARA_system_DATA;
    // Configure GPIO for OTA
    gpio_config_t OTA_conf = {};
    OTA_conf.mode = GPIO_MODE_OUTPUT;
    OTA_conf.intr_type = GPIO_INTR_DISABLE;
    OTA_conf.pin_bit_mask = GPIO_OUTPUT_SEL;
    OTA_conf.pull_down_en = 0;
    OTA_conf.pull_up_en = 1;
    gpio_config(&OTA_conf);

    // Configure GPIO for button
    gpio_config_t BTN_conf = {};
    BTN_conf.mode = GPIO_MODE_INPUT;
    BTN_conf.intr_type = GPIO_INTR_NEGEDGE;
    BTN_conf.pin_bit_mask = GPIO_BTN_SEL;
    BTN_conf.pull_down_en = 0;
    BTN_conf.pull_up_en = 1;
    gpio_config(&BTN_conf);

    gpio_config_t LED_conf = {};
    LED_conf.mode = GPIO_MODE_OUTPUT;
    LED_conf.intr_type = GPIO_INTR_DISABLE;
    LED_conf.pin_bit_mask = LED_SEL;
    LED_conf.pull_down_en = 0;
    LED_conf.pull_up_en = 0;
    gpio_config(&LED_conf);
#if DEBUG
    gpio_set_level(LEDR, 0);
    gpio_set_level(LEDG, 0);
    gpio_set_level(LEDB, 0);
#endif
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_1, BTN1_handler, NULL);
    gpio_isr_handler_add(BUTTON_2, BTN2_handler, NULL);
}

void gpio_run_both()
{
    gpio_set_level(BOOT1, 0);
    gpio_set_level(BOOT2, 0);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(RESET1, 0);
    gpio_set_level(RESET2, 0);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(RESET1, 1);
    gpio_set_level(RESET2, 1);
#if DEBUG
    ESP_LOGI(TAG, "MCU 1 : RUN");
    ESP_LOGI(TAG, "MCU 2 : RUN");
#endif
}

void gpio_run_mcu1()
{
    gpio_run_both();
    gpio_set_level(RESET2, 0);
#if DEBUG
    ESP_LOGI(TAG, "MCU 1 : RUN");
    ESP_LOGI(TAG, "MCU 2 : OFF");
#endif
}

void gpio_run_mcu2()
{
    gpio_run_both();
    gpio_set_level(RESET1, 0);
#if DEBUG
    ESP_LOGI(TAG, "MCU 1 : OFF");
    ESP_LOGI(TAG, "MCU 2 : RUN");
#endif
}

void gpio_boot_mcu1()
{
    gpio_set_level(BOOT1, 1);
    gpio_set_level(RESET2, 0);
    gpio_set_level(RESET1, 0);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(RESET1, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

void gpio_boot_mcu2()
{
    gpio_set_level(BOOT2, 1);
    gpio_set_level(RESET1, 0);
    gpio_set_level(RESET2, 0);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(RESET2, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
}
