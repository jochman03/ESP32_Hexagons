/*
 *  config.h
 *
 *  Created on: 14 Apr 2026
 *  Author: jochman03
 */

#ifndef MAIN_CONFIG_H_
#define MAIN_CONFIG_H_

#include <stdbool.h>
#include <stdint.h>

#define CONFIG_VERSION    1
#define HEX_STATE_VERSION 2

#define MAX_SSID_CHARACTERS 31
#define MAX_PASS_CHARACTERS 63

#define AP_MAX_CONNECTIONS         5
#define STA_CONNECTION_RETRY_COUNT 5

#define DEFAULT_STA_SSID ""
#define DEFAULT_STA_PASS ""
#define DEFAULT_AP_SSID  "HEX AP"
#define DEFAULT_AP_PASS  "pass1234"

#define DEFAULT_IP      "192.168.1.50"
#define DEFAULT_GATEWAY "192.168.1.1"
#define DEFAULT_MASK    "255.255.255.0"
#define DEFAULT_DNS     "8.8.8.8"

#define HEX_COUNT 15

typedef struct {
    bool enabled;
    bool dhcp;

    char ssid[32];
    char password[64];

    char ip[16];
    char gateway[16];
    char netmask[16];
    char dns[16];
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
    uint32_t version;
} app_wifi_config_full_t;

typedef struct {
    uint32_t version;
    bool valid;
    bool enabled;

    uint8_t mode;
    uint8_t speed;
    uint8_t colors[HEX_COUNT * 3];

} app_hex_config_t;

void config_set_defaults(app_wifi_config_full_t* cfg);

bool config_load(app_wifi_config_full_t* cfg);
bool config_save(const app_wifi_config_full_t* cfg);
bool config_update(app_wifi_config_full_t* new_cfg);

bool config_clear_sta(void);
bool config_update_ap(const char* ssid, const char* password, uint8_t channel, bool hidden,
                      bool always_on);
bool config_update_sta(const char* ssid, const char* password, bool dhcp, const char* ip,
                       const char* netmask, const char* gateway, const char* dns);

bool config_save_hex_state(const app_hex_config_t* state);
bool config_load_hex_state(app_hex_config_t* state);

app_wifi_config_full_t default_config(void);

#endif /* MAIN_CONFIG_H_ */