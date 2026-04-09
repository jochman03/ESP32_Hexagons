/*
 * hex.h
 *
 *  Created on: 9 mar 2026
 *      Author: jochman03
 */

#ifndef MAIN_HEX_H_
#define MAIN_HEX_H_

#include <stdint.h>

#define HEX_COUNT     15
#define STRIP_GPIO    18

typedef enum {
    STATIC,
    STARLIGHT,
    FADE
} HexMode_T;


void hex_init();

HexMode_T hex_get_mode();
void hex_set_mode(HexMode_T mode);

uint8_t hex_get_color_r(const int hexIndex);
uint8_t hex_get_color_g(const int hexIndex);
uint8_t hex_get_color_b(const int hexIndex);
void hex_set_color(const int hexIndex, const uint8_t r, const uint8_t g, const uint8_t b);

uint8_t hex_get_speed();
void hex_set_speed(uint8_t speed);

#endif /* MAIN_HEX_H_ */
