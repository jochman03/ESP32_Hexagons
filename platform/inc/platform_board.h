#ifndef PLATFORM_BOARD_H
#define PLATFORM_BOARD_H

#include <stdbool.h>
#include <esp_err.h>

#define PLATFORM_BOARD_STRIP_GPIO 48
#define PLATFORM_BOARD_LED_NUMBER 180

#define PLATFORM_BOARD_LED_GPIO      13
#define PLATFORM_BOARD_RECOVERY_GPIO 16

esp_err_t platform_board_init(void);
bool platform_board_recovery_pressed(void);
esp_err_t platform_board_status_led_set(bool enabled);

#endif // PLATFORM_BOARD_H