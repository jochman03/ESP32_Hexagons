/*
 *  wifi.h
 *
 *  Created on: 14 Apr 2026
 *  Author: jochman03
 */

#ifndef MAIN_WIFI_H_
#define MAIN_WIFI_H_

#include "config.h"
#include "esp_netif.h"

typedef enum {
    WIFI_EVT_STA_STARTED,
    WIFI_EVT_STA_CONNECTED,
    WIFI_EVT_STA_DISCONNECTED,
    WIFI_EVT_AP_STARTED,
    WIFI_EVT_RECONFIGURE
} wifi_evt_type_t;

typedef struct {
    wifi_evt_type_t type;
    int reason;
} wifi_evt_t;

void wifi_init(void);
void wifi_start(const app_wifi_config_full_t* cfg);
void wifi_reconfigure(void);
esp_netif_t* wifi_get_sta_netif(void);

#endif /* MAIN_WIFI_H_ */