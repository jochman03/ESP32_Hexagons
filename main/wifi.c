#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "tasks_common.h"
#include "config.h"
#include "wifi.h"
#include "http_server.h"

static const char* TAG = "WIFI";

static QueueHandle_t wifi_queue = NULL;

static app_wifi_config_full_t gCFG = {0};
static int gRetryCount = 0; 
static bool gHttpStarted = false;
static bool gTaskStarted = false;

static esp_netif_t* sta_netif = NULL;
static esp_netif_t* ap_netif = NULL;

static void wifi_task(void* arg);
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                               void* event_data);

static void register_events(void);
static void configure_sta(void);
static void configure_ap(void);
static void start_wifi_from_config(void);
static void start_fallback_ap(void);
static void start_http_once(void);
static void configure_sta_ip(void);

static void start_http_once(void) {
    if (!gHttpStarted) {
        http_server_start();
        gHttpStarted = true;
    }
}

static bool sta_configured(void) {
    return strlen(gCFG.sta.ssid) > 0;
}

static void register_events(void) {
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, NULL));
}

static void configure_sta(void) {
    if (!sta_configured()) {
        ESP_LOGW(TAG, "STA not configured");
        return;
    }
    wifi_config_t sta_cfg = {0};

    strncpy((char*)sta_cfg.sta.ssid, gCFG.sta.ssid, sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char*)sta_cfg.sta.password, gCFG.sta.password, sizeof(sta_cfg.sta.password) - 1);

    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));

    ESP_LOGI(TAG, "STA config:");
    ESP_LOGI(TAG, "SSID: %s", gCFG.sta.ssid);
}

static void configure_ap(void) {
    wifi_config_t ap_cfg = {0};

    strncpy((char*)ap_cfg.ap.ssid, gCFG.ap.ssid, sizeof(ap_cfg.ap.ssid) - 1);
    strncpy((char*)ap_cfg.ap.password, gCFG.ap.password, sizeof(ap_cfg.ap.password) - 1);

    ap_cfg.ap.ssid_len = strlen(gCFG.ap.ssid);
    ap_cfg.ap.channel = gCFG.ap.channel;
    ap_cfg.ap.ssid_hidden = gCFG.ap.hidden;
    ap_cfg.ap.max_connection = AP_MAX_CONNECTIONS;

    if (strlen(gCFG.ap.password) == 0) {
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    } else {
        ap_cfg.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));

    ESP_LOGI(TAG, "AP config:");
    ESP_LOGI(TAG, "SSID: %s", gCFG.ap.ssid);
    ESP_LOGI(TAG, "PASS: %s", gCFG.ap.password);
    ESP_LOGI(TAG, "CHANNEL: %d", gCFG.ap.channel);
    ESP_LOGI(TAG, "HIDDEN: %d", gCFG.ap.hidden);
}

static void start_wifi_from_config(void) {
    gRetryCount = 0;

    if (!sta_configured()) {
        ESP_LOGW(TAG, "STA not configured, starting configuration AP");

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        configure_ap();
        ESP_ERROR_CHECK(esp_wifi_start());

        return;
    }

    if (gCFG.ap.enabled && gCFG.ap.always_on) {
        ESP_LOGI(TAG, "Starting WiFi APSTA");

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

        configure_sta();
        configure_sta_ip();
        configure_ap();

        ESP_ERROR_CHECK(esp_wifi_start());

        return;
    }

    ESP_LOGI(TAG, "Starting WiFi STA");

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    configure_sta();
    configure_sta_ip();

    ESP_ERROR_CHECK(esp_wifi_start());
}

static void start_fallback_ap(void) {
    ESP_LOGW(TAG, "Starting fallback AP");

    ESP_ERROR_CHECK(esp_wifi_stop());
    vTaskDelay(pdMS_TO_TICKS(300));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    configure_ap();

    ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_init(void) {
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }

    wifi_queue = xQueueCreate(10, sizeof(wifi_evt_t));
    if (wifi_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create wifi_queue");
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    sta_netif = esp_netif_create_default_wifi_sta();
    ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    register_events();
}

void wifi_start(const app_wifi_config_full_t* cfg) {
    if (cfg == NULL) {
        ESP_LOGE(TAG, "WiFi config is NULL");
        return;
    }

    memcpy(&gCFG, cfg, sizeof(gCFG));

    if (!gTaskStarted) {
        xTaskCreate(wifi_task, "wifi_task", WIFI_APP_TASK_STACK_SIZE, NULL, WIFI_APP_TASK_PRIORITY,
                    NULL);

        gTaskStarted = true;
    }

    start_wifi_from_config();
}

void wifi_reconfigure(void) {
    wifi_evt_t evt = {.type = WIFI_EVT_RECONFIGURE};

    if (wifi_queue == NULL) {
        return;
    }

    if (xQueueSend(wifi_queue, &evt, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send WIFI_EVT_RECONFIGURE");
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                               void* event_data) {
    wifi_evt_t evt = {0};

    if (wifi_queue == NULL) {
        return;
    }

    if (event_base == WIFI_EVENT) {
        switch (event_id) {

            case WIFI_EVENT_STA_START:
                evt.type = WIFI_EVT_STA_STARTED;
                xQueueSend(wifi_queue, &evt, 0);
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                evt.type = WIFI_EVT_STA_DISCONNECTED;
                evt.reason = ((wifi_event_sta_disconnected_t*)event_data)->reason;
                xQueueSend(wifi_queue, &evt, 0);
                break;

            case WIFI_EVENT_AP_START:
                evt.type = WIFI_EVT_AP_STARTED;
                xQueueSend(wifi_queue, &evt, 0);
                break;

            case WIFI_EVENT_AP_STACONNECTED:
                ESP_LOGI(TAG, "Client connected to AP");
                break;

            case WIFI_EVENT_AP_STADISCONNECTED:
                ESP_LOGI(TAG, "Client disconnected from AP");
                break;

            default:
                break;
        }
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        evt.type = WIFI_EVT_STA_CONNECTED;
        xQueueSend(wifi_queue, &evt, 0);
    }
}

static void wifi_task(void* arg) {
    wifi_evt_t evt;

    while (1) {
        if (xQueueReceive(wifi_queue, &evt, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (evt.type) {

            case WIFI_EVT_STA_STARTED:
                ESP_LOGI(TAG, "STA started, connecting...");
                esp_wifi_connect();
                break;

            case WIFI_EVT_STA_CONNECTED:
                ESP_LOGI(TAG, "STA connected");
                gRetryCount = 0;
                start_http_once();
                break;

            case WIFI_EVT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "STA disconnected, reason=%d", evt.reason);

                if (gCFG.ap.always_on) {
                    ESP_LOGI(TAG, "AP always on, retrying STA in APSTA mode");
                }

                if (gRetryCount < STA_CONNECTION_RETRY_COUNT) {
                    gRetryCount++;

                    ESP_LOGI(TAG, "Retry STA connection: %d/%d", gRetryCount,
                             STA_CONNECTION_RETRY_COUNT);

                    vTaskDelay(pdMS_TO_TICKS(500));
                    esp_wifi_connect();
                } else {
                    ESP_LOGW(TAG, "STA connection failed");

                    if (gCFG.ap.enabled && !gCFG.ap.always_on) {
                        start_fallback_ap();
                    }
                }
                break;

            case WIFI_EVT_AP_STARTED:
                ESP_LOGI(TAG, "AP started");
                start_http_once();
                break;

            case WIFI_EVT_RECONFIGURE:
                ESP_LOGI(TAG, "Reconfiguring WiFi");

                ESP_ERROR_CHECK(esp_wifi_stop());

                gRetryCount = 0;

                if (!config_load(&gCFG)) {
                    ESP_LOGW(TAG, "Config load failed, using defaults");
                    gCFG = default_config();
                }

                vTaskDelay(pdMS_TO_TICKS(300));

                start_wifi_from_config();
                break;

            default:
                break;
        }
    }
}

esp_netif_t* wifi_get_sta_netif(void) {
    return sta_netif;
}

static void configure_sta_ip(void) {
    if (sta_netif == NULL) {
        ESP_LOGE(TAG, "STA netif is NULL");
        return;
    }

    if (gCFG.sta.dhcp) {
        ESP_LOGI(TAG, "STA DHCP enabled");

        esp_err_t err = esp_netif_dhcpc_start(sta_netif);
        if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
            ESP_LOGW(TAG, "Failed to start DHCP client: %s", esp_err_to_name(err));
        }

        return;
    }

    ESP_LOGI(TAG, "STA static IP enabled");

    esp_netif_ip_info_t ip_info = {0};

    if (inet_pton(AF_INET, gCFG.sta.ip, &ip_info.ip) != 1 ||
        inet_pton(AF_INET, gCFG.sta.gateway, &ip_info.gw) != 1 ||
        inet_pton(AF_INET, gCFG.sta.netmask, &ip_info.netmask) != 1) {

        ESP_LOGE(TAG, "Invalid static IP config, falling back to DHCP");

        esp_err_t err = esp_netif_dhcpc_start(sta_netif);
        if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
            ESP_LOGW(TAG, "Failed to start DHCP client: %s", esp_err_to_name(err));
        }

        return;
    }

    esp_err_t err = esp_netif_dhcpc_stop(sta_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGW(TAG, "Failed to stop DHCP client: %s", esp_err_to_name(err));
    }

    ESP_ERROR_CHECK(esp_netif_set_ip_info(sta_netif, &ip_info));

    if (strlen(gCFG.sta.dns) > 0) {
        esp_netif_dns_info_t dns_info = {0};

        if (inet_pton(AF_INET, gCFG.sta.dns, &dns_info.ip.u_addr.ip4) == 1) {
            dns_info.ip.type = IPADDR_TYPE_V4;
            ESP_ERROR_CHECK(esp_netif_set_dns_info(sta_netif, ESP_NETIF_DNS_MAIN, &dns_info));
        } else {
            ESP_LOGW(TAG, "Invalid DNS address: %s", gCFG.sta.dns);
        }
    }

    ESP_LOGI(TAG, "Static IP: %s", gCFG.sta.ip);
    ESP_LOGI(TAG, "Gateway: %s", gCFG.sta.gateway);
    ESP_LOGI(TAG, "Netmask: %s", gCFG.sta.netmask);
    ESP_LOGI(TAG, "DNS: %s", gCFG.sta.dns);
}