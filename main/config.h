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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

//DEFAULTS
#define DEFAULT_STA_SSID "wifi"
#define DEFAULT_STA_PASS "pass1234"

#define DEFAULT_AP_SSID "HEX AP"
#define DEFAULT_AP_PASS "pass1234"

#define CONFIG_VERSION 3

#define MAX_SSID_CHARACTERS 32
#define MAX_PASS_CHARACTERS 64

#define AP_MAX_CONNECTIONS 5

#define DEFAULT_IP "192.168.4.2"
#define DEFAULT_MASK "255.255.255.0"
#define DEFAULT_GATEWAY "192.168.4.1"
#define DEFAULT_DNS "8.8.8.8"


typedef struct {
    bool enabled;
    bool dhcp;

    char ssid[MAX_SSID_CHARACTERS];
    char password[MAX_PASS_CHARACTERS];

    char ip[16];
    char gateway[16];
    char netmask[16];
    char dns[8];
} app_wifi_sta_config_t;

typedef struct {
    bool enabled;
    bool always_on;
    bool hidden;

    char ssid[MAX_SSID_CHARACTERS];
    char password[MAX_PASS_CHARACTERS];

    uint8_t channel;
} app_wifi_ap_config_t;

typedef struct {
    app_wifi_sta_config_t sta;
    app_wifi_ap_config_t ap;
    bool valid;
    uint8_t version;
} app_wifi_config_full_t;


void config_set_defaults(app_wifi_config_full_t* cfg);

bool config_save(const app_wifi_config_full_t* cfg);
bool config_load(app_wifi_config_full_t* cfg);
bool config_update(app_wifi_config_full_t* new_cfg);

app_wifi_config_full_t default_config();

#endif /* MAIN_CONFIG_H_ */





