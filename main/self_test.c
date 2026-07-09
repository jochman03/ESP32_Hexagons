#include <string.h>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "self_test.h"
#include "iot_client.h"
#include "app_version.h"
#include "config.h"

static const char* TAG = "SELF_TEST";

static EventGroupHandle_t self_test_events = NULL;

static void self_test_ota_task(void* arg);
static void flags_to_string(uint32_t missing_flags, char* out, size_t out_size);
void self_test_init(void) {
    self_test_events = xEventGroupCreate();

    if (self_test_events == NULL) {
        ESP_LOGE(TAG, "Failed to create diagnostics event group");
    }
}

void self_test_set_flags(uint32_t flag) {
    if (self_test_events == NULL) {
        return;
    }

    xEventGroupSetBits(self_test_events, flag);
}

void self_test_clear_flags(uint32_t flag) {
    if (self_test_events == NULL) {
        return;
    }

    xEventGroupClearBits(self_test_events, flag);
}

uint32_t self_test_get_flags(void) {
    if (self_test_events == NULL) {
        return 0;
    }

    return xEventGroupGetBits(self_test_events);
}

bool self_test_passed(void) {
    uint32_t flags = self_test_get_flags();

    return (flags & SELF_TEST_REQUIRED_MASK) == SELF_TEST_REQUIRED_MASK;
}

void self_test_start_ota(void) {
    const esp_partition_t* running_partition = esp_ota_get_running_partition();

    if (running_partition == NULL) {
        ESP_LOGW(TAG, "Running partition is NULL");
        return;
    }

    esp_ota_img_states_t ota_state;

    esp_err_t err = esp_ota_get_state_partition(running_partition, &ota_state);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get OTA state: %s", esp_err_to_name(err));
        return;
    }

    if (ota_state != ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "Application is not pending OTA verification");
        return;
    }

    iot_client_log_ota("booted_after_update", "pending_verify", APP_VERSION, APP_BUILD, 0,
                       "New firmware booted after OTA", "");
    ESP_LOGW(TAG, "Application is pending OTA verification, starting self test");

    BaseType_t task_created = xTaskCreate(self_test_ota_task, "ota_self_test", 4096, NULL, 5, NULL);

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
    }
}

static void self_test_ota_task(void* arg) {
    ESP_LOGI(TAG, "OTA self-test started");
    iot_client_log_ota("self_test_started", "self_test_started", APP_VERSION, APP_BUILD, 100,
                       "self test started", "");
    vTaskDelay(pdMS_TO_TICKS(10000));

    uint32_t flags = self_test_get_flags();

    ESP_LOGI(TAG, "Diagnostics flags: 0x%lx", flags);
    ESP_LOGI(TAG, "Required flags:    0x%lx", (uint32_t)SELF_TEST_REQUIRED_MASK);

    if (self_test_passed()) {
        ESP_LOGI(TAG, "OTA self-test passed, marking app valid");

        iot_client_log_ota("self_test_passed", "self_test_ok", APP_VERSION, APP_BUILD, 100,
                           "self test passed", "");

        vTaskDelay(pdMS_TO_TICKS(1000));

        esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to mark app valid: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "OTA self-test failed, rolling back");

        uint32_t required = SELF_TEST_REQUIRED_MASK;
        uint32_t missing = required & ~flags;

        char missing_text[128];

        flags_to_string(missing, missing_text, sizeof(missing_text));

        iot_ota_log_event_t failed_event = {0};

        snprintf(failed_event.state, sizeof(failed_event.state), "self_test_failed");
        snprintf(failed_event.reason, sizeof(failed_event.reason), "health_flags_missing");
        snprintf(failed_event.target_version, sizeof(failed_event.target_version), "%s",
                 APP_VERSION);

        failed_event.target_build = APP_BUILD;
        failed_event.progress = 100;
        failed_event.flags = flags;
        failed_event.required_flags = required;
        failed_event.missing_flags = missing;

        snprintf(failed_event.missing_flags_text, sizeof(failed_event.missing_flags_text), "%s",
                 missing_text);
        snprintf(failed_event.message, sizeof(failed_event.message), "Self-test failed");

        failed_event.uptime_ms = (uint32_t)(esp_timer_get_time() / 1000);

        iot_client_log_ota_event(&failed_event);

        iot_client_log_ota("rollback_requested", "self_test_failed", APP_VERSION, APP_BUILD, 100,
                           "Rollback requested", missing_text);

        vTaskDelay(pdMS_TO_TICKS(1500));

        maintenance_config_t cfg = {0};
        if (!config_load_maintenance(&cfg)) {
            config_get_default_maintenance(&cfg);
        }
        cfg.auto_update.enabled = false;
        if (config_update_maintenance(&cfg)) {
            ESP_LOGW(TAG, "Auto update disabled after rollback request");
        } else {
            ESP_LOGE(TAG, "Failed to disable auto update after rollback request");
        }

        esp_ota_mark_app_invalid_rollback_and_reboot();
    }

    vTaskDelete(NULL);
}
static void flags_to_string(uint32_t flags, char* out, size_t out_size) {
    if (out == NULL || out_size == 0) {
        return;
    }

    out[0] = '\0';

    if (flags & SELF_TEST_CONFIG_OK) {
        strncat(out, "CONFIG_OK,", out_size - strlen(out) - 1);
    }

    if (flags & SELF_TEST_WIFI_OK) {
        strncat(out, "WIFI_OK,", out_size - strlen(out) - 1);
    }

    if (flags & SELF_TEST_HTTP_OK) {
        strncat(out, "HTTP_OK,", out_size - strlen(out) - 1);
    }

    if (flags & SELF_TEST_LED_OK) {
        strncat(out, "LED_OK,", out_size - strlen(out) - 1);
    }

    size_t len = strlen(out);

    if (len > 0 && out[len - 1] == ',') {
        out[len - 1] = '\0';
    }
}