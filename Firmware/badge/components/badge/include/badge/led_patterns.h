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

/**
 * @brief Starts a continuous LED ease in/out pattern for a specific color.
 *
 * @param r Red color component (0-255).
 * @param g Green color component (0-255).
 * @param b Blue color component (0-255).
 * @param fade_in_ms Duration of fade in phase in milliseconds.
 * @param hold_in_ms Duration to hold at full brightness in milliseconds.
 * @param fade_out_ms Duration of fade out phase in milliseconds.
 * @param hold_out_ms Duration to hold at zero brightness in milliseconds.
 * @param use_cosine_easing If true, use cosine easing; otherwise use linear.
 * @param max_brightness Maximum brightness level (0-255). Pattern will scale to this instead of global brightness.
 * @return int Layer handle (>=0) on success, negative on error.
 */
int led_pattern_ease(uint8_t r, uint8_t g, uint8_t b, uint32_t fade_in_ms, uint32_t hold_in_ms, uint32_t fade_out_ms,
                     uint32_t hold_out_ms, bool use_cosine_easing, uint8_t max_brightness);

/**
 * @brief Starts the faction LED pattern based on current badge configuration.
 *
 * If already running, restarts the pattern with current faction color.
 */
void led_pattern_faction_start();

/**
 * @brief Stops the faction LED pattern.
 */
void led_pattern_faction_stop();

/**
 * @brief Starts the Dawn Accord violation LED pattern.
 *
 * Pattern: Two red flashes (100ms on, 100ms off, 100ms on) followed by 500ms pause, repeating.
 * Uses high priority layer to override other patterns.
 *
 * @return int Layer handle (>=0) on success, negative on error.
 */
int led_pattern_dawn_accord_violation_start();

/**
 * @brief Stops the Dawn Accord violation LED pattern.
 */
void led_pattern_dawn_accord_violation_stop();

#ifdef __cplusplus
}
#endif
