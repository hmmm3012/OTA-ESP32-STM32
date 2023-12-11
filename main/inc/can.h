/* --------------------- Definitions and static variables ------------------ */

#define PING_PERIOD_MS 250
#define NO_OF_DATA_MSGS 50
#define NO_OF_ITERS 3
#define ITER_DELAY_MS 1000
#define RX_TASK_PRIO 8
#define TX_TASK_PRIO 9
#define CTRL_TSK_PRIO 10
#define TX_GPIO_NUM 4
#define RX_GPIO_NUM 5
#define TAG "TWAI"

#define ID_MASTER_STOP_CMD 0x0A0
#define ID_MASTER_START_CMD 0x0A1
#define ID_MASTER_PING 0x0A2
#define ID_SLAVE_STOP_RESP 0x0B0
#define ID_SLAVE_DATA 0x0B1
#define ID_SLAVE_PING_RESP 0x0B2

typedef enum
{
    TX_SEND_PINGS,
    TX_SEND_START_CMD,
    TX_SEND_STOP_CMD,
    TX_TASK_EXIT,
} tx_task_action_t;

typedef enum
{
    RX_RECEIVE_PING_RESP,
    RX_RECEIVE_DATA,
    RX_RECEIVE_STOP_RESP,
    RX_TASK_EXIT,
} rx_task_action_t;

void can_init(void *system_context);
