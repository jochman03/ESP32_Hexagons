/*
 * wifi.h
 *
 *  Created on: 14 apr 2026
 *      Author: jochman03
 */

#ifndef MAIN_WIFI_H_
#define MAIN_WIFI_H_

#include "config.h"

#define STA_CONNECTION_RETRY_COUNT 5

typedef enum {
    WIFI_EVT_STA_START,
    WIFI_EVT_STA_CONNECTED,
    WIFI_EVT_STA_DISCONNECTED,
    WIFI_EVT_STA_FAIL,
    WIFI_EVT_STA_TIMEOUT,
    WIFI_EVT_AP_START,
    WIFI_EVT_RECONFIGURE
} wifi_evt_type_t;

typedef struct {
    wifi_evt_type_t type;
    int reason;
} wifi_evt_t;

void wifi_init(void);
void wifi_start(const app_wifi_config_full_t* cfg);







#endif /* MAIN_WIFI_H_ */
