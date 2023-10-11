#include "stdbool.h"
#include "esp_vfs.h"

#define DEBUG 1
#define UART UART_NUM_1
#define SCRATCH_BUFSIZE 8192
#define UART_RX_BUFFER_SIZE 1024
#define MODE_BOOT 0
#define MODE_DEBUG 1
#define MODE_CHANGE (1<<1)
#define MODE_NOCHANGE (0<<1)

typedef struct System_Data
{
    /* Current SSID */
    char ssid[32];
    /* Current IP*/
    char ip[16];
    /* SPI info*/
    size_t total, used;
    /* Mcu state */
    bool STATE1, STATE2;
    /* Flag Oled */
    // 1: Boot/Debug mode , 2: Changed 
    uint8_t sys_state; 
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];
    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
    /* CAN data buf*/
    uint8_t CAN1_Data[8];
    uint8_t CAN2_Data[8];
    // Uart buffer
    uint8_t uart_buffer[UART_RX_BUFFER_SIZE];

} System_DataTypedef;

#ifdef __cplusplus
extern "C"
{
#endif

    esp_err_t mount_storage(System_DataTypedef *Para_system_data);
    esp_err_t start_file_server(System_DataTypedef *Para_system_data);

#ifdef __cplusplus
}
#endif
