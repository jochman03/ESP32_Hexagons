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
#define HEX_COUNT      15

typedef struct app_config_hex_t {
    uint32_t version;
    bool valid;
    bool enabled;

    uint8_t mode;
    uint8_t speed;
    uint8_t colors[HEX_COUNT * 3];

} app_config_hex_t;

bool app_config_hex_save(const app_config_hex_t* state);
bool app_config_hex_load(app_config_hex_t* state);

#endif // APP_CONFIG_H