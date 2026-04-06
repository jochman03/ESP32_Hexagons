/*
 * rgb_led.h
 *
 *  Created on: 1 gru 2025
 *      Author: jakub
 */

#ifndef MAIN_RGB_LED_H_
#define MAIN_RGB_LED_H_

#include <stdint.h>


// RGB LED GPIO's
#define RGB_LED_RED_GPIO 21
#define RGB_LED_GREEN_GPIO 22
#define RGB_LED_BLUE_GPIO 23

// RGB LED color mix channels
#define RGB_LED_CHANNEL_NUM 3

// RGB LED configuration

typedef struct {
	int channel;
	int gpio;
	int mode;
	int timer_index;
} ledc_info_t;

void rgb_led_set_color(uint8_t red, uint8_t green, uint8_t blue);

// Color to indicate wifi connection started
void rgb_led_wifi_app_started(void);

// Color to indicate http server started
void rgb_led_http_server_started(void);

// Color to indicate esp32 is connected to an access point started
void rgb_led_wifi_connected(void);


#endif /* MAIN_RGB_LED_H_ */
