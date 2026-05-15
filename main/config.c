
#include <string.h>
#include <stdio.h>

#include "config.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "hex.h"

static const char* TAG = "CONFIG";

#define CONFIG_NAMESPACE "wifi_cfg"
#define CONFIG_KEY       "cfg"
#define HEX_STATE_KEY    "hex_cfg"

void config_set_defaults(app_wifi_config_full_t* cfg) {
    memset(cfg, 0, sizeof(*cfg));

    // STA
    cfg->sta.enabled = false;
    cfg->sta.dhcp = true;
    strcpy(cfg->sta.ssid, DEFAULT_STA_SSID);
    strcpy(cfg->sta.password, DEFAULT_STA_PASS);
    strcpy(cfg->sta.ip, "192.168.1.50");
    strcpy(cfg->sta.gateway, "192.168.1.1");
    strcpy(cfg->sta.netmask, "255.255.255.0");
    strcpy(cfg->sta.dns, "8.8.8.8");

    // AP
    cfg->ap.enabled = true;
    cfg->ap.always_on = false;
    strcpy(cfg->ap.ssid, DEFAULT_AP_SSID);
    strcpy(cfg->ap.password, DEFAULT_AP_PASS);
    cfg->ap.channel = 1;
    cfg->ap.hidden = false;

    cfg->valid = true;
    cfg->version = CONFIG_VERSION;
}

app_wifi_config_full_t default_config(void) {
    app_wifi_config_full_t cfg;
    config_set_defaults(&cfg);
    return cfg;
}

bool config_update(app_wifi_config_full_t* new_cfg) {
    if (new_cfg == NULL) {
        return false;
    }

    new_cfg->version = CONFIG_VERSION;
    new_cfg->valid = true;

    return config_save(new_cfg);
}

bool config_save(const app_wifi_config_full_t* cfg) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &nvs);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_blob(nvs, CONFIG_KEY, cfg, sizeof(*cfg));

    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }

    nvs_close(nvs);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "config_save failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "config saved: sta_enabled=%d ssid=%s ap_ssid=%s version=%lu size=%u",
             cfg->sta.enabled, cfg->sta.ssid, cfg->ap.ssid, (unsigned long)cfg->version,
             (unsigned)sizeof(*cfg));

    return true;
}

bool config_load(app_wifi_config_full_t* cfg) {
    nvs_handle_t nvs;
    size_t size = sizeof(*cfg);

    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READONLY, &nvs);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_get_blob(nvs, CONFIG_KEY, cfg, &size);
    nvs_close(nvs);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_get_blob failed: %s", esp_err_to_name(err));
        return false;
    }

    if (size != sizeof(*cfg)) {
        ESP_LOGW(TAG, "size mismatch: stored=%u expected=%u", (unsigned)size,
                 (unsigned)sizeof(*cfg));
        return false;
    }

    if (!cfg->valid) {
        ESP_LOGW(TAG, "config invalid flag");
        return false;
    }

    if (cfg->version != CONFIG_VERSION) {
        ESP_LOGW(TAG, "config version mismatch: stored=%lu expected=%d",
                 (unsigned long)cfg->version, CONFIG_VERSION);
        return false;
    }

    ESP_LOGI(TAG, "config loaded: sta_enabled=%d ssid=%s ap_ssid=%s version=%lu", cfg->sta.enabled,
             cfg->sta.ssid, cfg->ap.ssid, (unsigned long)cfg->version);

    return true;
}

bool config_update_ap(const char* ssid, const char* password, uint8_t channel, bool hidden,
                      bool always_on) {

    app_wifi_config_full_t cfg;

    if (!config_load(&cfg)) {
        cfg = default_config();
    }

    memset(cfg.ap.ssid, 0, sizeof(cfg.ap.ssid));
    memset(cfg.ap.password, 0, sizeof(cfg.ap.password));

    snprintf(cfg.ap.ssid, sizeof(cfg.ap.ssid), "%s", ssid ? ssid : DEFAULT_AP_SSID);
    snprintf(cfg.ap.password, sizeof(cfg.ap.password), "%s", password ? password : "");

    cfg.ap.enabled = true;
    cfg.ap.always_on = always_on;
    cfg.ap.hidden = hidden;
    cfg.ap.channel = channel;

    cfg.valid = true;
    cfg.version = CONFIG_VERSION;

    return config_save(&cfg);
}

bool config_update_sta(const char* ssid, const char* password, bool dhcp, const char* ip,
                       const char* netmask, const char* gateway, const char* dns) {

    app_wifi_config_full_t cfg;

    if (!config_load(&cfg)) {
        cfg = default_config();
    }

    if (ssid == NULL || strlen(ssid) == 0) {
        ESP_LOGW(TAG, "STA SSID is empty");
        return false;
    }

    memset(cfg.sta.ssid, 0, sizeof(cfg.sta.ssid));
    memset(cfg.sta.password, 0, sizeof(cfg.sta.password));

    snprintf(cfg.sta.ssid, sizeof(cfg.sta.ssid), "%s", ssid);
    snprintf(cfg.sta.password, sizeof(cfg.sta.password), "%s", password ? password : "");

    cfg.sta.enabled = true;
    cfg.sta.dhcp = dhcp;

    if (ip) {
        snprintf(cfg.sta.ip, sizeof(cfg.sta.ip), "%s", ip);
    }

    if (netmask) {
        snprintf(cfg.sta.netmask, sizeof(cfg.sta.netmask), "%s", netmask);
    }

    if (gateway) {
        snprintf(cfg.sta.gateway, sizeof(cfg.sta.gateway), "%s", gateway);
    }

    if (dns) {
        snprintf(cfg.sta.dns, sizeof(cfg.sta.dns), "%s", dns);
    }

    cfg.valid = true;
    cfg.version = CONFIG_VERSION;

    return config_save(&cfg);
}

bool config_clear_sta(void) {
    app_wifi_config_full_t cfg;

    if (!config_load(&cfg)) {
        cfg = default_config();
    }

    cfg.sta.enabled = false;
    cfg.sta.dhcp = true;

    cfg.sta.ssid[0] = '\0';
    cfg.sta.password[0] = '\0';

    snprintf(cfg.sta.ip, sizeof(cfg.sta.ip), "%s", DEFAULT_IP);
    snprintf(cfg.sta.gateway, sizeof(cfg.sta.gateway), "%s", DEFAULT_GATEWAY);
    snprintf(cfg.sta.netmask, sizeof(cfg.sta.netmask), "%s", DEFAULT_MASK);
    snprintf(cfg.sta.dns, sizeof(cfg.sta.dns), "%s", DEFAULT_DNS);

    cfg.ap.enabled = true;

    cfg.valid = true;
    cfg.version = CONFIG_VERSION;

    return config_save(&cfg);
}

bool config_save_hex_state(const app_hex_config_t* state) {
    if (state == NULL) {
        return false;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &nvs);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_blob(nvs, HEX_STATE_KEY, state, sizeof(*state));

    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }

    nvs_close(nvs);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "config_save_hex_state failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "hex state saved: mode=%u speed=%u size=%u", state->mode, state->speed,
             (unsigned)sizeof(*state));

    return true;
}

bool config_load_hex_state(app_hex_config_t* state) {
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

    err = nvs_get_blob(nvs, HEX_STATE_KEY, state, &size);
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

    if (!state->valid || state->version != HEX_STATE_VERSION) {
        ESP_LOGW(TAG, "hex state version/valid mismatch");
        return false;
    }

    ESP_LOGI(TAG, "hex state loaded: mode=%u speed=%u", state->mode, state->speed);

    return true;
}