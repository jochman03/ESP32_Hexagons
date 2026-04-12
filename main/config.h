/*
 * config.h
 *
 *  Created on: 14 apr 2026
 *      Author: jochman03
 */

#ifndef MAIN_CONFIG_H_
#define MAIN_CONFIG_H_

#include <stdbool.h>
#include <stdint.h>

//DEFAULTS
#define DEFAULT_STA_SSID "wifi"
#define DEFAULT_STA_PASS "pass1234"

#define DEFAULT_AP_SSID "HEX AP"
#define DEFAULT_AP_PASS "pass1234"

typedef struct {
    bool enabled;
    bool dhcp;

    char ssid[32];
    char password[64];

    char ip[16];
    char gateway[16];
    char netmask[16];
} app_wifi_sta_config_t;

typedef struct {
    bool enabled;
    bool always_on;
    bool hidden;

    char ssid[32];
    char password[64];

    uint8_t channel;
} app_wifi_ap_config_t;

typedef struct {
    app_wifi_sta_config_t sta;
    app_wifi_ap_config_t ap;
    bool valid;
} app_wifi_config_full_t;


void config_set_defaults(app_wifi_config_full_t* cfg);


#endif /* MAIN_CONFIG_H_ */





