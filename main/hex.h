#ifndef MAIN_HEX_H_
#define MAIN_HEX_H_

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

#define HEX_COUNT  15
#define STRIP_GPIO 18

typedef enum { STATIC, STARLIGHT, FADE } HexMode_T;

void hex_init(void);

void hex_set_mode(HexMode_T mode);
HexMode_T hex_get_mode(void);

void hex_set_speed(uint8_t speed);
uint8_t hex_get_speed(void);

void hex_set_enabled(bool enabled);
bool hex_is_enabled(void);

void hex_set_color(int hexIndex, uint8_t r, uint8_t g, uint8_t b);

uint8_t hex_get_color_r(int hexIndex);
uint8_t hex_get_color_g(int hexIndex);
uint8_t hex_get_color_b(int hexIndex);

bool hex_get_state(app_hex_config_t* state);
bool hex_apply_state(const app_hex_config_t* state);

#endif /* MAIN_HEX_H_ */