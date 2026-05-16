/*
 *  ota_update.h
 *
 *  Created on: 15 May 2026
 *  Author: jochman03
 */

#ifndef MAIN_OTA_UPDATE_H_
#define MAIN_OTA_UPDATE_H_

#include "esp_err.h"
#include <stdbool.h>

typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_CHECKING,
    OTA_STATE_UPDATE_AVAILABLE,
    OTA_STATE_NO_UPDATE,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VERIFYING,
    OTA_STATE_SUCCESS,
    OTA_STATE_FAILED,
    OTA_STATE_REBOOTING
} ota_state_t;

typedef struct {
    ota_state_t state;

    int progress;

    int current_build;
    int latest_build;

    char current_version[16];
    char latest_version[16];

    char message[96];

    bool update_available;
    bool update_running;
} ota_status_t;

void ota_update_init(void);

esp_err_t ota_update_check(void);
esp_err_t ota_update_start_simulated(void);
esp_err_t ota_update_start(void);

void ota_update_get_status(ota_status_t* out_status);

#endif