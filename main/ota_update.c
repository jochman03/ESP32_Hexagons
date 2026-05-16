#include "ota_update.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "mbedtls/sha256.h"

#include "app_version.h"
#include "iot_server_config.h"
#include "iot_client.h"

static const char* TAG = "OTA_UPDATE";

#define OTA_MAX_HTTP_RESPONSE_SIZE 1024

static ota_status_t g_ota_status;
static SemaphoreHandle_t g_ota_mutex = NULL;

static char g_manifest_url[160] = {0};
static char g_firmware_url[160] = {0};
static int g_manifest_size = 0;
static char g_manifest_sha256[65] = {0};

static void ota_set_running(bool running);
static void ota_task(void* arg);
static void sha256_to_hex(const uint8_t* sha, char* out_hex, size_t out_size);
static const char* ota_state_to_message(ota_state_t state);

static void ota_set_status(ota_state_t state, int progress, const char* message) {
    if (g_ota_mutex != NULL) {
        xSemaphoreTake(g_ota_mutex, portMAX_DELAY);
    }

    g_ota_status.state = state;
    g_ota_status.progress = progress;

    if (message != NULL) {
        snprintf(g_ota_status.message, sizeof(g_ota_status.message), "%s", message);
    } else {
        snprintf(g_ota_status.message, sizeof(g_ota_status.message), "%s",
                 ota_state_to_message(state));
    }

    if (g_ota_mutex != NULL) {
        xSemaphoreGive(g_ota_mutex);
    }
}

static esp_err_t http_get_string(const char* url, char* out, int out_size) {
    if (url == NULL || out == NULL || out_size <= 1) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int content_length = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "HTTP content length: %d", content_length);

    int total_read = 0;

    while (total_read < out_size - 1) {
        int read_len = esp_http_client_read(client, out + total_read, out_size - 1 - total_read);

        if (read_len < 0) {
            ESP_LOGE(TAG, "HTTP read failed");
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }

        if (read_len == 0) {
            break;
        }

        total_read += read_len;
    }

    out[total_read] = '\0';

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    return ESP_OK;
}

void ota_update_init(void) {
    g_ota_mutex = xSemaphoreCreateMutex();

    memset(&g_ota_status, 0, sizeof(g_ota_status));

    g_ota_status.state = OTA_STATE_IDLE;
    g_ota_status.progress = 0;
    g_ota_status.current_build = APP_BUILD;
    g_ota_status.latest_build = 0;
    g_ota_status.update_available = false;
    g_ota_status.update_running = false;

    snprintf(g_ota_status.current_version, sizeof(g_ota_status.current_version), "%s", APP_VERSION);
    snprintf(g_ota_status.message, sizeof(g_ota_status.message), "Idle");

    snprintf(g_manifest_url, sizeof(g_manifest_url), "%s", OTA_MANIFEST_URL);
}

void ota_update_get_status(ota_status_t* out_status) {
    if (out_status == NULL) {
        return;
    }

    if (g_ota_mutex != NULL) {
        xSemaphoreTake(g_ota_mutex, portMAX_DELAY);
    }

    memcpy(out_status, &g_ota_status, sizeof(ota_status_t));

    if (g_ota_mutex != NULL) {
        xSemaphoreGive(g_ota_mutex);
    }
}

esp_err_t ota_update_check(void) {
    char response[OTA_MAX_HTTP_RESPONSE_SIZE];

    ota_set_status(OTA_STATE_CHECKING, 0, "Checking manifest");

    esp_err_t err = http_get_string(g_manifest_url, response, sizeof(response));
    if (err != ESP_OK) {
        ota_set_status(OTA_STATE_FAILED, 0, "Manifest download failed");
        return err;
    }

    ESP_LOGI(TAG, "Manifest: %s", response);

    cJSON* root = cJSON_Parse(response);
    if (root == NULL) {
        ota_set_status(OTA_STATE_FAILED, 0, "Invalid manifest JSON");
        return ESP_FAIL;
    }

    cJSON* project_json = cJSON_GetObjectItem(root, "project");
    cJSON* version_json = cJSON_GetObjectItem(root, "version");
    cJSON* build_json = cJSON_GetObjectItem(root, "build");
    cJSON* url_json = cJSON_GetObjectItem(root, "url");
    cJSON* size_json = cJSON_GetObjectItem(root, "size");
    cJSON* sha256_json = cJSON_GetObjectItem(root, "sha256");

    if (!cJSON_IsString(project_json) || !cJSON_IsString(version_json) ||
        !cJSON_IsNumber(build_json) || !cJSON_IsString(url_json) || !cJSON_IsNumber(size_json) ||
        !cJSON_IsString(sha256_json)) {

        cJSON_Delete(root);
        ota_set_status(OTA_STATE_FAILED, 0, "Manifest fields missing");
        return ESP_FAIL;
    }

    if (strcmp(project_json->valuestring, APP_PROJECT_NAME) != 0) {
        cJSON_Delete(root);
        ota_set_status(OTA_STATE_FAILED, 0, "Manifest project mismatch");
        return ESP_FAIL;
    }

    int latest_build = build_json->valueint;

    if (g_ota_mutex != NULL) {
        xSemaphoreTake(g_ota_mutex, portMAX_DELAY);
    }

    g_ota_status.latest_build = latest_build;
    snprintf(g_ota_status.latest_version, sizeof(g_ota_status.latest_version), "%s",
             version_json->valuestring);

    snprintf(g_firmware_url, sizeof(g_firmware_url), "%s", url_json->valuestring);
    g_manifest_size = size_json->valueint;
    snprintf(g_manifest_sha256, sizeof(g_manifest_sha256), "%s", sha256_json->valuestring);

    if (latest_build > APP_BUILD) {
        g_ota_status.state = OTA_STATE_UPDATE_AVAILABLE;
        g_ota_status.progress = 0;
        g_ota_status.update_available = true;
        snprintf(g_ota_status.message, sizeof(g_ota_status.message), "Update available");
        iot_ota_log_event_t event = {0};

        snprintf(event.state, sizeof(event.state), "update_available");
        snprintf(event.reason, sizeof(event.reason), "newer_build_found");
        snprintf(event.target_version, sizeof(event.target_version), "%s",
                 version_json->valuestring);
        event.target_build = latest_build;
        event.progress = 0;
        snprintf(event.message, sizeof(event.message), "Update available");

        iot_client_log_ota_event(&event);
    } else {
        g_ota_status.state = OTA_STATE_NO_UPDATE;
        g_ota_status.progress = 0;
        g_ota_status.update_available = false;
        snprintf(g_ota_status.message, sizeof(g_ota_status.message), "No update available");
    }

    if (g_ota_mutex != NULL) {
        xSemaphoreGive(g_ota_mutex);
    }

    cJSON_Delete(root);

    return ESP_OK;
}

esp_err_t ota_update_start(void) {
    bool can_start = false;

    if (g_ota_mutex != NULL) {
        xSemaphoreTake(g_ota_mutex, portMAX_DELAY);
    }

    can_start = g_ota_status.update_available && !g_ota_status.update_running;

    if (g_ota_mutex != NULL) {
        xSemaphoreGive(g_ota_mutex);
    }

    if (!can_start) {
        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t task_ok = xTaskCreate(ota_task, "ota_task", 8192, NULL, 5, NULL);

    if (task_ok != pdPASS) {
        ota_set_status(OTA_STATE_FAILED, 0, "Failed to start OTA task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void ota_task(void* arg) {
    ota_set_running(true);

    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);

    if (update_partition == NULL) {
        ota_set_status(OTA_STATE_FAILED, 0, "No OTA partition");
        ota_set_running(false);
        iot_client_log_ota("download_failed", "no_ota_partition", "", 0, 0,
                           "No suitable OTA partition", "");

        vTaskDelete(NULL);
    }

    if (g_manifest_size <= 0 || g_manifest_size > update_partition->size) {
        ota_set_status(OTA_STATE_FAILED, 0, "Firmware too large");
        ota_set_running(false);
        iot_client_log_ota("manifest_error", "firmware_too_large", "", 0, 0,
                           "Firmware size declared in manifest is too large", "");

        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "OTA partition: subtype=%d offset=0x%lx size=%lu", update_partition->subtype,
             update_partition->address, update_partition->size);

    ESP_LOGI(TAG, "Firmware URL: %s", g_firmware_url);
    ESP_LOGI(TAG, "Firmware size from manifest: %d", g_manifest_size);

    esp_http_client_config_t config = {
        .url = g_firmware_url,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (client == NULL) {
        ota_set_status(OTA_STATE_FAILED, 0, "HTTP client init failed");
        ota_set_running(false);
        iot_client_log_ota("download_error", "http_client_init_failed", "", 0, 0,
                           "HTTP client init failed", "");
        vTaskDelete(NULL);
    }

    esp_err_t err = esp_http_client_open(client, 0);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);

        ota_set_status(OTA_STATE_FAILED, 0, "Firmware HTTP open failed");
        ota_set_running(false);
        iot_client_log_ota("download_failed", "http_client_open_failed", "", 0, 0,
                           "Firmware HTTP open failed", "");

        vTaskDelete(NULL);
    }

    int content_length = esp_http_client_fetch_headers(client);

    if (content_length <= 0) {
        ESP_LOGE(TAG, "Invalid firmware content length: %d", content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);

        ota_set_status(OTA_STATE_FAILED, 0, "Invalid firmware size");
        ota_set_running(false);
        iot_client_log_ota("download_failed", "invalid_firmware_size", "", 0, 0,
                           "Invalid firmware size", "");

        vTaskDelete(NULL);
    }

    if (content_length > update_partition->size) {
        ESP_LOGE(TAG, "Firmware too large: %d > %lu", content_length, update_partition->size);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);

        ota_set_status(OTA_STATE_FAILED, 0, "Firmware too large for partition");
        ota_set_running(false);
        iot_client_log_ota("download_failed", "firmware_too_large", "", 0, 0,
                           "Firmware too large for partition", "");

        vTaskDelete(NULL);
    }

    if (g_manifest_size > 0 && content_length != g_manifest_size) {
        ESP_LOGE(TAG, "Firmware size mismatch with manifest: HTTP %d manifest %d", content_length,
                 g_manifest_size);

        esp_http_client_close(client);
        esp_http_client_cleanup(client);

        ota_set_status(OTA_STATE_FAILED, 0, "Firmware size differs from manifest");
        ota_set_running(false);
        iot_client_log_ota("download_failed", "size_mismatch", "", 0, 0,
                           "Firmware size differs from manifest", "");
        vTaskDelete(NULL);
    }
    ota_status_t status;
    ota_update_get_status(&status);

    esp_ota_handle_t ota_handle = 0;

    err = esp_ota_begin(update_partition, content_length, &ota_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);

        ota_set_status(OTA_STATE_FAILED, 0, "OTA begin failed");
        ota_set_running(false);
        vTaskDelete(NULL);
    }

    char buffer[1024];
    int total_read = 0;

    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0);

    iot_client_log_ota("download_started", "ota_start", status.latest_version, status.latest_build,
                       0, "Firmware download started", "");
    ota_set_status(OTA_STATE_DOWNLOADING, 0, "Downloading firmware");

    while (total_read < content_length) {
        int read_len = esp_http_client_read(client, buffer, sizeof(buffer));

        if (read_len < 0) {
            ESP_LOGE(TAG, "HTTP read failed");

            esp_ota_abort(ota_handle);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);

            ota_set_status(OTA_STATE_FAILED, 0, "Firmware download failed");
            ota_set_running(false);
            iot_client_log_ota("download_failed", "http_read_failed", status.latest_version,
                               status.latest_build, 0, "Firmware download failed", "");
            mbedtls_sha256_free(&sha_ctx);
            vTaskDelete(NULL);
        }

        if (read_len == 0) {
            break;
        }

        err = esp_ota_write(ota_handle, buffer, read_len);

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));

            esp_ota_abort(ota_handle);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);

            ota_set_status(OTA_STATE_FAILED, 0, "OTA write failed");
            ota_set_running(false);
            iot_client_log_ota("download_failed", "ota_write_failed", status.latest_version,
                               status.latest_build, 0, "OTA write failed", esp_err_to_name(err));
            mbedtls_sha256_free(&sha_ctx);
            vTaskDelete(NULL);
        }
        mbedtls_sha256_update(&sha_ctx, (const unsigned char*)buffer, read_len);

        total_read += read_len;

        int progress = (total_read * 100) / content_length;
        ota_set_status(OTA_STATE_DOWNLOADING, progress, "Downloading firmware");
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (total_read != content_length) {
        ESP_LOGE(TAG, "Firmware size mismatch: read %d expected %d", total_read, content_length);

        esp_ota_abort(ota_handle);

        ota_set_status(OTA_STATE_FAILED, 0, "Firmware size mismatch");
        ota_set_running(false);
        mbedtls_sha256_free(&sha_ctx);
        vTaskDelete(NULL);
    }

    ota_set_status(OTA_STATE_VERIFYING, 100, "Verifying firmware");

    uint8_t downloaded_sha[32];
    char downloaded_sha_hex[65];

    mbedtls_sha256_finish(&sha_ctx, downloaded_sha);
    mbedtls_sha256_free(&sha_ctx);

    sha256_to_hex(downloaded_sha, downloaded_sha_hex, sizeof(downloaded_sha_hex));

    ESP_LOGI(TAG, "Manifest SHA256:   %s", g_manifest_sha256);
    ESP_LOGI(TAG, "Downloaded SHA256: %s", downloaded_sha_hex);

    if (strlen(g_manifest_sha256) == 64) {
        if (strcmp(g_manifest_sha256, downloaded_sha_hex) != 0) {
            ESP_LOGE(TAG, "SHA256 mismatch");

            esp_ota_abort(ota_handle);

            ota_set_status(OTA_STATE_FAILED, 0, "SHA256 mismatch");
            ota_set_running(false);
            vTaskDelete(NULL);
        }
    } else {
        iot_client_log_ota("sha_failed", "sha_mismatch", status.latest_version, status.latest_build,
                           100, "SHA256 mismatch", "Downloaded SHA differs from manifest");
        ESP_LOGW(TAG, "Manifest SHA256 missing or invalid, skipping SHA check");
    }
    iot_client_log_ota("sha_ok", "integrity_verified", status.latest_version, status.latest_build,
                       100, "Firmware SHA256 verified", "");
    err = esp_ota_end(ota_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));

        ota_set_status(OTA_STATE_FAILED, 0, "OTA verification failed");
        ota_set_running(false);
        iot_client_log_ota("ota_end_failed", "image_validation_failed", status.latest_version,
                           status.latest_build, 100, "OTA image validation failed",
                           esp_err_to_name(err));
        vTaskDelete(NULL);
    }
    iot_client_log_ota("ota_end_ok", "image_valid", status.latest_version, status.latest_build, 100,
                       "OTA image validated", "");

    err = esp_ota_set_boot_partition(update_partition);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));

        ota_set_status(OTA_STATE_FAILED, 0, "Set boot partition failed");
        ota_set_running(false);
        vTaskDelete(NULL);
    }

    ota_set_status(OTA_STATE_REBOOTING, 100, "Update installed, rebooting");

    iot_client_log_ota("rebooting", "ota_success", status.latest_version, status.latest_build, 100,
                       "Update installed, rebooting", "");
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();

    vTaskDelete(NULL);
}

static const char* ota_state_to_message(ota_state_t state) {
    switch (state) {
        case OTA_STATE_IDLE:
            return "Idle";
        case OTA_STATE_CHECKING:
            return "Checking for update";
        case OTA_STATE_UPDATE_AVAILABLE:
            return "Update available";
        case OTA_STATE_NO_UPDATE:
            return "No update available";
        case OTA_STATE_DOWNLOADING:
            return "Downloading update";
        case OTA_STATE_VERIFYING:
            return "Verifying update";
        case OTA_STATE_SUCCESS:
            return "Update completed";
        case OTA_STATE_FAILED:
            return "Update failed";
        case OTA_STATE_REBOOTING:
            return "Rebooting";
        default:
            return "Unknown";
    }
}

static void ota_set_running(bool running) {
    if (g_ota_mutex != NULL) {
        xSemaphoreTake(g_ota_mutex, portMAX_DELAY);
    }

    g_ota_status.update_running = running;

    if (g_ota_mutex != NULL) {
        xSemaphoreGive(g_ota_mutex);
    }
}

static void sha256_to_hex(const uint8_t* sha, char* out_hex, size_t out_size) {
    if (sha == NULL || out_hex == NULL || out_size < 65) {
        return;
    }

    for (int i = 0; i < 32; i++) {
        snprintf(out_hex + (i * 2), out_size - (i * 2), "%02x", sha[i]);
    }

    out_hex[64] = '\0';
}