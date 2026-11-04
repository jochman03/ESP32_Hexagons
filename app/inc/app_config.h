/*
 *  app_config.h
 *
 *  Created on: 14 Apr 2026
 *  Author: jochman03
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define CONFIG_VERSION 4

#define MAX_SSID_CHARACTERS 31
#define MAX_PASS_CHARACTERS 63

#define AP_MAX_CONNECTIONS         5
#define STA_CONNECTION_RETRY_COUNT 5

#define DEFAULT_STA_SSID ""
#define DEFAULT_STA_PASS ""
#define DEFAULT_AP_SSID  "HEX AP"
#define DEFAULT_AP_PASS  "pass1234"

#define DEFAULT_IP      "192.168.2.83"
#define DEFAULT_GATEWAY "192.168.2.1"
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
} app_wifi_config_sta_t;

typedef struct {
    bool enabled;
    bool always_on;
    bool hidden;

    char ssid[32];
    char password[64];

    uint8_t channel;
} app_wifi_config_ap_t;

typedef struct {
    app_wifi_config_sta_t sta;
    app_wifi_config_ap_t ap;

    bool valid;
    uint32_t version;
} app_wifi_config_t;

typedef struct {
    uint32_t version;
    bool valid;
    bool enabled;

    uint8_t mode;
    uint8_t speed;
    uint8_t colors[HEX_COUNT * 3];

} hex_config_t;

typedef enum {
    TIME_SEC = 0,
    TIME_MIN = 1,
    TIME_HOUR = 2,
} time_unit_t;

typedef struct {
    bool enabled;
    uint8_t every;
    time_unit_t unit;
} interval_setting_t;

typedef struct {
    bool valid;
    uint32_t version;

    interval_setting_t wifi_retry;
    interval_setting_t auto_update;
} maintenance_config_t;

void config_get_default_wifi(app_wifi_config_t* cfg);

bool config_load_wifi(app_wifi_config_t* cfg);
bool config_save_wifi(const app_wifi_config_t* cfg);
bool config_update_wifi(app_wifi_config_t* new_cfg);

bool config_clear_sta(void);
bool config_update_ap(const char* ssid, const char* password, uint8_t channel, bool hidden,
                      bool always_on);
bool config_update_sta(const char* ssid, const char* password, bool dhcp, const char* ip,
                       const char* netmask, const char* gateway, const char* dns);

bool config_save_hex_state(const hex_config_t* state);
bool config_load_hex_state(hex_config_t* state);

void config_get_default_maintenance(maintenance_config_t* cfg);
bool config_load_maintenance(maintenance_config_t* cfg);
bool config_save_maintenance(const maintenance_config_t* cfg);
bool config_update_maintenance(const maintenance_config_t* cfg);

#endif // APP_CONFIG_H