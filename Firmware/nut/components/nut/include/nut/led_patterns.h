#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

/**
 * @brief Triggers an LED flash pattern.
 *
 * @param r Red color component (0-255).
 * @param g Green color component (0-255).
 * @param b Blue color component (0-255).
 * @param brightness Brightness level (0-255).
 * @param flashes Number of flashes.
 * @param on_ms Duration of the "on" state in milliseconds.
 * @param off_ms Duration of the "off" state in milliseconds.
 */
void flash_leds(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness, int flashes, uint32_t on_ms, uint32_t off_ms);

/**
 * @brief Triggers an LED flash pattern for error states.
 */
void led_pattern_flash_red();

/**
 * @brief Triggers a LED sparkle pattern.
 */
void led_pattern_sparkles();

//This is just for Nut code - doesn't go with code above
typedef enum {
    LED_PATTERN_NONE = 0,
    LED_PATTERN_BLINK,
    LED_PATTERN_CHASE
} led_pattern_type_t;

typedef struct {
    led_pattern_type_t type;
    uint32_t red, green, blue;
    uint32_t on_ms;
    uint32_t off_ms;
    uint32_t blink_count;     // 0 = infinite
    uint32_t speed_ms;
    uint32_t leds_on;
    uint32_t chase_count;     // 0 = infinite
    bool chase_dir;           // Chase Direction
    bool fade;                // enable/disable fading
} led_pattern_t;

void led_pattern_set(const led_pattern_t *pattern);

#ifdef __cplusplus
}
#endif
