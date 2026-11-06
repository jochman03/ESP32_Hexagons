/*
 *  network_config.h
 *
 *  Created on: 06 Nov 2026
 *  Author: jochman03
 */

#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

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

#define NETWORK_CONFIG_VERSION 4

typedef struct network_config_wifi_sta_t {
    bool enabled;
    bool dhcp;

    char ssid[32];
    char password[64];

    char ip[16];
    char gateway[16];
    char netmask[16];
    char dns[16];
} network_config_wifi_sta_t;

typedef struct network_config_wifi_ap_t {
    bool enabled;
    bool always_on;
    bool hidden;
    uint8_t channel;

    char ssid[32];
    char password[64];
} network_config_wifi_ap_t;

typedef struct network_config_wifi_t {
    network_config_wifi_sta_t sta;
    network_config_wifi_ap_t ap;

    bool valid;
    uint32_t version;

} network_config_wifi_t;

typedef enum network_interval_unit_t {
    NETWORK_TIME_SEC = 0,
    NETWORK_TIME_MIN = 1,
    NETWORK_TIME_HOUR = 2,
} network_interval_unit_t;

typedef struct network_interval_t {
    bool enabled;
    uint8_t every;
    network_interval_unit_t unit;
} network_interval_t;

typedef struct network_config_maintenance_t {
    bool valid;
    uint32_t version;

    network_interval_t wifi_retry_interval;
    network_interval_t auto_update_interval;
} network_config_maintenance_t;

void network_config_wifi_default(network_config_wifi_t* cfg);

bool network_config_wifi_load(network_config_wifi_t* cfg);
bool network_config_wifi_save(const network_config_wifi_t* cfg);
bool network_config_wifi_update(network_config_wifi_t* new_cfg);

bool network_config_sta_clear(void);
bool network_config_ap_update(const char* ssid, const char* password, uint8_t channel, bool hidden,
                              bool always_on);
bool network_config_sta_update(const char* ssid, const char* password, bool dhcp, const char* ip,
                               const char* netmask, const char* gateway, const char* dns);

void network_config_maintenance_default(network_config_maintenance_t* cfg);
bool network_config_maintenance_load(network_config_maintenance_t* cfg);
bool network_config_maintenance_save(const network_config_maintenance_t* cfg);
bool network_config_maintenance_update(const network_config_maintenance_t* cfg);

#endif // NETWORK_CONFIG_H