#include "nvs.h"
#include "nvs_flash.h"
#include "config.h"
#include <string.h>
#include "esp_log.h"
#define NVS_NAMESPACE "wifi"
#define NVS_KEY "cfg"

void config_set_defaults(app_wifi_config_full_t* cfg){
    *cfg = default_config();
}

static bool validate_config(const app_wifi_config_full_t *cfg){
    if (cfg->sta.enabled) {
        if (strlen(cfg->sta.ssid) == 0){
            return false;
        }            
    }

    if (cfg->ap.enabled) {
        if (strlen(cfg->ap.ssid) == 0){
            return false;
        }            
    }

    return true;
}

bool config_load(app_wifi_config_full_t* cfg){
    nvs_handle_t nvs = {0};
    size_t size = sizeof(*cfg);

    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK){
        ESP_LOGW("CONFIG", "nvs_open(NVS_NAMESPACE");
        return false;
    }

    esp_err_t err = nvs_get_blob(nvs, NVS_KEY, cfg, &size);
    nvs_close(nvs);

    if (err != ESP_OK){
        ESP_LOGW("CONFIG", "err != ESP_OK");
        return false;
    }

    if (size != sizeof(*cfg)){
        ESP_LOGW("CONFIG", "Size mismatch: stored=%d expected=%d", size, sizeof(*cfg));
        return false;
    }

    if (cfg->version != CONFIG_VERSION){
        ESP_LOGW("CONFIG", "Version mismatch: %d", cfg->version);
        return false;
    }

    ESP_LOGI("CONFIG", "Loaded OK size=%d", size);
    return true;
}

bool config_save(const app_wifi_config_full_t* cfg){
    nvs_handle_t nvs;

    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK){
        return false;
    }
    if (nvs_set_blob(nvs, NVS_KEY, cfg, sizeof(*cfg)) != ESP_OK){
        nvs_close(nvs);
        return false;
    }
    esp_err_t err = nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI("CONFIG", "Saving blob size: %d", sizeof(*cfg));

    return err == ESP_OK;
}

app_wifi_config_full_t default_config(){
    app_wifi_config_full_t cfg = {0};
	memset(&cfg, 0, sizeof(cfg));

    // AP
    snprintf(cfg.ap.ssid, sizeof(cfg.ap.ssid), "%s", DEFAULT_AP_SSID);
    snprintf(cfg.ap.password, sizeof(cfg.ap.password), "%s", DEFAULT_AP_PASS);
    cfg.ap.always_on = false;
    cfg.ap.channel = 1;
    cfg.ap.hidden = false;
    cfg.ap.enabled = true;

    // STA
    snprintf(cfg.sta.ssid, sizeof(cfg.sta.ssid), "%s", DEFAULT_STA_SSID);
    snprintf(cfg.sta.password, sizeof(cfg.sta.password), "%s", DEFAULT_STA_PASS);
    snprintf(cfg.sta.ip, sizeof(cfg.sta.ip), "%s", DEFAULT_IP);
    snprintf(cfg.sta.netmask, sizeof(cfg.sta.netmask), "%s", DEFAULT_MASK);
    snprintf(cfg.sta.gateway, sizeof(cfg.sta.gateway), "%s", DEFAULT_GATEWAY);
    snprintf(cfg.sta.dns, sizeof(cfg.sta.dns), "%s", DEFAULT_DNS);
    cfg.sta.enabled = true;
    cfg.sta.dhcp = true;



    cfg.valid = true;
    cfg.version = CONFIG_VERSION;

    return cfg;
}

bool config_update(app_wifi_config_full_t* new_cfg){
    if (!validate_config(new_cfg)){
        return false;
    }
    new_cfg->version = CONFIG_VERSION;
    new_cfg->valid = true;

    return config_save(new_cfg);
}