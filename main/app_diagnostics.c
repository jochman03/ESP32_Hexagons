#include <string.h>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "app_diagnostics.h"
#include "iot_client.h"
#include "app_version.h"

static const char* TAG = "APP_DIAG";

static EventGroupHandle_t app_diag_events = NULL;

static void app_diagnostics_ota_self_test_task(void* arg);
static void app_diagnostics_missing_flags_to_string(uint32_t missing_flags, char* out,
                                                    size_t out_size);
void app_diagnostics_init(void) {
    app_diag_events = xEventGroupCreate();

    if (app_diag_events == NULL) {
        ESP_LOGE(TAG, "Failed to create diagnostics event group");
    }
}

void app_diagnostics_set(uint32_t flag) {
    if (app_diag_events == NULL) {
        return;
    }

    xEventGroupSetBits(app_diag_events, flag);
}

void app_diagnostics_clear(uint32_t flag) {
    if (app_diag_events == NULL) {
        return;
    }

    xEventGroupClearBits(app_diag_events, flag);
}

uint32_t app_diagnostics_get_flags(void) {
    if (app_diag_events == NULL) {
        return 0;
    }

    return xEventGroupGetBits(app_diag_events);
}

bool app_diagnostics_is_healthy(void) {
    uint32_t flags = app_diagnostics_get_flags();

    return (flags & APP_DIAG_REQUIRED_MASK) == APP_DIAG_REQUIRED_MASK;
}

void app_diagnostics_start_ota_self_test(void) {
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
    ESP_LOGW(TAG, "Application is pending OTA verification, starting self-test");

    BaseType_t task_created =
        xTaskCreate(app_diagnostics_ota_self_test_task, "ota_self_test", 4096, NULL, 5, NULL);

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA self-test task");
    }
}

static void app_diagnostics_ota_self_test_task(void* arg) {
    ESP_LOGI(TAG, "OTA self-test started");
    iot_client_log_ota("self_test_started", "health_check_started", APP_VERSION, APP_BUILD, 100,
                       "Self-test started", "");
    vTaskDelay(pdMS_TO_TICKS(10000));

    uint32_t flags = app_diagnostics_get_flags();

    ESP_LOGI(TAG, "Diagnostics flags: 0x%lx", flags);
    ESP_LOGI(TAG, "Required flags:    0x%lx", (uint32_t)APP_DIAG_REQUIRED_MASK);

    if (app_diagnostics_is_healthy()) {
        ESP_LOGI(TAG, "OTA self-test passed, marking app valid");

        iot_client_log_ota("self_test_passed", "health_check_ok", APP_VERSION, APP_BUILD, 100,
                           "Self-test passed", "");

        vTaskDelay(pdMS_TO_TICKS(1000));

        esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to mark app valid: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "OTA self-test failed, rolling back");

        uint32_t required = APP_DIAG_REQUIRED_MASK;
        uint32_t missing = required & ~flags;

        char missing_text[128];

        app_diagnostics_missing_flags_to_string(missing, missing_text, sizeof(missing_text));

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

        esp_ota_mark_app_invalid_rollback_and_reboot();
    }

    vTaskDelete(NULL);
}
static void app_diagnostics_missing_flags_to_string(uint32_t missing_flags, char* out,
                                                    size_t out_size) {
    if (out == NULL || out_size == 0) {
        return;
    }

    out[0] = '\0';

    if (missing_flags & APP_DIAG_CONFIG_OK) {
        strncat(out, "CONFIG_OK,", out_size - strlen(out) - 1);
    }

    if (missing_flags & APP_DIAG_WIFI_OK) {
        strncat(out, "WIFI_OK,", out_size - strlen(out) - 1);
    }

    if (missing_flags & APP_DIAG_HTTP_OK) {
        strncat(out, "HTTP_OK,", out_size - strlen(out) - 1);
    }

    if (missing_flags & APP_DIAG_LED_OK) {
        strncat(out, "LED_OK,", out_size - strlen(out) - 1);
    }

    size_t len = strlen(out);

    if (len > 0 && out[len - 1] == ',') {
        out[len - 1] = '\0';
    }
}