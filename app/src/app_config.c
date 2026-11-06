
#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "app_config.h"
#include "app_hex.h"

static const char* TAG = "APP_CONFIG";

#define CONFIG_NAMESPACE "app_cfg"

#define HEX_CONFIG_KEY "hex"

bool app_config_hex_save(const app_config_hex_t* state) {
    if (state == NULL) {
        return false;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &nvs);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_blob(nvs, HEX_CONFIG_KEY, state, sizeof(*state));

    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }

    nvs_close(nvs);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "app_config_hex_save failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "hex state saved: mode=%u speed=%u size=%u", state->mode, state->speed,
             (unsigned)sizeof(*state));

    return true;
}

bool app_config_hex_load(app_config_hex_t* state) {
    if (state == NULL) {
        return false;
    }

    nvs_handle_t nvs;
    size_t size = sizeof(*state);

    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READONLY, &nvs);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "hex state nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_get_blob(nvs, HEX_CONFIG_KEY, state, &size);
    nvs_close(nvs);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "hex state nvs_get_blob failed: %s", esp_err_to_name(err));
        return false;
    }

    if (size != sizeof(*state)) {
        ESP_LOGW(TAG, "hex state size mismatch: stored=%u expected=%u", (unsigned)size,
                 (unsigned)sizeof(*state));
        return false;
    }
    if (!state->valid) {
        ESP_LOGW(TAG, "hex config invalid.");
        return false;
    }

    if (state->version != CONFIG_VERSION) {
        ESP_LOGW(TAG, "hex config version mismatch: stored=%lu expected=%d",
                 (unsigned long)state->version, CONFIG_VERSION);
        return false;
    }

    ESP_LOGI(TAG, "hex state loaded: mode=%u speed=%u", state->mode, state->speed);

    return true;
}