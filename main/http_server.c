/*
 * http_server.c
 *
 *  Created on: 27 gru 2025
 *      Author: jakub
 */

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "http_parser.h"
#include "sys/param.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "http_server.h"
#include "freertos/idf_additions.h"
#include "portmacro.h"
#include "tasks_common.h"
#include "cJSON.h"
#include "hex.h"
#include "config.h"
#include "wifi.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "wifi.h"
#include "state_save.h"

// Tag used for ESP serial messages
static const char TAG[] = "http_server";

// Firmware update status
static int g_fw_update_status = OTA_UPDATE_PENDING;

// HTTP server task handle
static httpd_handle_t http_server_handle = NULL;

// HTTP server monitor task handle
static TaskHandle_t task_http_server_monitor = NULL;

// Queue handle used to manipulate the main queue of events
static QueueHandle_t http_server_monitor_queue_handle;

/*
    ESP32 timer configuration passed to esp_timer_create
*/
const esp_timer_create_args_t fw_update_reset_args = {.callback =
                                                          &http_server_fw_update_reset_callback,
                                                      .arg = NULL,
                                                      .dispatch_method = ESP_TIMER_TASK,
                                                      .name = "fw_update_reset"};
esp_timer_handle_t fw_update_reset;

// Embedded files: jQuery, index.html, script.js, app.css, favicon.ico, config.html,
// config_script.js files
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t script_js_start[] asm("_binary_script_js_start");
extern const uint8_t script_js_end[] asm("_binary_script_js_end");
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");
extern const uint8_t favicon_png_start[] asm("_binary_favicon_png_start");
extern const uint8_t favicon_png_end[] asm("_binary_favicon_png_end");
extern const uint8_t config_html_start[] asm("_binary_config_html_start");
extern const uint8_t config_html_end[] asm("_binary_config_html_end");
extern const uint8_t config_script_js_start[] asm("_binary_config_script_js_start");
extern const uint8_t config_script_js_end[] asm("_binary_config_script_js_end");

static void wifi_reconfigure_delayed_task(void* arg) {
    vTaskDelay(pdMS_TO_TICKS(700));
    wifi_reconfigure();
    vTaskDelete(NULL);
}

static void schedule_wifi_reconfigure(void) {
    xTaskCreate(wifi_reconfigure_delayed_task, "wifi_reconfig_delay", 3072, NULL, 5, NULL);
}

/*
    Checks the g_fw_update_status and creates the fw_update_reset timer if the g_fw_update_status is
   true
*/
static void http_server_fw_update_reset_timer(void) {
    if (g_fw_update_status == OTA_UPDATE_SUCCESSFUL) {
        ESP_LOGI(TAG, "http_server_fw_update_reset_timer: fw updated successful starting FW update "
                      "reset timer");

        // Give the web page a chance to receive an acknowledge back and initialize the timer
        ESP_ERROR_CHECK(esp_timer_create(&fw_update_reset_args, &fw_update_reset));
        ESP_ERROR_CHECK(esp_timer_start_once(fw_update_reset, 8000000));
    } else {
        ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW update unsuccessful");
    }
}

/*
    HTTP server monitor task used to track events of the HTTP server
    @param pvParameters parameter which can be passed to the task
*/
static void http_server_monitor(void* parameter) {
    http_server_queue_message_t msg;
    while (1) {
        if (xQueueReceive(http_server_monitor_queue_handle, &msg, portMAX_DELAY)) {
            switch (msg.msgID) {
                case HTTP_MSG_WIFI_CONNECT_INIT:
                    ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_INIT");
                    break;
                case HTTP_MSG_WIFI_CONNECT_SUCCESS:
                    ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_SUCCESS");
                    break;
                case HTTP_MSG_WIFI_FAIL:
                    ESP_LOGI(TAG, "HTTP_MSG_WIFI_FAIL");
                    break;
                case HTTP_MSG_OTA_UPDATE_SUCCESSFUL:
                    ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_SUCCESSFUL");
                    g_fw_update_status = OTA_UPDATE_SUCCESSFUL;
                    http_server_fw_update_reset_timer();
                    break;
                case HTTP_MSG_OTA_UPDATE_FAILED:
                    ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_FAILED");
                    g_fw_update_status = OTA_UPDATE_FAILED;
                    break;
                default:
                    ESP_LOGI(TAG, "Unidentified message");
                    break;
            }
        }
    }
}

/*
    Sends the index.html page
    @param req HTTP request for which the uri needs to be handled
    @return ESP_OK
*/
static esp_err_t http_server_index_html_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "index.html requested");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char*)index_html_start, index_html_end - index_html_start);

    return ESP_OK;
}

/*
    Sends the config.html page
    @param req HTTP request for which the uri needs to be handled
    @return ESP_OK
*/
static esp_err_t http_server_config_html_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "config.html requested");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char*)config_html_start, config_html_end - config_html_start);

    return ESP_OK;
}

/*
    style css get handler is requested when sccessing the web page
    @param req HTTP request for which the uri needs to be handled
    @return ESP_OK
*/
static esp_err_t http_server_style_css_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "style.css requested");
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char*)style_css_start, style_css_end - style_css_start);

    return ESP_OK;
}

/*
    script js get handler is requested when sccessing the web page
    @param req HTTP request for which the uri needs to be handled
    @return ESP_OK
*/
static esp_err_t http_server_script_js_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "script.js requested");
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char*)script_js_start, script_js_end - script_js_start);

    return ESP_OK;
}

/*
    config script js get handler is requested when sccessing the web page
    @param req HTTP request for which the uri needs to be handled
    @return ESP_OK
*/
static esp_err_t http_server_config_script_js_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "config_script.js requested");
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char*)config_script_js_start,
                    config_script_js_end - config_script_js_start);

    return ESP_OK;
}

/*
    favicon png get handler is requested when sccessing the web page
    @param req HTTP request for which the uri needs to be handled
    @return ESP_OK
*/
static esp_err_t http_server_favicon_png_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "favicon png requested");
    httpd_resp_set_type(req, "image/png");
    httpd_resp_send(req, (const char*)favicon_png_start, favicon_png_end - favicon_png_start);

    return ESP_OK;
}

/*
    Receiver the .bin file via the webpage and handles the firmware update
    @param req HTTP request for which the uri needs to be handled.
    @return ESP_OK, otherwise ESP_FAIL if timeout occurs and the update cannot be started.
*/
esp_err_t http_server_OTA_update_handler(httpd_req_t* req) {
    esp_ota_handle_t ota_handle;

    char ota_buff[1024];
    int content_length = req->content_len;
    int content_received = 0;
    int recv_len;
    bool is_req_body_started = false;
    bool flash_successful = false;

    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);

    do {
        // Read the data for the request
        if ((recv_len = httpd_req_recv(req, ota_buff, MIN(content_length, sizeof(ota_buff)))) < 0) {
            // Check if timeout occured
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                ESP_LOGI(TAG, "http_server_OTA_update_handler: Socket timeout");
                continue; // Retry receiving if timeout occured
            }
            ESP_LOGI(TAG, "http_server_OTA_update_handler: Ota other Error %d", recv_len);
            return ESP_FAIL;
        }
        printf("http_server_OTA_update_handler: OTA RX: %d of %d\r", content_received,
               content_length);

        // is this the first datra we are receiving
        // If so, it will have the information in the header that we need
        if (!is_req_body_started) {
            is_req_body_started = true;

            // Get the location of the .bin file content (remove the web form data)
            char* body_start_p = strstr(ota_buff, "\r\n\r\n") + 4;
            int body_part_len = recv_len - (body_start_p - ota_buff);

            printf("http_server_OTA_update_handler: OTA file size: %d\r\n", content_length);

            esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
            if (err != ESP_OK) {
                printf("http_server_OTA_update_handler: Error with OTA begin, cancelling OTA\n\r");
                return ESP_FAIL;
            } else {
                printf("http_server_OTA_update_handler: writing to partition subtype %d\r\n",
                       update_partition->subtype);
            }

            // Write this first part of the data
            esp_ota_write(ota_handle, body_start_p, body_part_len);
            content_received += body_part_len;
        } else {
            // Write OTA data
            esp_ota_write(ota_handle, ota_buff, recv_len);
            content_received += recv_len;
        }
    } while (recv_len > 0 && content_received < content_length);

    if (esp_ota_end(ota_handle) == ESP_OK) {
        // Update the partition
        if (esp_ota_set_boot_partition(update_partition) == ESP_OK) {
            const esp_partition_t* boot_partition = esp_ota_get_boot_partition();
            ESP_LOGI(
                TAG,
                "http_server_OTA_update_handler: Next boot partition subtype %d at offset 0x%x",
                boot_partition->subtype, boot_partition->address);
            flash_successful = true;
        } else {
            ESP_LOGI(TAG, "http_server_OTA_update_handler: Flashed error!!!");
        }
    } else {
        ESP_LOGI(TAG, "http_server_OTA_update_handler: esp_ota_end error!!!");
    }
    // We won't update the global variables throughout the file, so send the message about the
    // status
    if (flash_successful) {
        http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_SUCCESSFUL);
    } else {
        http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_FAILED);
    }
    return ESP_OK;
}

/*
    Ota status handler responds with the firmware update status after the OTA update is started
    and responds with the compile time/date when the page is first requested
    @param req HTTP request for which the uri needs to be handled
    @return ESP_OK
*/
esp_err_t http_server_OTA_status_handler(httpd_req_t* req) {
    char otaJSON[100];

    ESP_LOGI(TAG, "OTAstatus requested");

    sprintf(otaJSON, "{\"ota_update_status\":%d,\"compile_time\":\"%s\",\"compile_date\":\"%s\"}",
            g_fw_update_status, __TIME__, __DATE__);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, otaJSON, strlen(otaJSON));

    return ESP_OK;
}

esp_err_t http_server_set_speed_handler(httpd_req_t* req) {
    int total_len = req->content_len;
    int cur_len = 0;
    int received = 0;

    char* buf = malloc(total_len + 1);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (received <= 0) {
            free(buf);
            return ESP_FAIL;
        }
        cur_len += received;
    }

    buf[total_len] = '\0';
    ESP_LOGI(TAG, "Received JSON: %s", buf);

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON* value = cJSON_GetObjectItem(root, "value");
    if (!cJSON_IsString(value)) {
        cJSON_Delete(root);
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing value");
        return ESP_FAIL;
    }

    int speed = atoi(value->valuestring);
    if (speed > 49 && speed < 201) {
        hex_set_speed(speed);
    }
    return ESP_OK;
}

esp_err_t http_server_set_mode_handler(httpd_req_t* req) {
    int total_len = req->content_len;
    int cur_len = 0;
    int received = 0;

    char* buf = malloc(total_len + 1);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Received JSON: %s", buf);

    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (received <= 0) {
            free(buf);
            return ESP_FAIL;
        }
        cur_len += received;
    }

    buf[total_len] = '\0';

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON* value = cJSON_GetObjectItem(root, "value");
    if (!cJSON_IsString(value)) {
        cJSON_Delete(root);
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing value");
        return ESP_FAIL;
    }
    int mode = atoi(value->valuestring);

    switch (mode) {
        case 0:
            hex_set_mode(STATIC);
            break;
        case 1:
            hex_set_mode(FADE);
            break;
        case 2:
            hex_set_mode(STARLIGHT);
            break;
        default:
            hex_set_mode(STATIC);
            break;
    }

    cJSON_Delete(root);
    free(buf);
    httpd_resp_sendstr(req, "OK");

    return ESP_OK;
}

esp_err_t http_server_set_colors_handler(httpd_req_t* req) {
    char* buf = NULL;
    int total_len = req->content_len;
    int received = 0;
    int remaining = total_len;

    if (total_len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }

    buf = malloc(total_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
        return ESP_FAIL;
    }

    while (remaining > 0) {
        int ret = httpd_req_recv(req, buf + received, remaining);
        if (ret <= 0) {
            free(buf);
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }

    buf[total_len] = '\0';

    ESP_LOGI(TAG, "Received JSON: %s", buf);

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON* colors = cJSON_GetObjectItem(root, "colors");
    if (!cJSON_IsArray(colors)) {
        cJSON_Delete(root);
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No colors array");
        return ESP_FAIL;
    }

    int count = cJSON_GetArraySize(colors);

    for (int i = 0; i < count; i++) {
        cJSON* color = cJSON_GetArrayItem(colors, i);

        if (!cJSON_IsArray(color) || cJSON_GetArraySize(color) != 3)
            continue;

        uint8_t r = (uint8_t)cJSON_GetArrayItem(color, 0)->valueint;
        uint8_t g = (uint8_t)cJSON_GetArrayItem(color, 1)->valueint;
        uint8_t b = (uint8_t)cJSON_GetArrayItem(color, 2)->valueint;

        hex_set_color(i, r, g, b);
    }

    cJSON_Delete(root);
    free(buf);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t http_server_get_colors_handler(httpd_req_t* req) {
    cJSON* root = cJSON_CreateObject();
    cJSON* colors = cJSON_CreateArray();

    for (int i = 0; i < HEX_COUNT; i++) {
        cJSON* rgb = cJSON_CreateArray();
        cJSON_AddItemToArray(rgb, cJSON_CreateNumber(hex_get_color_r(i)));
        cJSON_AddItemToArray(rgb, cJSON_CreateNumber(hex_get_color_g(i)));
        cJSON_AddItemToArray(rgb, cJSON_CreateNumber(hex_get_color_b(i)));

        cJSON_AddItemToArray(colors, rgb);
    }

    cJSON_AddItemToObject(root, "colors", colors);

    char* json_string = cJSON_PrintUnformatted(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_string);

    cJSON_Delete(root);
    free(json_string);

    return ESP_OK;
}

esp_err_t http_server_get_status_handler(httpd_req_t* req) {
    int mode = (int)hex_get_mode();
    int speed = hex_get_speed();
    bool enabled = hex_is_enabled();

    char response[96];

    snprintf(response, sizeof(response), "{\"mode\":%d,\"speed\":%d,\"enabled\":%d}", mode, speed,
             enabled ? 1 : 0);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);

    return ESP_OK;
}

esp_err_t http_server_get_config_handler(httpd_req_t* req) {
    app_wifi_config_full_t cfg = {0};
    if (!config_load(&cfg)) {
        cfg = default_config();
    }

    cJSON* root = cJSON_CreateObject();

    // AP
    cJSON_AddNumberToObject(root, "ap_mode", cfg.ap.always_on ? 1 : 0);
    cJSON_AddStringToObject(root, "ap_ssid", cfg.ap.ssid);
    cJSON_AddStringToObject(root, "ap_pass", cfg.ap.password);
    cJSON_AddNumberToObject(root, "ap_channel", cfg.ap.channel);
    cJSON_AddNumberToObject(root, "ap_hidden", cfg.ap.hidden ? 1 : 0);

    // STA
    cJSON_AddStringToObject(root, "sta_ssid", cfg.sta.ssid);
    cJSON_AddStringToObject(root, "sta_pass", cfg.sta.password);

    cJSON_AddNumberToObject(root, "sta_dhcp", cfg.sta.dhcp ? 1 : 0);

    cJSON_AddStringToObject(root, "sta_ip", cfg.sta.ip);
    cJSON_AddStringToObject(root, "sta_mask", cfg.sta.netmask);
    cJSON_AddStringToObject(root, "sta_gateway", cfg.sta.gateway);

    cJSON_AddStringToObject(root, "sta_dns", cfg.sta.dns);

    char* json = cJSON_PrintUnformatted(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);

    cJSON_Delete(root);
    free(json);

    return ESP_OK;
}

esp_err_t http_server_save_ap_config_handler(httpd_req_t* req) {
    int total_len = req->content_len;
    int cur_len = 0;
    int received = 0;

    char* buf = malloc(total_len + 1);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (received <= 0) {
            free(buf);
            return ESP_FAIL;
        }
        cur_len += received;
    }

    buf[total_len] = '\0';
    ESP_LOGI(TAG, "Received JSON: %s", buf);

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON* ap_mode_json = cJSON_GetObjectItem(root, "ap_mode");
    cJSON* ap_ssid_json = cJSON_GetObjectItem(root, "ap_ssid");
    cJSON* ap_pass_json = cJSON_GetObjectItem(root, "ap_pass");
    cJSON* ap_channel_json = cJSON_GetObjectItem(root, "ap_channel");
    cJSON* ap_hidden_json = cJSON_GetObjectItem(root, "ap_hidden");

    if (!cJSON_IsString(ap_mode_json) || !cJSON_IsString(ap_ssid_json) ||
        !cJSON_IsString(ap_pass_json) || !cJSON_IsString(ap_channel_json) ||
        !cJSON_IsString(ap_hidden_json)) {
        cJSON_Delete(root);
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        ESP_LOGE(TAG, "AP config save failed - invalid JSON");
        return ESP_FAIL;
    }
    bool valid_creds = true;
    app_wifi_config_full_t cfg = {0};
    memset(&cfg, 0, sizeof(cfg));
    if (!config_load(&cfg)) {
        cfg = default_config();
    }

    int ap_mode = atoi(ap_mode_json->valuestring);
    if (ap_mode == 0 || ap_mode == 1) {
        cfg.ap.always_on = (bool)ap_mode;
    } else {
        valid_creds = false;
    }
    if (cJSON_IsString(ap_ssid_json) && ap_ssid_json->valuestring) {
        size_t len = strlen(ap_ssid_json->valuestring);
        if (len <= MAX_SSID_CHARACTERS) {
            snprintf(cfg.ap.ssid, sizeof(cfg.ap.ssid), "%s", ap_ssid_json->valuestring);
        } else {
            valid_creds = false;
        }
    } else {
        valid_creds = false;
    }
    if (cJSON_IsString(ap_pass_json) && ap_pass_json->valuestring) {
        size_t len = strlen(ap_pass_json->valuestring);
        if (len <= MAX_PASS_CHARACTERS) {
            snprintf(cfg.ap.password, sizeof(cfg.ap.password), "%s", ap_pass_json->valuestring);
        } else {
            valid_creds = false;
        }
    } else {
        valid_creds = false;
    }
    int ap_channel = atoi(ap_channel_json->valuestring);
    if (ap_channel >= 1 && ap_channel <= 11) {
        cfg.ap.channel = ap_channel;
    } else {
        valid_creds = false;
    }
    int ap_hidden = atoi(ap_hidden_json->valuestring);
    if (ap_hidden == 1 || ap_hidden == 0) {
        cfg.ap.hidden = (bool)ap_hidden;
    } else {
        valid_creds = false;
    }
    if (valid_creds) {
        if (!config_update(&cfg)) {
            ESP_LOGE(TAG, "CONFIG SAVE FAILED");
        } else {
            ESP_LOGI(TAG, "CONFIG SAVED OK");
        }
    } else {
        ESP_LOGE(TAG, "AP config save failed - invalid values");
        return ESP_FAIL;
    }

    cJSON_Delete(root);
    free(buf);

    ESP_LOGI(TAG, "AP config save successful");
    httpd_resp_sendstr(req, "OK");

    schedule_wifi_reconfigure();

    return ESP_OK;
}

esp_err_t http_server_save_sta_config_handler(httpd_req_t* req) {
    int total_len = req->content_len;
    int cur_len = 0;
    int received = 0;

    char* buf = malloc(total_len + 1);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (received <= 0) {
            free(buf);
            return ESP_FAIL;
        }

        cur_len += received;
    }

    buf[total_len] = '\0';

    ESP_LOGI(TAG, "Received STA JSON: %s", buf);

    cJSON* root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON* sta_ssid = cJSON_GetObjectItem(root, "sta_ssid");
    cJSON* sta_pass = cJSON_GetObjectItem(root, "sta_pass");
    cJSON* sta_dhcp = cJSON_GetObjectItem(root, "sta_dhcp");
    cJSON* sta_ip = cJSON_GetObjectItem(root, "sta_ip");
    cJSON* sta_mask = cJSON_GetObjectItem(root, "sta_mask");
    cJSON* sta_gateway = cJSON_GetObjectItem(root, "sta_gateway");
    cJSON* sta_dns = cJSON_GetObjectItem(root, "sta_dns");

    if (!cJSON_IsString(sta_ssid) || strlen(sta_ssid->valuestring) == 0) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing STA SSID");
        return ESP_FAIL;
    }

    const char* ssid = sta_ssid->valuestring;
    const char* pass = cJSON_IsString(sta_pass) ? sta_pass->valuestring : "";
    bool dhcp = cJSON_IsNumber(sta_dhcp)
                    ? sta_dhcp->valueint != 0
                    : (cJSON_IsString(sta_dhcp) && strcmp(sta_dhcp->valuestring, "1") == 0);

    const char* ip = cJSON_IsString(sta_ip) ? sta_ip->valuestring : "";
    const char* mask = cJSON_IsString(sta_mask) ? sta_mask->valuestring : "";
    const char* gateway = cJSON_IsString(sta_gateway) ? sta_gateway->valuestring : "";
    const char* dns = cJSON_IsString(sta_dns) ? sta_dns->valuestring : "";

    bool ok = config_update_sta(ssid, pass, dhcp, ip, mask, gateway, dns);

    cJSON_Delete(root);

    if (!ok) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Config save failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    schedule_wifi_reconfigure();

    return ESP_OK;
}

esp_err_t http_server_get_wifi_status_handler(httpd_req_t* req) {
    wifi_ap_record_t ap_info = {0};
    esp_netif_ip_info_t ip_info = {0};

    bool sta_connected = esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;

    esp_netif_t* sta_netif = wifi_get_sta_netif();
    if (sta_netif != NULL) {
        esp_netif_get_ip_info(sta_netif, &ip_info);
    }

    char ip[16] = "-";
    char gateway[16] = "-";

    if (sta_connected) {
        snprintf(ip, sizeof(ip), IPSTR, IP2STR(&ip_info.ip));
        snprintf(gateway, sizeof(gateway), IPSTR, IP2STR(&ip_info.gw));
    }

    cJSON* root = cJSON_CreateObject();
    if (root == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON_AddBoolToObject(root, "sta_connected", sta_connected);

    if (sta_connected) {
        cJSON_AddStringToObject(root, "sta_ssid", (const char*)ap_info.ssid);
        cJSON_AddStringToObject(root, "sta_ip", ip);
        cJSON_AddStringToObject(root, "sta_gateway", gateway);
        cJSON_AddNumberToObject(root, "sta_signal", ap_info.rssi);
    } else {
        cJSON_AddStringToObject(root, "sta_ssid", "-");
        cJSON_AddStringToObject(root, "sta_ip", "-");
        cJSON_AddStringToObject(root, "sta_gateway", "-");
        cJSON_AddStringToObject(root, "sta_signal", "-");
    }

    char* response = cJSON_PrintUnformatted(root);
    if (response == NULL) {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);

    free(response);
    cJSON_Delete(root);

    return ESP_OK;
}

esp_err_t http_server_disconnect_sta_handler(httpd_req_t* req) {
    bool ok = config_clear_sta();

    if (!ok) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Config save failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    schedule_wifi_reconfigure();

    return ESP_OK;
}

esp_err_t http_server_set_hex_enabled_handler(httpd_req_t* req) {
    int total_len = req->content_len;
    int cur_len = 0;
    int received = 0;

    char* buf = malloc(total_len + 1);
    if (buf == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);

        if (received <= 0) {
            free(buf);
            return ESP_FAIL;
        }

        cur_len += received;
    }

    buf[total_len] = '\0';

    cJSON* root = cJSON_Parse(buf);
    free(buf);

    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON* enabled_json = cJSON_GetObjectItem(root, "enabled");

    if (!cJSON_IsNumber(enabled_json) && !cJSON_IsBool(enabled_json)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing enabled");
        return ESP_FAIL;
    }

    bool enabled = false;

    if (cJSON_IsBool(enabled_json)) {
        enabled = cJSON_IsTrue(enabled_json);
    } else {
        enabled = enabled_json->valueint != 0;
    }

    cJSON_Delete(root);

    hex_set_enabled(enabled);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

/*
    Sets up the default httpd server configuration
    @return http server instance handle if successful, NULL otherwise
*/
static httpd_handle_t http_server_configure() {
    // Generate the default configuration
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Create the message queue
    http_server_monitor_queue_handle = xQueueCreate(3, sizeof(http_server_queue_message_t));

    // Create HTTP server monitor task
    xTaskCreatePinnedToCore(&http_server_monitor, "http_server_monitor",
                            HTTP_SERVER_MONITOR_STACK_SIZE, NULL, HTTP_SERVER_MONITOR_PRIORITY,
                            &task_http_server_monitor, HTTP_SERVER_MONITOR_CORE_ID);

    // The core that the HTTP server will run on
    config.core_id = HTTP_SERVER_TASK_CORE_ID;

    // Adjust the default priority to 1 less than the wifi application task
    config.task_priority = HTTP_SERVER_TASK_PRIORITY;

    // Bump up the stack size (default is 4096)
    config.stack_size = HTTP_SERVER_TASK_STACK_SIZE;

    // Increase uri handlers
    config.max_uri_handlers = 20;

    // Increase the timeout limits
    config.recv_wait_timeout = 10; // 10 s
    config.send_wait_timeout = 10;

    ESP_LOGI(TAG, "http_server_configure: Starting server on port: '%d' with task priority: '%d'",
             config.server_port, config.task_priority);

    // Start the httpd server
    if (httpd_start(&http_server_handle, &config) == ESP_OK) {
        ESP_LOGI(TAG, "http_server_configure: Registering URI handlers");

        // register index.html handler
        httpd_uri_t index_html = {.uri = "/",
                                  .method = HTTP_GET,
                                  .handler = http_server_index_html_handler,
                                  .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &index_html);

        // register style.css handler
        httpd_uri_t style_css = {.uri = "/style.css",
                                 .method = HTTP_GET,
                                 .handler = http_server_style_css_handler,
                                 .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &style_css);

        // register script.js handler
        httpd_uri_t script_js = {.uri = "/script.js",
                                 .method = HTTP_GET,
                                 .handler = http_server_script_js_handler,
                                 .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &script_js);

        // register favicon.png handler
        httpd_uri_t favicon_png = {.uri = "/favicon.png",
                                   .method = HTTP_GET,
                                   .handler = http_server_favicon_png_handler,
                                   .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &favicon_png);

        // register OTAupdate handler
        httpd_uri_t OTA_update = {.uri = "/OTAupdate",
                                  .method = HTTP_POST,
                                  .handler = http_server_OTA_update_handler,
                                  .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &OTA_update);

        // register OTAstatus handler
        httpd_uri_t OTA_status = {.uri = "/OTAstatus",
                                  .method = HTTP_POST,
                                  .handler = http_server_OTA_status_handler,
                                  .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &OTA_status);

        // register setColors handler
        httpd_uri_t setColors = {.uri = "/setColors",
                                 .method = HTTP_POST,
                                 .handler = http_server_set_colors_handler,
                                 .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &setColors);

        // register getColors.json handler
        httpd_uri_t getColors = {.uri = "/getColors.json",
                                 .method = HTTP_GET,
                                 .handler = http_server_get_colors_handler,
                                 .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &getColors);

        // register getStatus.json handler
        httpd_uri_t getStatus = {.uri = "/getStatus.json",
                                 .method = HTTP_GET,
                                 .handler = http_server_get_status_handler,
                                 .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &getStatus);

        // register setMode handler
        httpd_uri_t setMode = {.uri = "/setMode",
                               .method = HTTP_POST,
                               .handler = http_server_set_mode_handler,
                               .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &setMode);

        // register setSpeed handler
        httpd_uri_t setSpeed = {.uri = "/setSpeed",
                                .method = HTTP_POST,
                                .handler = http_server_set_speed_handler,
                                .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &setSpeed);

        // register config.html handler
        httpd_uri_t config_html = {.uri = "/config.html",
                                   .method = HTTP_GET,
                                   .handler = http_server_config_html_handler,
                                   .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &config_html);

        // register config_script.js handler
        httpd_uri_t config_script_js = {.uri = "/config_script.js",
                                        .method = HTTP_GET,
                                        .handler = http_server_config_script_js_handler,
                                        .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &config_script_js);

        // register saveAPConfig handler
        httpd_uri_t saveAPConfig = {.uri = "/saveAPConfig",
                                    .method = HTTP_POST,
                                    .handler = http_server_save_ap_config_handler,
                                    .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &saveAPConfig);

        // register getConfig.json handler
        httpd_uri_t getConfig = {.uri = "/getConfig.json",
                                 .method = HTTP_GET,
                                 .handler = http_server_get_config_handler,
                                 .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &getConfig);

        // register saveSTAConfig handler
        httpd_uri_t saveSTAConfig = {.uri = "/saveSTAConfig",
                                     .method = HTTP_POST,
                                     .handler = http_server_save_sta_config_handler,
                                     .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &saveSTAConfig);

        // register getWifiStatus.json handler
        httpd_uri_t getWifiStatus = {.uri = "/getWifiStatus.json",
                                     .method = HTTP_GET,
                                     .handler = http_server_get_wifi_status_handler,
                                     .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &getWifiStatus);

        // register disconnectSTA.json handler
        httpd_uri_t disconnectSTA = {.uri = "/disconnectSTA",
                                     .method = HTTP_POST,
                                     .handler = http_server_disconnect_sta_handler,
                                     .user_ctx = NULL};
        httpd_register_uri_handler(http_server_handle, &disconnectSTA);

        // register setHexEnabled handler
        httpd_uri_t setHexEnabled = {.uri = "/setHexEnabled",
                                     .method = HTTP_POST,
                                     .handler = http_server_set_hex_enabled_handler,
                                     .user_ctx = NULL};

        httpd_register_uri_handler(http_server_handle, &setHexEnabled);

        return http_server_handle;
    }
    return NULL;
}

void http_server_start(void) {
    if (http_server_handle == NULL) {
        http_server_handle = http_server_configure();
    }
}

void http_server_stop(void) {
    if (http_server_handle) {
        httpd_stop(http_server_handle);
        ESP_LOGI(TAG, "http_server_stop: stopping HTTP server");
        http_server_handle = NULL;
    }
    if (task_http_server_monitor) {
        vTaskDelete(task_http_server_monitor);
        ESP_LOGI(TAG, "http_server_stop: stopping HTTP server monitor");
        task_http_server_monitor = NULL;
    }
}

BaseType_t http_server_monitor_send_message(http_server_message_e msgID) {
    http_server_queue_message_t msg;
    msg.msgID = msgID;
    return xQueueSend(http_server_monitor_queue_handle, &msg, portMAX_DELAY);
}

void http_server_fw_update_reset_callback(void* arg) {
    ESP_LOGI(TAG, "http_server_fw_update_reset_callback: Timer timed-out, restarting the device");
    esp_restart();
}
