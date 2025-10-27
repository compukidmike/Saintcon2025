#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "esp_err.h"
#include "sdkconfig.h"

#include <stdbool.h>

// Use Kconfig values for LED configuration
#define LED_GPIO CONFIG_LED_GPIO
#define NUM_LEDS CONFIG_LED_NUM_LEDS

/**
 * @brief Initialize the LED strip.
 *
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t led_init();

/**
 * @brief Set the color of a specific LED in the strip.
 *
 * @param index The index of the LED to set (0-based).
 * @param red The red color value (0-255).
 * @param green The green color value (0-255).
 * @param blue The blue color value (0-255).
 *
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t led_set(uint32_t index, uint32_t red, uint32_t green, uint32_t blue);

/**
 * @brief Set global brightness for all LED operations (0-255).
 *
 * @param brightness Global brightness level (0 = off, 255 = full brightness).
 *
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t led_set_brightness(uint8_t brightness);

/**
 * @brief Get current global brightness level.
 *
 * @return uint8_t Current brightness level (0-255).
 */
uint8_t led_get_brightness();

/**
 * @brief Set LED color with automatic brightness scaling using global brightness.
 *
 * @param index The index of the LED to set (0-based).
 * @param red The red color value (0-255).
 * @param green The green color value (0-255).
 * @param blue The blue color value (0-255).
 *
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t led_set_scaled(uint32_t index, uint32_t red, uint32_t green, uint32_t blue);

/**
 * @brief Set LED color with custom brightness scaling (per-LED brightness).
 *
 * @param index The index of the LED to set (0-based).
 * @param red The red color value (0-255).
 * @param green The green color value (0-255).
 * @param blue The blue color value (0-255).
 * @param brightness The brightness level for this LED (0-255).
 *
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t led_set_with_brightness(uint32_t index, uint32_t red, uint32_t green, uint32_t blue, uint8_t brightness);

/**
 * @brief Clear the LED strip (turn off all LEDs).
 *
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t led_clear();

/**
 * @brief Show the current state of the LED strip (update the LEDs).
 *
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t led_show();

// -------------------------------------------------------------------------------------------------
// Layered Pattern Management API
// -------------------------------------------------------------------------------------------------

// Priority levels (lower numeric value = higher priority)
typedef enum {
    LED_PRIORITY_HIGH   = 0,
    LED_PRIORITY_MEDIUM = 1,
    LED_PRIORITY_LOW    = 2,
} led_pattern_priority_t;

/**
 * @brief LED pattern render callback.
 *
 * This callback is called from the LED render task context at a fixed frame interval.
 *
 * @param now_ms Current time in milliseconds.
 * @param user_ctx User-provided context pointer.
 */
typedef void (*led_pattern_render_cb)(uint32_t now_ms, void *user_ctx);

/**
 * @brief Create a new LED pattern layer.
 *
 * @param priority Priority for arbitration (HIGH overrides MEDIUM overrides LOW).
 * @param render_cb Callback to render a frame. Must not be NULL.
 * @param user_ctx Opaque pointer passed to callback.
 * @param active If true layer starts active immediately.
 * @param exclusive If true and active, suppresses all lower-priority (and same-priority non-exclusive) layers.
 * @param clear_before If true the manager clears the LED buffer before calling render_cb.
 * @return int Handle (>=0) on success, negative error code on failure.
 */
int led_layer_create(led_pattern_priority_t priority, led_pattern_render_cb render_cb, void *user_ctx, bool active,
                     bool exclusive, bool clear_before);

/** Destroy a previously created layer (handle becomes invalid). */
esp_err_t led_layer_destroy(int handle);

/** Activate or deactivate (logical on/off) a layer without destroying it. */
esp_err_t led_layer_set_active(int handle, bool active);

/** Pause/unpause a layer (paused layers are ignored like inactive but retain active flag). */
esp_err_t led_layer_set_paused(int handle, bool paused);

/** Change exclusive status. */
esp_err_t led_layer_set_exclusive(int handle, bool exclusive);

/** Change priority of an existing layer. */
esp_err_t led_layer_set_priority(int handle, led_pattern_priority_t priority);

/** Bring layer to top ordering among peers of same priority (tie-breaker). */
esp_err_t led_layer_bump(int handle);

/** Convenience: create a static color layer (fills all LEDs each frame). Returns handle. */
int led_layer_create_static_color(led_pattern_priority_t priority, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness,
                                  bool active, bool exclusive);

/** Update static color layer parameters (only valid for layers created with static helper). */
esp_err_t led_layer_update_static_color(int handle, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);

/** Get handle of currently rendered layer (or -1 if none). */
int led_layer_current();

/** Force immediate re-evaluation of active layer (e.g., after external manual LED usage). */
void led_layer_request_refresh();

/**
 * Gamma utilities. Gamma correction is disabled by default unless led_gamma_enable(true) is called.
 * Apply AFTER brightness scaling if you want perceptual uniformity.
 */
void led_gamma_enable(bool enable);
bool led_gamma_is_enabled();
uint8_t led_gamma_apply(uint8_t v);
void led_gamma_apply_rgb(uint8_t *r, uint8_t *g, uint8_t *b);

// ----------------------------------------------------------------------------------------------
// Global pause/resume helpers
// ----------------------------------------------------------------------------------------------

/**
 * @brief Pause all LED layers except the one specified by keep_handle.
 *
 * @param keep_handle Handle of the layer to keep active (pass -1 to keep none active).
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_layers_pause_all_except(int keep_handle);

/**
 * @brief Resume all previously paused LED layers.
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_layers_resume_all();

/**
 * @brief Dump current layer table to log (development only)
 */
void led_layers_dump();

// ----------------------------------------------------------------------------------------------
// Timed / one-shot helpers
// ----------------------------------------------------------------------------------------------

/**
 * @brief Create a timed layer that auto-destroys after a duration. If auto_destroy is false it
 *        will simply deactivate (keeping handle valid). Returns handle or <0 on error.
 *
 * @param priority Priority for arbitration (HIGH overrides MEDIUM overrides LOW).
 * @param render_cb Callback to render a frame. Must not be NULL.
 * @param user_ctx Opaque pointer passed to callback.
 * @param exclusive If true and active, suppresses all lower-priority (and same-priority non-exclusive) layers.
 * @param clear_before If true the manager clears the LED buffer before calling render_cb.
 * @param duration_ms Duration in milliseconds before the layer is destroyed or deactivated.
 * @param auto_destroy If true, the layer will be destroyed after the duration; if false, it will be deactivated.
 * @return int Handle of the created layer or <0 on error.
 */
int led_layer_create_timed(led_pattern_priority_t priority, led_pattern_render_cb render_cb, void *user_ctx, bool exclusive,
                           bool clear_before, uint32_t duration_ms, bool auto_destroy);

/**
 * @brief Create a static color timed layer. The layer will display a solid color for the specified duration.
 *
 * @param priority Priority for arbitration (HIGH overrides MEDIUM overrides LOW).
 * @param r Red color component (0-255).
 * @param g Green color component (0-255).
 * @param b Blue color component (0-255).
 * @param brightness Brightness level (0-255).
 * @param exclusive If true and active, suppresses all lower-priority (and same-priority non-exclusive) layers.
 * @param duration_ms Duration in milliseconds before the layer is destroyed or deactivated.
 * @param auto_destroy If true, the layer will be destroyed after the duration; if false, it will be deactivated.
 * @return int Handle of the created layer or <0 on error.
 */
int led_layer_create_static_color_timed(led_pattern_priority_t priority, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness,
                                        bool exclusive, uint32_t duration_ms, bool auto_destroy);

#ifdef __cplusplus
}
#endif