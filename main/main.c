/*
 * main.c
 *
 *  Created on: 9 mar 2026
 *      Author: jochman03
 */

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include "esp_log.h"

#include "platform_led.h"
#include "platform_board.h"

#include "app_config.h"
#include "app_hex.h"
#include "app_state.h"

#include "network_wifi.h"
#include "network_ota.h"
#include "network_self_test.h"
#include "network_iot_client.h"
#include "network_maintenance.h"

void app_main(void) {
    self_test_init();
    iot_client_init();

    platform_board_init();

    platform_led_init();
    hex_init();
    self_test_set_flags(SELF_TEST_LED_OK);

    app_state_init();
    ota_update_init();

    wifi_init();
    hex_config_t hex_state;

    if (config_load_hex_state(&hex_state)) {
        hex_apply_state(&hex_state);
    }

    app_wifi_config_t cfg;

    if (!config_load_wifi(&cfg)) {
        config_get_default_wifi(&cfg);
    }
    self_test_set_flags(SELF_TEST_CONFIG_OK);
    vTaskDelay(pdMS_TO_TICKS(50));

    if (platform_board_recovery_pressed()) {
        cfg.ap.enabled = true;
        cfg.ap.always_on = true;

        config_save_wifi(&cfg);
    }

    wifi_start(&cfg);
    self_test_start_ota();
    maintenance_start();
}