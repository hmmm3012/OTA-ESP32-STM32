#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "string.h"
#include "stm32ota.h"
#include "app_internal.h"

char *STM32_CHIPNAME[2] = {
    "Unknown Chip",
    "STM32F103x8/B",
};

static const char *RX_TASK_TAG = "UART";
static uint8_t data[5];

void stm32SendCommand(uint8_t commd)
{
  uart_write_bytes(UART, &commd, 1);
  commd = ~commd;
  uart_write_bytes(UART, &commd, 1);
}

bool stm32Erase()
{
  uart_flush(UART);
  stm32SendCommand(STM32ERASE);
  uart_read_bytes(UART, data, 1, 3000 / portTICK_PERIOD_MS);
  if (data[0] == STM32ACK)
  {
    uint8_t commd = 0xFF;
    uart_write_bytes(UART, &commd, 1);
    commd = 0x00;
    uart_write_bytes(UART, &commd, 1);
#if DEBUG
    ESP_LOGI(RX_TASK_TAG, "ERASE DONE");
#endif
  }
  else
    return 0;
  return 1;
}

bool stm32Erasen()
{
  uart_flush(UART);
  stm32SendCommand(STM32ERASEN);
  uart_read_bytes(UART, data, 1, 1000 / portTICK_PERIOD_MS);
  if (data[0] == STM32ACK)
  {
    ESP_LOGI(RX_TASK_TAG, "ERASE DONE");
    uint8_t commd = 0xFF;
    uart_write_bytes(UART, &commd, 1);
    uart_write_bytes(UART, &commd, 1);
    commd = 0x00;
    uart_write_bytes(UART, &commd, 1);
  }
  else
    return 0;
  return 1;
}

uint8_t stm32Address(unsigned long addr)
{
  uint8_t sendaddr[4];
  uint8_t addcheck = 0;
  sendaddr[0] = addr >> 24;
  sendaddr[1] = (addr >> 16) & 0xFF;
  sendaddr[2] = (addr >> 8) & 0xFF;
  sendaddr[3] = addr & 0xFF;
  uart_flush(UART);
  for (int i = 0; i <= 3; i++)
  {
    uint8_t commd = sendaddr[i];
    uart_write_bytes(UART, &commd, 1);

    addcheck ^= sendaddr[i];
  }
  uart_write_bytes(UART, &addcheck, 1);
  data[0] = 0;
  uart_read_bytes(UART, data, 1, 1000 / portTICK_PERIOD_MS);
  return data[0] == STM32ACK ? 1 : 0;
}

uint8_t stm32SendData(uint8_t *data, uint8_t wrlen)
{
  uart_flush(UART);
  uart_write_bytes(UART, &wrlen, 1);
  for (int i = 0; i <= wrlen; i++)
  {
    uart_write_bytes(UART, data + i, 1);
  }
  uint8_t Csum = getChecksum(data, wrlen);
  uart_write_bytes(UART, &Csum, 1);
  data[0] = 0;
  uart_read_bytes(UART, data, 1, 1000 / portTICK_PERIOD_MS);
  return data[0] == STM32ACK ? 1 : 0;
}

bool initSTM32()
{
  data[0] = 0;
  uint8_t commd = STM32INIT;
  uart_flush(UART);
  uart_write_bytes(UART, &commd, 1);
  uart_read_bytes(UART, data, 1, 3000 / portTICK_PERIOD_MS);
#if DEBUG
  ESP_LOGI("Init stm:", "rec: %X %X %X %X %X", data[0], data[1], data[2], data[3], data[4]);
#endif
  if (data[0] == STM32ACK)
  {
    return 1;
  }
  return 0;
}

uint8_t getChecksum(uint8_t *data, uint8_t datalen)
{
  uint8_t lendata = datalen;
  for (int i = 0; i <= datalen; i++)
    lendata ^= data[i];
  return lendata;
}

/*
char stm32Version() {     // Tested
  unsigned char vsbuf[14];
  stm32SendCommand(STM32GET);
  while (!Serial.available());
  vsbuf[0] = Serial.read();
  if (vsbuf[0] != STM32ACK)
    return STM32ERR;
  else {
    Serial.readBytesUntil(STM32ACK, vsbuf, 14);
    return vsbuf[1];
  }
}
*/
/*
char* stm32GetId() {     // Tested
  int getid = 0;
  uart_flush(UART);
  stm32SendCommand(STM32ID);
  const int rxBytes = uart_read_bytes(UART, data, 5, 1000 / portTICK_PERIOD_MS);
  ESP_LOGI(RX_TASK_TAG, "rec: %d bytes", rxBytes);
  for (int i = 0; i < 5;i++){
    ESP_LOGI(RX_TASK_TAG, "REV[%d]: %X ",i, data[i]);
  }
  if (data[0] == STM32ACK)
  {
    getid = data[2];
    getid = (getid << 8) + data[3];
    if (getid == 0x410)
      return STM32_CHIPNAME[1];
  }
  return STM32_CHIPNAME[0];
}*/
