#include "maintenance.h"

#include "config.h"
#include "ota_update.h"
#include "wifi.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "MAINTENANCE";

#define MAINTENANCE_TASK_STACK_SIZE 4096
#define MAINTENANCE_TASK_PRIORITY   3

#define MAINTENANCE_TICK_MS            1000
#define MAINTENANCE_CONFIG_REFRESH_SEC 5
#define MAINTENANCE_START_DELAY_MS     10000

static TaskHandle_t maintenance_task_handle = NULL;

static void maintenance_task(void* arg);
static uint32_t interval_to_seconds(const interval_setting_t* setting);

void maintenance_start(void) {
    if (maintenance_task_handle != NULL) {
        return;
    }

    BaseType_t ok = xTaskCreate(maintenance_task, "maintenance", MAINTENANCE_TASK_STACK_SIZE, NULL,
                                MAINTENANCE_TASK_PRIORITY, &maintenance_task_handle);

    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create maintenance task");
        maintenance_task_handle = NULL;
    }
}

static uint32_t interval_to_seconds(const interval_setting_t* setting) {
    if (setting == NULL || !setting->enabled || setting->every == 0) {
        return 0;
    }

    switch (setting->unit) {
        case TIME_SEC:
            return setting->every;

        case TIME_MIN:
            return setting->every * 60;

        case TIME_HOUR:
            return setting->every * 3600;

        default:
            return 0;
    }
}

static void maintenance_task(void* arg) {
    maintenance_config_t cfg;

    config_get_default_maintenance(&cfg);
    config_load_maintenance(&cfg);

    uint32_t uptime_sec = 0;
    uint32_t last_config_load = 0;
    uint32_t last_wifi_retry = 0;
    uint32_t last_auto_update = 0;

    ESP_LOGI(TAG, "Maintenance task started");

    vTaskDelay(pdMS_TO_TICKS(MAINTENANCE_START_DELAY_MS));

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(MAINTENANCE_TICK_MS));

        uptime_sec++;

        if ((uptime_sec - last_config_load) >= MAINTENANCE_CONFIG_REFRESH_SEC) {
            if (!config_load_maintenance(&cfg)) {
                config_get_default_maintenance(&cfg);
            }

            last_config_load = uptime_sec;
        }

        uint32_t wifi_retry_sec = interval_to_seconds(&cfg.wifi_retry);
        uint32_t auto_update_sec = interval_to_seconds(&cfg.auto_update);

        if (wifi_retry_sec > 0 && !wifi_sta_is_connected()) {
            if ((uptime_sec - last_wifi_retry) >= wifi_retry_sec) {
                ESP_LOGI(TAG, "STA disconnected, trying reconnect");
                wifi_sta_reconnect();
                last_wifi_retry = uptime_sec;
            }
        }

        if (auto_update_sec > 0 && wifi_sta_is_connected()) {
            if ((uptime_sec - last_auto_update) >= auto_update_sec) {
                ota_status_t ota_status;

                ota_update_get_status(&ota_status);

                if (ota_status.update_running) {
                    ESP_LOGI(TAG, "Skipping auto update, OTA already running");
                    last_auto_update = uptime_sec;
                    continue;
                }

                ESP_LOGI(TAG, "Auto update: checking firmware");

                esp_err_t err = ota_update_check();

                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "Auto update: check failed: %s", esp_err_to_name(err));
                    last_auto_update = uptime_sec;
                    continue;
                }

                ota_update_get_status(&ota_status);

                if (!ota_status.update_available) {
                    ESP_LOGI(TAG, "Auto update: no update available");
                    last_auto_update = uptime_sec;
                    continue;
                }

                if (ota_status.update_running) {
                    ESP_LOGI(TAG, "Auto update: OTA already running after check");
                    last_auto_update = uptime_sec;
                    continue;
                }

                ESP_LOGI(TAG, "Auto update: update available, starting OTA");

                err = ota_update_start(OTA_TRIGGER_AUTO);

                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "Auto update: start failed: %s", esp_err_to_name(err));
                }

                last_auto_update = uptime_sec;
            }
        }
    }
}