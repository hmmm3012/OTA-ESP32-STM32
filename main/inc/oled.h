#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#include <stdio.h>
#include "stdbool.h"
void oled_init(void *System_Data);
void oled_system_info(const char* name,const char* ssid,const char* ip,const bool state1,const bool state2);
void oled_debug( uint8_t *Can_data,const char *uart_rcv_buf);