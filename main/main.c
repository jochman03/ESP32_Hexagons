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

#include "wifi.h"
#include "config.h"
#include "hex.h"

#define LED_PIN 2

void app_main(void){
	// Initialize NVS
	app_wifi_config_full_t cfg;

    config_set_defaults(&cfg);

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
