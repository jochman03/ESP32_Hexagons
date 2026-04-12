#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "tasks_common.h"
#include "config.h"
#include "wifi.h"
#include "http_server.h"

static const char* TAG = "WIFI";
static const app_wifi_config_full_t *gCFG = NULL;
static int gRetryCount = 0;

typedef enum {
    WIFI_EVT_STA_START,
    WIFI_EVT_STA_CONNECTED,
    WIFI_EVT_STA_DISCONNECTED,
    WIFI_EVT_STA_FAIL,
    WIFI_EVT_STA_TIMEOUT,
    WIFI_EVT_AP_START
} wifi_evt_type_t;

typedef struct {
    wifi_evt_type_t type;
    int reason;
} wifi_evt_t;

static QueueHandle_t wifi_queue;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
     int32_t event_id, void* event_data);
static void register_events(void);

static void start_sta(void);
static void start_ap(void);

static void register_events(void){
    esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);
}

static void start_sta(void){
    wifi_config_t sta_cfg = {0};

    strcpy((char*)sta_cfg.sta.ssid, gCFG->sta.ssid);
    strcpy((char*)sta_cfg.sta.password, gCFG->sta.password);

    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_mode(gCFG->ap.always_on ? WIFI_MODE_APSTA : WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    esp_wifi_start();
}

static void start_ap(void){
    wifi_config_t ap_cfg = {0};

    strcpy((char*)ap_cfg.ap.ssid, gCFG->ap.ssid);
    strcpy((char*)ap_cfg.ap.password, gCFG->ap.password);

    ap_cfg.ap.ssid_len = strlen(gCFG->ap.ssid);
    ap_cfg.ap.channel = gCFG->ap.channel;
    ap_cfg.ap.max_connection = 5;
    ap_cfg.ap.ssid_hidden = gCFG->ap.hidden;

    ap_cfg.ap.authmode =
        strlen(gCFG->ap.password) == 0
        ? WIFI_AUTH_OPEN
        : WIFI_AUTH_WPA_WPA2_PSK;

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();
}

void wifi_init(void){
    wifi_queue = xQueueCreate(10, sizeof(wifi_evt_t));

    nvs_flash_init();

    esp_netif_init();
    esp_event_loop_create_default();

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

}
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
     int32_t event_id, void* event_data){
    wifi_evt_t evt = {0};

    if (event_base == WIFI_EVENT) {

        switch (event_id) {

        case WIFI_EVENT_STA_START:
            evt.type = WIFI_EVT_STA_START;
            xQueueSend(wifi_queue, &evt, 0);
            ESP_LOGI(TAG, "STA start");
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            evt.type = WIFI_EVT_STA_DISCONNECTED;
            evt.reason = ((wifi_event_sta_disconnected_t*)event_data)->reason;
            xQueueSend(wifi_queue, &evt, 0);
            ESP_LOGI(TAG, "STA disconnected");
            break;

        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "AP connected");
            break;
        }

    } else if (event_base == IP_EVENT &&
               event_id == IP_EVENT_STA_GOT_IP) {

        evt.type = WIFI_EVT_STA_CONNECTED;
        xQueueSend(wifi_queue, &evt, 0);
    }
}

static void wifi_task(void* arg){
    wifi_evt_t evt;

    while (1) {

        if (xQueueReceive(wifi_queue, &evt, portMAX_DELAY)) {

            switch (evt.type) {

            case WIFI_EVT_STA_START:
                gRetryCount = 0;
                start_sta();
                esp_wifi_connect();
                break;

            case WIFI_EVT_STA_CONNECTED:
                ESP_LOGI(TAG, "CONNECTED");
                http_server_start();
                break;

            case WIFI_EVT_STA_DISCONNECTED:

                gRetryCount++;

                if (gRetryCount < STA_CONNECTION_RETRY_COUNT) {
                    start_sta();
                    esp_wifi_connect();
                } else {
                    if (gCFG->ap.enabled) {
                        wifi_evt_t ap_evt = {
                            .type = WIFI_EVT_AP_START
                        };
                        xQueueSend(wifi_queue, &ap_evt, 0);
                    }
                }
                break;

            case WIFI_EVT_AP_START:
                start_ap();
                http_server_start();
                break;

            default:
                break;
            }
        }
    }
}

void wifi_start(const app_wifi_config_full_t* cfg){
    gCFG = cfg;

    register_events();

    xTaskCreate(wifi_task, "wifi_task", 4096, NULL, 5, NULL);

    wifi_evt_t evt = {
        .type = WIFI_EVT_STA_START
    };

    xQueueSend(wifi_queue, &evt, 0);
}