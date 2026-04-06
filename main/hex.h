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

typedef enum hex_mode_t {
    STATIC,
    STARLIGHT,
    FADE
} hex_mode_t;

void hex_setColor(int hex_index, uint8_t r, uint8_t g, uint8_t b);
hex_mode_t hex_getMode();
void hex_setMode(hex_mode_t mode);
void hex_init();


uint8_t hex_getColor_r(int hex_index);
uint8_t hex_getColor_g(int hex_index);
uint8_t hex_getColor_b(int hex_index);

void hex_setSpeed(uint8_t speed);
uint8_t hex_getSpeed();

#endif /* MAIN_HEX_H_ */
