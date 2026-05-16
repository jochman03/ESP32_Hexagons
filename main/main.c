/*
 * main.c
 *
 *  Created on: 9 mar 2026
 *      Author: jochman03
 */

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "wifi.h"
#include "config.h"
#include "hex.h"
#include "state_save.h"
#include "ota_update.h"
#include "app_diagnostics.h"
#include "iot_client.h"

#define LED_PIN            2
#define WIFI_RECOVERY_GPIO 4

static bool wifi_recovery_button_pressed(void);

void app_main(void) {
    app_diagnostics_init();
    iot_client_init();

    app_wifi_config_full_t cfg;

    hex_init();
    state_save_init();
    ota_update_init();

    wifi_init();
    app_hex_config_t hex_state;

    if (config_load_hex_state(&hex_state)) {
        hex_apply_state(&hex_state);
    }

    if (!config_load(&cfg)) {
        cfg = default_config();
    }
    app_diagnostics_set(APP_DIAG_CONFIG_OK);

    if (wifi_recovery_button_pressed()) {
        cfg.ap.enabled = true;
        cfg.ap.always_on = true;

        config_save(&cfg);
    }

    wifi_start(&cfg);
    app_diagnostics_start_ota_self_test();
}

static bool wifi_recovery_button_pressed(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << WIFI_RECOVERY_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&cfg);

    vTaskDelay(pdMS_TO_TICKS(50));

    return gpio_get_level(WIFI_RECOVERY_GPIO) == 0;
}