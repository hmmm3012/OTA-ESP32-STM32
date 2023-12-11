#include "oled.h"
#include "app_internal.h"
#include "I2C_OLED.h"
#include "esp_log.h"
void oled_init(void *System_Data)
{
	sh1106_init();
}

void oled_system_info(const char *name, const char *ssid, const char *ip, const bool state1, const bool state2)
{
	char buf[500] = "";
	sh1106_display_clear(NULL);
	sprintf(buf, "SSID:\n\t%s\nIP:\n%s\nMCU 1:\t\t%s\nMCU 2:\t\t%s\n%s",
			ssid == NULL ? "Not connected" : ssid,
			ip == NULL ? "Not connected" : ip,
			state1 == 1 ? "RUN" : "OFF",
			state2 == 1 ? "RUN" : "OFF",
			name);
	sh1106_display_text(buf);
}

void oled_debug(uint8_t *can_rcv_buf, const char *uart_rcv_buf)
{
	sh1106_display_clear(NULL);
	char Uart_data[32];
	// strcpy(Uart_data,uart_rcv_buf);
	memcpy(Uart_data, uart_rcv_buf, 32);
	char Uart_scr[64];
	char Can_scr[64];
	char Screen[128];
	sprintf(Can_scr, "CAN:%x %x %x %x\n%x %x %x %x", can_rcv_buf[0], can_rcv_buf[1], can_rcv_buf[2], can_rcv_buf[3], can_rcv_buf[4], can_rcv_buf[5], can_rcv_buf[6], can_rcv_buf[7]);
	sprintf(Uart_scr, "Uart: %s", uart_rcv_buf);
	sprintf(Screen, "%s\n%s", Uart_scr, Can_scr);
#if DEBUG
	ESP_LOGI("OLED", "%s", Screen);
#endif
	sh1106_display_text(Screen);
}