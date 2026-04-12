#include "config.h"
#include <string.h>

void config_set_defaults(app_wifi_config_full_t* cfg){
    memset(cfg, 0, sizeof(*cfg));

    // STA
    cfg->sta.enabled = true;
    cfg->sta.dhcp = true;
    strcpy(cfg->sta.ssid, DEFAULT_STA_SSID);
    strcpy(cfg->sta.password, DEFAULT_STA_PASS);

    // AP
    cfg->ap.enabled = true;
    cfg->ap.always_on = false;
    strcpy(cfg->ap.ssid, DEFAULT_AP_SSID);
    strcpy(cfg->ap.password, DEFAULT_AP_PASS);
    cfg->ap.channel = 1;
    cfg->ap.hidden = false;
    cfg->valid = true;
}