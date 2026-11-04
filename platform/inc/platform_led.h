#ifndef PLATFORM_LED_H
#define PLATFORM_LED_H

#include <stddef.h>
#include <stdint.h>

#include <esp_err.h>

esp_err_t platform_led_init();
esp_err_t platform_led_set_pixel(size_t index, uint8_t red, uint8_t green, uint8_t blue);
esp_err_t platform_led_refresh(void);
esp_err_t platform_led_clear(void);

#endif // PLATFORM_LED_H