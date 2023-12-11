#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_netif_net_stack.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#if IP_NAPT
#include "lwip/lwip_napt.h"
#endif
#include "lwip/err.h"
#include "lwip/sys.h"
#include "wifi.h"
#include "app_internal.h"
#include "soc/soc.h"
#if DEBUG
#include "esp_log.h"
    static const char *TAG_AP = "WiFi SoftAP";
static const char *TAG_STA = "WiFi Sta";
static const char *TAG_Scan = "WiFi Sta";
#endif

static int s_retry_num = 0;
static esp_netif_t *esp_netif_sta;
static esp_netif_t *esp_ap_cfg;
/* FreeRTOS event group to signal when we are connected/disconnected */
static EventGroupHandle_t s_wifi_event_group;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
#if DEBUG
        ESP_LOGI(TAG_AP, "Station " MACSTR " joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
#endif
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
#if DEBUG
        ESP_LOGI(TAG_AP, "Station " MACSTR " left, AID=%d",
                 MAC2STR(event->mac), event->aid);
#endif
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
#if DEBUG
        ESP_LOGI(TAG_STA, "Station started");
#endif
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
#if DEBUG
        ESP_LOGI(TAG_STA, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
#endif
        s_retry_num = 0;
        xEventGroupClearBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
#if DEBUG
        ESP_LOGI(TAG_STA, "Disconnected from AP");
#endif
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
    }
}
/* Initialize soft AP */
void wifi_init_softap(void)
{
    esp_ap_cfg = esp_netif_create_default_wifi_ap();
    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = ESP_WIFI_AP_SSID,
            .ssid_len = strlen(ESP_WIFI_AP_SSID),
            .channel = ESP_WIFI_CHANNEL,
            .password = ESP_WIFI_AP_PASSWD,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    if (strlen(ESP_WIFI_AP_PASSWD) == 0)
    {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
#if DEBUG
    ESP_LOGI(TAG_AP, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             ESP_WIFI_AP_SSID, ESP_WIFI_AP_PASSWD, ESP_WIFI_CHANNEL);
#endif

    //   return esp_netif_ap;
}

/* Initialize wifi station */
void wifi_init_sta(void)
// esp_netif_t *wifi_init_sta(void)
{
    esp_netif_sta = esp_netif_create_default_wifi_sta();
    wifi_config_t wft;
    esp_wifi_get_config(0, &wft);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wft));
#if DEBUG
    ESP_LOGI(TAG_STA, "%s", wft.sta.ssid);
    ESP_LOGI(TAG_STA, "wifi_init_sta finished.");
#endif
}

void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize event group */
    s_wifi_event_group = xEventGroupCreate();

    /* Register Event handler */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    /*Initialize WiFi */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

/* Initialize AP */
#if DEBUG
    ESP_LOGI(TAG_AP, "ESP_WIFI_MODE_AP");
#endif
    wifi_init_softap();

/* Initialize STA */
#if DEBUG
    ESP_LOGI(TAG_STA, "ESP_WIFI_MODE_STA");
#endif
    wifi_init_sta();

    /* Start WiFi */
    ESP_ERROR_CHECK(esp_wifi_start());

    /*
     * Wait until either the connection is established (WIFI_CONNECTED_BIT) or
     * connection failed for the maximum number of re-tries (WIFI_FAIL_BIT).
     * The bits are set by event_handler() (see above)
     */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           10000 / portTICK_PERIOD_MS);

    /* Set sta as the default interface */
    esp_netif_set_default_netif(esp_ap_cfg);

    /* Enable napt on the AP netif */
    /*
    if (esp_netif_napt_enable(esp_netif_ap) != ESP_OK) {
        ESP_LOGE(TAG_STA, "NAPT not enabled on the netif: %p", esp_netif_ap);
    }
    */
}

bool Connect_New_Wifi(const char nssid[], const char npass[])
{
    esp_wifi_disconnect();
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_DISCONNECTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           10000 / portTICK_PERIOD_MS);
    if (bits == WIFI_DISCONNECTED_BIT)
    {
#if DEBUG
        ESP_LOGI(TAG_STA, "Disconnect from AP successed");
#endif
    }
    else
    {
#if DEBUG
        ESP_LOGI(TAG_STA, "Not connect yet boy");
#endif
    }
    wifi_config_t wifi_sta_config = {
        .sta = {
            .bssid_set = 0,
            .ssid = "",
            .password = "",
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .failure_retry_cnt = ESP_MAXIMUM_RETRY,
            .threshold.authmode = WIFI_AUTH_WPA_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    for (int i = 0; i < strlen(nssid); i++)
        wifi_sta_config.sta.ssid[i] = nssid[i];
    if (strlen(npass) != 0)
    {
        for (int i = 0; i < strlen(npass); i++)
            wifi_sta_config.sta.password[i] = npass[i];
    }
#if DEBUG
    ESP_LOGI(TAG_STA, "SSID : %s", wifi_sta_config.sta.ssid);
    ESP_LOGI(TAG_STA, "PASS : %s", wifi_sta_config.sta.password);
#endif
    esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config);
    esp_err_t ret = esp_wifi_connect();
    // xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    bits = xEventGroupWaitBits(s_wifi_event_group,
                               WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                               pdFALSE,
                               pdFALSE,
                               10000 / portTICK_PERIOD_MS);
    /* xEventGroupWaitBits() returns the bits before the call returned,
     * hence we can test which event actually happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
#if DEBUG
        ESP_LOGI(TAG_STA, "connected to ap SSID:%s password:%s",
                 nssid, npass);
#endif
        return 1;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
#if DEBUG
        ESP_LOGI(TAG_STA, "Failed to connect to SSID:%s, password:%s",
                 nssid, npass);
#endif
        return 0;
    }
    else
    {
#if DEBUG
        ESP_LOGE(TAG_STA, "UNEXPECTED EVENT");
#endif
        return 0;
    }
}
void Scan_Wifi(char *str)
{
    strcat(str, "{\"SSID\":[");
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    wifi_scan_config_t scanf = {};
    scanf.scan_time.active.max = 200;
    esp_wifi_scan_start(&scanf, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
#if DEBUG
    ESP_LOGI(TAG_Scan, "Total APs scanned = %u", ap_count);
#endif
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++)
    {
#if DEBUG
        ESP_LOGI(TAG_Scan, "SSID \t%s", ap_info[i].ssid);
#endif
        strcat(str, "\"");
        strcat(str, &(ap_info[i].ssid));
        strcat(str, "\",");
    }
    strcat(str, "\"\"]}");
#if DEBUG
    ESP_LOGI(TAG_Scan, "%s", str);
#endif
}

// Need to update to check if sta connected to a AP or not
void Get_SSID(char *str)
{
    wifi_ap_record_t ap;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap);
    for (int i = 0; i < 33; i++)
        str[i] = ap.ssid[i];
}
void Get_IP(char *str)
{
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_sta, &ip_info);
    sprintf(str, IPSTR, IP2STR(&ip_info.ip));
}