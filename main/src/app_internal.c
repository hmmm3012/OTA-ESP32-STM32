#include "app_internal.h"
#include <stdio.h>
#include "driver/uart.h"
// Init system data
System_DataTypedef *system_init()
{
    System_DataTypedef *system_DATA = calloc(1, sizeof(System_DataTypedef));
    system_DATA->name = CAR_NAME;
    system_DATA->mcu1_state = 1; // MCU 1 run
    system_DATA->mcu2_state = 1; // MCU 2 run
    system_DATA->sys_state = 2;  // Boot mode, change : 0011
    // strcpy(system_DATA->base_path, "/data/");
    strlcpy(system_DATA->base_path, "/data", sizeof(system_DATA->base_path));
    return system_DATA;
}
// Update state of MCU 1 and MCU 2
void system_update_mcu_state(System_DataTypedef *system_DATA, bool state1, bool state2)
{
    system_DATA->mcu1_state = state1;
    system_DATA->mcu2_state = state2;
}
// Update state of system
void system_set_sys_state(System_DataTypedef *system_DATA, uint8_t state)
{
    system_DATA->sys_state = state;
    if (system_is_debug_mode(system_DATA))
        uart_set_parity(UART_NUM_1, UART_PARITY_DISABLE);
    else
        uart_set_parity(UART_NUM_1, UART_PARITY_EVEN);
}

void system_set_sys_change(System_DataTypedef *system_DATA)
{
    system_DATA->sys_state = system_DATA->sys_state | 0b10;
}
// Get state of system
uint8_t system_get_sys_state(System_DataTypedef *system_DATA)
{
    return system_DATA->sys_state;
}
// Get state of MCU 1
bool system_get_mcu1_state(System_DataTypedef *system_DATA)
{
    return system_DATA->mcu1_state;
}
// Get state of MCU 2
bool system_get_mcu2_state(System_DataTypedef *system_DATA)
{
    return system_DATA->mcu2_state;
}
// Get total size of SPI
size_t system_get_spi_total(System_DataTypedef *system_DATA)
{
    return system_DATA->total;
}
// Get used size of SPI
size_t system_get_spi_used(System_DataTypedef *system_DATA)
{
    return system_DATA->used;
}
// Check if system is in debug mode
bool system_is_debug_mode(System_DataTypedef *system_DATA)
{
    return (system_DATA->sys_state & 0b1) == 1;
}
// Check if system is changed
bool system_is_changed(System_DataTypedef *system_DATA)
{
    return (system_DATA->sys_state & 0b10) == 2;
}