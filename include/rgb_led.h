#pragma once
#include <stdint.h>

typedef enum {
    LED_EFFECT_STATIC = 0,
    LED_EFFECT_BREATHE,
    LED_EFFECT_STROBE
} led_effect_t;

void rgb_led_init(void);
void rgb_led_set_color(uint8_t r, uint8_t g, uint8_t b);
void rgb_led_set_hex(const char *hex_color, led_effect_t effect, uint8_t brightness);
void rgb_led_set_room_mode(const char *mode);
