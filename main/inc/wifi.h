/* STA Configuration */
#define ESP_MAXIMUM_RETRY 2
#define DEFAULT_SCAN_LIST_SIZE 5


/* AP Configuration */
#define ESP_WIFI_AP_SSID "CAR_3"
#define ESP_WIFI_AP_PASSWD ""
#define ESP_WIFI_CHANNEL 1
#define MAX_STA_CONN 5

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_DISCONNECTED_BIT BIT2
/*
    - Init Wifi Station and AP mode 
*/
void wifi_init(void);
bool Connect_New_Wifi(const char nssid[], const char npass[]);
void Scan_Wifi(char *str);
void Get_SSID(char* str);
void Get_IP(char* str);