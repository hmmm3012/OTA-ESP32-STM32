#include "app_internal.h"

#include <stdio.h>
#include <string.h>
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"

#include "esp_log.h"
#include "esp_err.h"
static const char *TAG = "SPIFFS";

/* Function to initialize SPIFFS */
esp_err_t mount_storage(System_DataTypedef *Para_system_data)
{
#if DEBUG
    ESP_LOGI(TAG, "Initializing SPIFFS");
#endif
    esp_vfs_spiffs_conf_t conf = {
        .base_path = Para_system_data->base_path,
        .partition_label = NULL,
        .max_files = 1, // This sets the maximum number of files that can be open at the same time
        .format_if_mount_failed = true};

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
#if DEBUG
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
#endif
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
#if DEBUG
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
#endif
        }
        else
        {
#if DEBUG
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
#endif
        }
        return ret;
    }
    ret = esp_spiffs_info(NULL, &Para_system_data->total, &Para_system_data->used);
    if (ret != ESP_OK)
    {
#if DEBUG
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
#endif
        return ret;
    }
#if DEBUG
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", Para_system_data->total, Para_system_data->used);
#endif
    return ESP_OK;
}
