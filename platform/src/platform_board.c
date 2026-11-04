#include <driver/gpio.h>

#include "platform_board.h"

esp_err_t platform_board_init(void) {
    gpio_config_t config = {
        .pin_bit_mask = 1ULL << PLATFORM_BOARD_RECOVERY_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&config);
    if (ret != ESP_OK) {
        return ret;
    }

    config = (gpio_config_t){
        .pin_bit_mask = 1ULL << PLATFORM_BOARD_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&config);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = gpio_set_level(PLATFORM_BOARD_LED_GPIO, 0);

    return ret;
}

bool platform_board_recovery_pressed(void) {
    return gpio_get_level(PLATFORM_BOARD_RECOVERY_GPIO) == 0;
}

esp_err_t platform_board_status_led_set(bool enabled) {
    return gpio_set_level(PLATFORM_BOARD_LED_GPIO, enabled ? 1 : 0);
}
