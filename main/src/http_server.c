// C library
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "stdbool.h"
// LOGGING
#include "esp_err.h"
#include "esp_log.h"
// SPIFFS
#include "esp_vfs.h"
#include "esp_spiffs.h"
// HTTP server
#include "app_internal.h"
#include "esp_http_server.h"
// OTA
#include "driver/uart.h"
#include "stm32ota.h"
#include "gpio.h"
#include "wifi.h"

/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE (200 * 1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"
#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)
/*Web page binary*/
extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const unsigned char favicon_ico_end[] asm("_binary_favicon_ico_end");
extern const unsigned char ota_start[] asm("_binary_ota_html_start");
extern const unsigned char ota_end[] asm("_binary_ota_html_end");
extern const unsigned char style_start[] asm("_binary_style_css_start");
extern const unsigned char style_end[] asm("_binary_style_css_end");
extern const unsigned char wifimanager_start[] asm("_binary_wifimanager_html_start");
extern const unsigned char wifimanager_end[] asm("_binary_wifimanager_html_end");
extern const unsigned char about_start[] asm("_binary_about_html_start");
extern const unsigned char about_end[] asm("_binary_about_html_end");
extern const unsigned char list_start[] asm("_binary_list_html_start");
extern const unsigned char list_end[] asm("_binary_list_html_end");
System_DataTypedef *system_data_FS = NULL;

#if DEBUG
static const char *TAG = "file_server";
#endif

// Handler by me
static esp_err_t cfgWifi_handler(httpd_req_t *req);
static esp_err_t scan_handler(httpd_req_t *req);
static esp_err_t reload_json(httpd_req_t *req);
static esp_err_t wifi_manager_handler(httpd_req_t *req);
static esp_err_t style_handler(httpd_req_t *req);
static esp_err_t ota_manager_handler(httpd_req_t *req);
static esp_err_t about_handler(httpd_req_t *req);
static esp_err_t favicon_get_handler(httpd_req_t *req);
static esp_err_t download_mcu_handler(httpd_req_t *req);
static esp_err_t http_resp_dir_html(httpd_req_t *req);
static const char *get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize);
static esp_err_t upload_post_handler(httpd_req_t *req);
static esp_err_t delete_post_handler(httpd_req_t *req);
/* Function to start the file server */
esp_err_t start_file_server(System_DataTypedef *Para_system_data)
{
    system_data_FS = Para_system_data;
    gpio_run_both();
    Get_SSID(system_data_FS->ssid);
    Get_IP(system_data_FS->ip);
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // SET max URI server can handle
    config.max_uri_handlers = 13;

    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;

#if DEBUG
    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
#endif

    if (httpd_start(&server, &config) != ESP_OK)
    {
#if DEBUG
        ESP_LOGE(TAG, "Failed to start file server!");
#endif
        return ESP_FAIL;
    }

    /* URI handler for CSS request */
    httpd_uri_t style_get = {
        .uri = "/style.css",
        .method = HTTP_GET,
        .handler = style_handler,
        .user_ctx = NULL,
    };
    /* URI handler for FAV icon request */
    httpd_uri_t favicon_handle = {
        .uri = "/favicon.ico",
        .method = HTTP_GET,
        .handler = favicon_get_handler,
        .user_ctx = NULL,
    };
    /* URI handler for download program to MCU */
    httpd_uri_t file_download = {
        .uri = "/download*",
        .method = HTTP_GET,
        .handler = download_mcu_handler,
        .user_ctx = NULL,
    };
    /* URI handler for uploading files to server */
    httpd_uri_t file_upload = {
        .uri = "/upload/*",
        .method = HTTP_POST,
        .handler = upload_post_handler,
        .user_ctx = NULL,
    };

    /* URI handler for deleting files from server */
    httpd_uri_t file_delete = {
        .uri = "/delete/*",
        .method = HTTP_POST,
        .handler = delete_post_handler,
        .user_ctx = NULL,
    };
    /* URI handler for Wifi Manager page request */
    httpd_uri_t wifi_manager = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = wifi_manager_handler,
        .user_ctx = NULL, // Pass server data as context
    };

    /* URI handler for About page request */
    httpd_uri_t about_page = {
        .uri = "/about",
        .method = HTTP_GET,
        .handler = about_handler,
        .user_ctx = NULL // Pass server data as context
    };

    /* URI handler for OTA request */
    httpd_uri_t ota_manager = {
        .uri = "/ota*",
        .method = HTTP_GET,
        .handler = ota_manager_handler,
        .user_ctx = NULL, // Pass server data as context
    };

    /* URI handler for list file request */
    httpd_uri_t list_file = {
        .uri = "/list",
        .method = HTTP_GET,
        .handler = http_resp_dir_html,
        .user_ctx = NULL // Pass server data as context
    };

    /* URI handler for scan wifi request */
    httpd_uri_t scan_handle = {
        .uri = "/scan",
        .method = HTTP_GET,
        .handler = scan_handler,
        .user_ctx = NULL};
    httpd_uri_t cfgWifi_handle = {
        .uri = "/cfgwifi",
        .method = HTTP_POST,
        .handler = cfgWifi_handler,
        .user_ctx = NULL,
    };
    // Register URI handler
    httpd_register_uri_handler(server, &file_download);
    httpd_register_uri_handler(server, &file_upload);
    httpd_register_uri_handler(server, &file_delete);
    httpd_register_uri_handler(server, &ota_manager);
    httpd_register_uri_handler(server, &about_page);
    httpd_register_uri_handler(server, &style_get);
    httpd_register_uri_handler(server, &list_file);
    httpd_register_uri_handler(server, &favicon_handle);
    httpd_register_uri_handler(server, &scan_handle);
    httpd_register_uri_handler(server, &cfgWifi_handle);
    httpd_register_uri_handler(server, &wifi_manager);

    return ESP_OK;
}
static esp_err_t cfgWifi_handler(httpd_req_t *req)
{
    char *buf = malloc(req->content_len);
    char *ssid = malloc(req->content_len);
    char *pass = malloc(req->content_len);
    httpd_req_recv(req, buf, req->content_len);
#if DEBUG
    ESP_LOGI(TAG, "%s", buf);
#endif
    int i, j;
    for (i = 5, j = 0; i < req->content_len; j++, i++)
    {
        if (buf[i] == '&')
            break;
        if (buf[i] == '+')
            ssid[j] = ' ';
        else
            ssid[j] = buf[i];
    }
    ssid[j] = '\0';
    for (i += 6, j = 0; i < req->content_len; j++, i++)
    {
        pass[j] = buf[i];
    }
    pass[j] = '\0';
    if (Connect_New_Wifi(ssid, pass))
    {
        httpd_resp_sendstr(req, "success");
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Wrong ssid or password or some unexpected events");
    }
    Get_SSID(system_data_FS->ssid);
    Get_IP(system_data_FS->ip);
    system_set_sys_state(system_data_FS, MODE_BOOT | MODE_CHANGE);
    free(buf);
    free(ssid);
    free(pass);
    return ESP_OK;
}
static esp_err_t scan_handler(httpd_req_t *req)
{
    char str[500] = "";
    Scan_Wifi(str);
    httpd_resp_sendstr(req, str);
    return ESP_OK;
}
static esp_err_t reload_json(httpd_req_t *req)
{
    size_t TOTAL = system_get_spi_total(system_data_FS);
    size_t USED = system_get_spi_used(system_data_FS);
    bool MCU1_STATE = system_get_mcu1_state(system_data_FS);
    bool MCU2_STATE = system_get_mcu2_state(system_data_FS);
    esp_err_t ret = esp_spiffs_info(NULL, &TOTAL, &USED);
    if (ret != ESP_OK)
    {
#if DEBUG
        ESP_LOGE("SPIFFS", "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
#endif
    }
    char buf[260];
    char str[1000] = "{\"spiStorage\":";
#if DEBUG
    ESP_LOGI(TAG, "GET SPI INFO DONE");
#endif
    sprintf(str, "{\"spistorage\":\"%d\",\"spiused\":\"%d\",\"state1\":\"%s\",\"state2\":\"%s\",\"files\":[", TOTAL, USED, MCU1_STATE == 1 ? "running" : "off", MCU2_STATE == 1 ? "running" : "off");
    const char *dirpath = "/data/";
    char entrypath[FILE_PATH_MAX];
    char entrysize[16];
    const char *entrytype;
    struct dirent *entry;
    struct stat entry_stat;
    DIR *dir = opendir(dirpath);
    const size_t dirpath_len = strlen(dirpath);
    strlcpy(entrypath, dirpath, sizeof(entrypath));
    while ((entry = readdir(dir)) != NULL)
    {
        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");
        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
        if (stat(entrypath, &entry_stat) == -1)
        {
#if DEBUG
            ESP_LOGE(TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
#endif
            continue;
        }
        sprintf(entrysize, "%ld", entry_stat.st_size);
#if DEBUG
        ESP_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);
#endif
        sprintf(buf, "\"%s\",", entry->d_name);
        strcat(str, buf);
    }
    closedir(dir);
    strcat(str, "\"\"]}");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, str);
    return ESP_OK;
}
static esp_err_t download_mcu_handler(httpd_req_t *req)
{
#if DEBUG
    ESP_LOGI("MODE", "%d", system_data_FS->sys_state);
#endif
    if (system_is_debug_mode(system_data_FS) == 1)
    {
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "WRONG MODE");
    }
    else
    {
        struct stat entry_stat;
        char filepath[FILE_PATH_MAX];
        int lastbuf = 0, bini = 0;
        uint8_t binw[256];
        /* Retrieve the base path of file storage to con  the full path */
        const char *filename = get_path_from_uri(filepath, system_data_FS->base_path,
                                                 req->uri + sizeof("/download/") - 1, sizeof(filepath));
        stat(filepath, &entry_stat);
        bini = entry_stat.st_size / 256;
        lastbuf = entry_stat.st_size % 256;
#if DEBUG
        ESP_LOGI(TAG, "%ld", entry_stat.st_size);
        ESP_LOGI(TAG, "%s", filename);
        ESP_LOGI(TAG, "%s", filepath);
        ESP_LOGI(TAG, "num buf : %d", bini);
#endif
        if (req->uri[9] == '2')
        {
            gpio_boot_mcu2();
        }
        else
        {
            gpio_boot_mcu1();
        }
        if (initSTM32())
        {
            FILE *fd = fopen(filepath, "r");
            if (fd)
            {
                for (int i = 0; i < bini; i++)
                {
                    fread(binw, 1, 256, fd);
                    stm32SendCommand(STM32WR);
                    if (stm32Address(STM32STADDR + (256 * i)))
                    {
#if DEBUG
                        if (stm32SendData(binw, 255))
                            ESP_LOGI(TAG, "data send success: %d", i);
                        else
                        {
                            ESP_LOGI(TAG, "data send failed : %d", i);
                        }
#else
                        stm32SendData(binw, 255);
                        // httpd_resp_send_500(req);
#endif
                    }
                    else
                    {
#if DEBUG
                        ESP_LOGI(TAG, "send address failed");
#endif
                        httpd_resp_send_500(req);
                    }
                }
                fread(binw, 1, lastbuf, fd);
                stm32SendCommand(STM32WR);
                if (stm32Address(STM32STADDR + (256 * bini)))
                {
#if DEBUG
                    if (stm32SendData(binw, lastbuf))
                        ESP_LOGI(TAG, "last buf send success");
                    else
                    {
                        ESP_LOGI(TAG, "last buf send failed");
                        httpd_resp_send_500(req);
                    }
#else
                    stm32SendData(binw, lastbuf);
                    // httpd_resp_send_500(req);
#endif
                }
                else
                {
#if DEBUG
                    ESP_LOGI(TAG, "send address for last failed");
#endif
                    httpd_resp_send_500(req);
                }
                fclose(fd);
            }
            else
            {
#if DEBUG
                ESP_LOGI(TAG, "file not found");
#endif
                httpd_resp_send_500(req);
            }
        }
        else
        {
#if DEBUG
            ESP_LOGI(TAG, "init stm32 failed");
#endif
            httpd_resp_send_500(req);
        }
        httpd_resp_sendstr(req, "Download done");
    }
    return ESP_OK;
}
static esp_err_t wifi_manager_handler(httpd_req_t *req)
{
    if (strcmp(req->uri, "/") == 0)
    {
        const size_t wifi_manager_size = (wifimanager_end - wifimanager_start);
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_status(req, "200 OK");
        httpd_resp_send(req, (const char *)wifimanager_start, wifi_manager_size);
    }
    else if (strcmp(req->uri, "getIpSsid"))
    {
#if DEBUG
        ESP_LOGI(TAG, "Current SSID: %s", (system_data_FS->ssid));
        ESP_LOGI(TAG, "Current IP  : %s", (system_data_FS->ip));
#endif
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr_chunk(req, system_data_FS->ssid);
        httpd_resp_sendstr_chunk(req, "?");
        httpd_resp_sendstr_chunk(req, system_data_FS->ip);
        httpd_resp_sendstr_chunk(req, NULL);
    }
    else
    {
        httpd_resp_send_404(req);
    }
    return ESP_OK;
}
static esp_err_t style_handler(httpd_req_t *req)
{
    const size_t style_size = (style_end - style_start);
    httpd_resp_set_type(req, "text/css");
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_send(req, (const char *)style_start, style_size);
    return ESP_OK;
}
static esp_err_t ota_manager_handler(httpd_req_t *req)
{
    const char *para = strchr(req->uri, '?');
    if (para)
    {
#if DEBUG
        ESP_LOGI(TAG, "RCV URI: %s", para);
#endif
    }
    if (!para)
    {
        const size_t ota_size = (ota_end - ota_start);
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_status(req, "200 OK");
        httpd_resp_send(req, (const char *)ota_start, ota_size);
        return ESP_OK;
    }
    else if (!strcmp(para, "?reload"))
    {
        system_set_sys_state(system_data_FS, MODE_CHANGE | MODE_BOOT);
        reload_json(req);
        return ESP_OK;
    }
    else
    {
        httpd_resp_set_type(req, "text/plain");
        if (strcmp(para, "?runboth") == 0)
        {
            gpio_run_both();
            system_update_mcu_state(system_data_FS, 1, 1);
            system_set_sys_state(system_data_FS, MODE_BOOT | MODE_CHANGE);
            httpd_resp_sendstr(req, "RUN BOTH");
#if DEBUG
            ESP_LOGI(TAG, "RUN BOTH");
#endif
        }
        else if (!strcmp(para, "?run1"))
        {
            gpio_run_mcu1();
            system_update_mcu_state(system_data_FS, 1, 0);
            system_set_sys_state(system_data_FS, MODE_BOOT | MODE_CHANGE);
            httpd_resp_sendstr(req, "RUN 1");
#if DEBUG
            ESP_LOGI(TAG, "Only RUN 1");
#endif
        }
        else if (strcmp(para, "?run2") == 0)
        {
            gpio_run_mcu2();
            system_update_mcu_state(system_data_FS, 0, 1);
            system_set_sys_state(system_data_FS, MODE_BOOT | MODE_CHANGE);
            httpd_resp_sendstr(req, "RUN 2");
#if DEBUG
            ESP_LOGI(TAG, "Only RUN 2");
#endif
        }
        else if (strcmp(para, "?erase") && para[1] != 'd')
        {
            if (system_is_debug_mode(system_data_FS) == 1)
            {
                httpd_resp_set_type(req, "text/plain");
                httpd_resp_sendstr(req, "WRONG MODE");
            }
            else
            {
                if (strcmp(para, "?erase1") == 0)
                {
                    gpio_boot_mcu1();
                }
                else
                {
                    gpio_boot_mcu2();
                }
#if DEBUG
                if (initSTM32() == 1)
                    ESP_LOGI(TAG, "INIT STM32 DONE");
                else
                    ESP_LOGI(TAG, "INIT STM32 FAILED");
#else
                initSTM32();
#endif

                if (stm32Erase() == 1)
                {
                    httpd_resp_sendstr(req, "ERASE 1");
#if DEBUG
                    ESP_LOGI(TAG, "ERASE DONE");
#endif
                }
                else
                {
#if DEBUG
                    ESP_LOGI(TAG, "ERASE FAILED");
#endif
                    httpd_resp_send_500(req);
                }
            }
        }

        // Debug mode
        else
        {
            system_set_sys_state(system_data_FS, MODE_DEBUG | MODE_CHANGE);
            if (strcmp(para, "?debug1") == 0)
            {
                system_update_mcu_state(system_data_FS, 1, 0);
                gpio_run_mcu1();
            }
            else if (strcmp(para, "?debug2") == 0)
            {
                system_update_mcu_state(system_data_FS, 0, 1);
                gpio_run_mcu2();
            }
            else
            {
                system_update_mcu_state(system_data_FS, 1, 1);
                gpio_run_both();
            }
            httpd_resp_sendstr(req, "DEBUG");
        }
    }
    return ESP_OK;
}
static esp_err_t about_handler(httpd_req_t *req)
{
    const size_t about_size = (about_end - about_start);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_send(req, (const char *)about_start, about_size);
    return ESP_OK;
}
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}
static esp_err_t http_resp_dir_html(httpd_req_t *req)
{
    const char *dirpath = "/data/";
    char entrypath[FILE_PATH_MAX];
    char entrysize[16];
    const char *entrytype;

    struct dirent *entry;
    struct stat entry_stat;

    DIR *dir = opendir(dirpath);
    const size_t dirpath_len = strlen(dirpath);

    /* Retrieve the base path of file storage to con  the full path */
    strlcpy(entrypath, dirpath, sizeof(entrypath));
#if DEBUG
    ESP_LOGI(TAG, "%s", entrypath);
#endif
    if (!dir)
    {
#if DEBUG
        ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath);
#endif
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        return ESP_FAIL;
    }
    const size_t list_size = (list_end - list_start);
    /* Add file upload form and script which on execution sends a POST request to /upload */
    httpd_resp_send_chunk(req, (const char *)list_start, list_size);

    /* Iterate over all files / folders and fetch their names and sizes */
    while ((entry = readdir(dir)) != NULL)
    {
        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
#if DEBUG
        ESP_LOGI(TAG, "%s", entrypath);
#endif
        if (stat(entrypath, &entry_stat) == -1)
        {
#if DEBUG
            ESP_LOGE(TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
#endif
            continue;
        }
        sprintf(entrysize, "%ld", entry_stat.st_size);
#if DEBUG
        ESP_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);
#endif
        /* Send chunk of HTML file containing table entries with file name and size */
        httpd_resp_sendstr_chunk(req, "<tr><td>");
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, entrysize);
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/delete/");
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "\"><input type=\"submit\" value =\"Delete\"></form>");
        httpd_resp_sendstr_chunk(req, "</td></tr>\n");
    }
    closedir(dir);

    /* Finish the file list table */
    httpd_resp_sendstr_chunk(req, "</tbody></table>");

    /* Send remaining chunk of HTML file to complete it */
    httpd_resp_sendstr_chunk(req, "</section></div></body></html>");

    /* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}
static const char *get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest)
    {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash)
    {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize)
    {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }
    /* Con  full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);
    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}
static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    /* Skip leading "/upload" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, system_data_FS->base_path,
                                             req->uri + sizeof("/upload") - 1, sizeof(filepath));
#if DEBUG
    ESP_LOGI(TAG, "file name: %s", filename);
    ESP_LOGI(TAG, "file path: %s", filepath);
#endif
    if (!filename)
    {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/')
    {
#if DEBUG
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
#endif
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == 0)
    {
#if DEBUG
        ESP_LOGE(TAG, "File already exists : %s", filepath);
#endif
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        return ESP_FAIL;
    }

    /* File cannot be larger than a limit */
    if (req->content_len > MAX_FILE_SIZE)
    {
#if DEBUG
        ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
#endif
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than " MAX_FILE_SIZE_STR "!");
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd)
    {
#if DEBUG
        ESP_LOGE(TAG, "Failed to create file : %s", filepath);
#endif
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }
#if DEBUG
    ESP_LOGI(TAG, "Receiving file : %s...", filename);
#endif
    /* Retrieve the pointer to scratch buffer for temporary storage */
    // char *buf = ((System_DataTypedef *)req->user_ctx)->scratch;
    char *buf = system_data_FS->scratch;
    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;

    while (remaining > 0)
    {
#if DEBUG
        ESP_LOGI(TAG, "Remaining size : %d", remaining);
#endif
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0)
        {
            if (received == HTTPD_SOCK_ERR_TIMEOUT)
            {
                /* Retry if timeout occurred */
                continue;
            }

            /* In case of unrecoverable error,
             * close and delete the unfinished file*/
            fclose(fd);
            unlink(filepath);
#if DEBUG
            ESP_LOGE(TAG, "File reception failed!");
#endif
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        /* Write buffer content to file on storage */
        if (received && (received != fwrite(buf, 1, received, fd)))
        {
            /* Couldn't write everything to file!
             * Storage may be full? */
            fclose(fd);
            unlink(filepath);
#if DEBUG
            ESP_LOGE(TAG, "File write failed!");
#endif
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    /* Close file upon upload completion */
    fclose(fd);
#if DEBUG
    ESP_LOGI(TAG, "File reception complete");
#endif
    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/list");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}
static esp_err_t delete_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    /* Skip leading "/delete" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, system_data_FS->base_path,
                                             req->uri + sizeof("/delete") - 1, sizeof(filepath));

    if (!filename)
    {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/')
    {
#if DEBUG
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
#endif
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1)
    {
#if DEBUG
        ESP_LOGE(TAG, "File does not exist : %s", filename);
#endif
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }
#if DEBUG
    ESP_LOGI(TAG, "Deleting file : %s", filename);
#endif
    /* Delete file */
    unlink(filepath);

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/list");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File deleted successfully");
    return ESP_OK;
}