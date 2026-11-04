

#include <stddef.h>
#include <stdint.h>
#include <esp_err.h>
#include <esp_log.h>

#include "led_strip.h"

#include "platform_led.h"
#include "platform_board.h"

static led_strip_handle_t strip;

esp_err_t platform_led_init() {
    led_strip_config_t strip_config = {
        .strip_gpio_num = PLATFORM_BOARD_STRIP_GPIO,
        .max_leds = PLATFORM_BOARD_LED_NUMBER,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = led_strip_clear(strip);

    return ret;
}

esp_err_t platform_led_set_pixel(size_t index, uint8_t red, uint8_t green, uint8_t blue) {
    if (index >= PLATFORM_BOARD_LED_NUMBER) {
        return ESP_ERR_INVALID_ARG;
    }
    return led_strip_set_pixel(strip, index, red, green, blue);
}

esp_err_t platform_led_refresh(void) {
    return led_strip_refresh(strip);
}

esp_err_t platform_led_clear(void) {
    return led_strip_clear(strip);
}
