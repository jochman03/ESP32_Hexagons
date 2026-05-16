#include "iot_client.h"

#include <stdio.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "app_version.h"
#include "iot_server_config.h"

static const char* TAG = "IOT_CLIENT";

#define IOT_CLIENT_QUEUE_LENGTH    10
#define IOT_CLIENT_TASK_STACK_SIZE 6144
#define IOT_CLIENT_TASK_PRIORITY   4

typedef enum {
    IOT_CLIENT_MSG_OTA_LOG = 0,
} iot_client_msg_type_t;

typedef struct {
    iot_client_msg_type_t type;
    iot_ota_log_event_t ota_log;
} iot_client_msg_t;

static QueueHandle_t iot_client_queue = NULL;

static void iot_client_task(void* arg);
static esp_err_t iot_client_post_ota_log(const iot_ota_log_event_t* event);
static void get_device_id(char* out, size_t out_size);
static void copy_string(char* dst, size_t dst_size, const char* src);

void iot_client_init(void) {
    if (iot_client_queue != NULL) {
        return;
    }

    iot_client_queue = xQueueCreate(IOT_CLIENT_QUEUE_LENGTH, sizeof(iot_client_msg_t));

    if (iot_client_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }

    BaseType_t task_ok = xTaskCreate(iot_client_task, "iot_client", IOT_CLIENT_TASK_STACK_SIZE,
                                     NULL, IOT_CLIENT_TASK_PRIORITY, NULL);

    if (task_ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        vQueueDelete(iot_client_queue);
        iot_client_queue = NULL;
    }
}

esp_err_t iot_client_log_ota_event(const iot_ota_log_event_t* event) {
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (iot_client_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    iot_client_msg_t msg = {0};

    msg.type = IOT_CLIENT_MSG_OTA_LOG;
    memcpy(&msg.ota_log, event, sizeof(iot_ota_log_event_t));

    if (xQueueSend(iot_client_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Queue full, dropping OTA event: %s", event->state);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t iot_client_log_ota(const char* state, const char* reason, const char* target_version,
                             int target_build, int progress, const char* message,
                             const char* error) {
    iot_ota_log_event_t event = {0};

    copy_string(event.state, sizeof(event.state), state);
    copy_string(event.reason, sizeof(event.reason), reason);
    copy_string(event.target_version, sizeof(event.target_version), target_version);
    copy_string(event.message, sizeof(event.message), message);
    copy_string(event.error, sizeof(event.error), error);

    event.target_build = target_build;
    event.progress = progress;
    event.uptime_ms = (uint32_t)(esp_timer_get_time() / 1000);

    return iot_client_log_ota_event(&event);
}

static void iot_client_task(void* arg) {
    iot_client_msg_t msg;

    while (1) {
        if (xQueueReceive(iot_client_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (msg.type) {
            case IOT_CLIENT_MSG_OTA_LOG: {
                esp_err_t err = iot_client_post_ota_log(&msg.ota_log);

                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to send OTA log '%s': %s", msg.ota_log.state,
                             esp_err_to_name(err));
                }

                break;
            }

            default:
                ESP_LOGW(TAG, "Unknown message type: %d", msg.type);
                break;
        }
    }
}

static esp_err_t iot_client_post_ota_log(const iot_ota_log_event_t* event) {
    char device_id[18];
    char json[1024];

    get_device_id(device_id, sizeof(device_id));

    int json_len =
        snprintf(json, sizeof(json),
                 "{"
                 "\"device_id\":\"%s\","
                 "\"project\":\"%s\","
                 "\"current_version\":\"%s\","
                 "\"current_build\":%d,"
                 "\"target_version\":\"%s\","
                 "\"target_build\":%d,"
                 "\"state\":\"%s\","
                 "\"reason\":\"%s\","
                 "\"progress\":%d,"
                 "\"flags\":%lu,"
                 "\"required_flags\":%lu,"
                 "\"missing_flags\":%lu,"
                 "\"missing_flags_text\":\"%s\","
                 "\"message\":\"%s\","
                 "\"error\":\"%s\","
                 "\"uptime_ms\":%lu"
                 "}",
                 device_id, APP_PROJECT_NAME, APP_VERSION, APP_BUILD, event->target_version,
                 event->target_build, event->state, event->reason, event->progress,
                 (unsigned long)event->flags, (unsigned long)event->required_flags,
                 (unsigned long)event->missing_flags, event->missing_flags_text, event->message,
                 event->error, (unsigned long)event->uptime_ms);

    if (json_len <= 0 || json_len >= sizeof(json)) {
        ESP_LOGE(TAG, "OTA log JSON buffer too small");
        return ESP_FAIL;
    }

    esp_http_client_config_t config = {
        .url = IOT_OTA_LOG_URL,
        .timeout_ms = 3000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (client == NULL) {
        return ESP_FAIL;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json, strlen(json));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);

        ESP_LOGI(TAG, "OTA log sent: state=%s status=%d", event->state, status);

        if (status < 200 || status >= 300) {
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGW(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);

    return err;
}

static void get_device_id(char* out, size_t out_size) {
    uint8_t mac[6];

    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    snprintf(out, out_size, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4],
             mac[5]);
}

static void copy_string(char* dst, size_t dst_size, const char* src) {
    if (dst == NULL || dst_size == 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    snprintf(dst, dst_size, "%s", src);
}