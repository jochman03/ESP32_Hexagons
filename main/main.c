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

#define LED_PIN 2

void app_main(void){
	app_wifi_config_full_t cfg;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    if (!config_load(&cfg)) {
        ESP_LOGE("CONFIG", "No saved config");
        config_set_defaults(&cfg);
        config_save(&cfg);
    }

    wifi_init();
    wifi_start(&cfg);

    hex_init();

    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    while(1){
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(500));

        gpio_set_level(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(500));    
    }
}
