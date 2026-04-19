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

QueueHandle_t wifi_queue;

static const char* TAG = "WIFI";
static app_wifi_config_full_t gCFG;
static int gRetryCount = 0;

static bool sta_running = false;
static bool ap_running = false;
static bool reconnecting = false;

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
    if (sta_running){ 
        return;
    }

    sta_running = true;

    wifi_config_t sta_cfg = {0};

    strncpy((char*)sta_cfg.sta.ssid, gCFG.sta.ssid, sizeof(sta_cfg.sta.ssid));
    strncpy((char*)sta_cfg.sta.password, gCFG.sta.password, sizeof(sta_cfg.sta.password));

    esp_wifi_set_mode(gCFG.ap.always_on ? WIFI_MODE_APSTA : WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    esp_wifi_start();
}

static void start_ap(void){
    if (ap_running){
        return;
    }
    ap_running = true;

    wifi_config_t ap_cfg = {0};

    strncpy((char*)ap_cfg.ap.ssid, gCFG.ap.ssid, sizeof(ap_cfg.ap.ssid));
    strncpy((char*)ap_cfg.ap.password, gCFG.ap.password, sizeof(ap_cfg.ap.password));
    if (strlen(gCFG.ap.password) == 0) {
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    } else {
        ap_cfg.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    }

    ap_cfg.ap.channel = gCFG.ap.channel;
    ap_cfg.ap.ssid_hidden = gCFG.ap.hidden;
    ap_cfg.ap.max_connection = AP_MAX_CONNECTIONS;

    ESP_LOGI(TAG, "AP started with credentials:");
    char visibility[16] = {0};
    if(gCFG.ap.hidden){
        sprintf(visibility, "Hidden");
    }
    else{
        sprintf(visibility, "Visible");
    }
    ESP_LOGI(TAG, "SSID: %s, Pass: %s", ap_cfg.ap.ssid, ap_cfg.ap.password);
    ESP_LOGI(TAG, "Visibility: %s, Channel: %d", visibility, ap_cfg.ap.channel);


    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();
}

void wifi_init(void){
    wifi_queue = xQueueCreate(10, sizeof(wifi_evt_t));

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
            case WIFI_EVENT_STA_START: {
                wifi_evt_t evt = {
                    .type = WIFI_EVT_STA_START
                };
                xQueueSend(wifi_queue, &evt, 0);
                ESP_LOGI(TAG, "STA start");
                break;
            }

            case WIFI_EVENT_STA_DISCONNECTED: {
                evt.type = WIFI_EVT_STA_DISCONNECTED;
                evt.reason = ((wifi_event_sta_disconnected_t*)event_data)->reason;
                xQueueSend(wifi_queue, &evt, 0);
                ESP_LOGI(TAG, "STA disconnected");
                break;
            }
            case WIFI_EVENT_AP_STACONNECTED:{
                ESP_LOGI(TAG, "AP connected");
                break;
            }
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
                    sta_running = false;

                    if (gRetryCount < STA_CONNECTION_RETRY_COUNT) {
                        ESP_LOGI(TAG, "retry STA %d", gRetryCount);
                        vTaskDelay(pdMS_TO_TICKS(200));
                        esp_wifi_connect();

                    } else {
                        wifi_evt_t ap_evt = {
                            .type = WIFI_EVT_AP_START
                        };
                        xQueueSend(wifi_queue, &ap_evt, 0);
                    }
                    break;
                case WIFI_EVT_AP_START:
                    start_ap();
                    http_server_start();
                    break;
                case WIFI_EVT_RECONFIGURE:
                    ESP_LOGI(TAG, "WIFI_EVT_RECONFIGURE RECEIVED");
                    reconnecting = true;
                    esp_wifi_stop();
                    gRetryCount = 0;
                    sta_running = false;
                    ap_running = false;
                    vTaskDelay(pdMS_TO_TICKS(500));

                    config_load(&gCFG);
                    if (gCFG.sta.enabled) {
                        start_sta();
                        esp_wifi_connect();
                    }
                    if (gCFG.ap.always_on || !gCFG.sta.enabled) {
                        wifi_evt_t ap_evt = {
                            .type = WIFI_EVT_AP_START
                        };
                        xQueueSend(wifi_queue, &ap_evt, 0);
                    }
                    reconnecting = false;
                    break;
                default:
                    break;
            }
        }
    }
}

void wifi_start(const app_wifi_config_full_t* cfg){
    memcpy(&gCFG, cfg, sizeof(gCFG));

    register_events();

    xTaskCreate(wifi_task, "wifi_task", 4096, NULL, 5, NULL);

    wifi_evt_t evt = {
        .type = WIFI_EVT_STA_START
    };

    xQueueSend(wifi_queue, &evt, 0);
}