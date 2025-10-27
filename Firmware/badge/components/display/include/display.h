#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "freertos/FreeRTOS.h"

// Compmonent headers to expose externally
#include "../config.h"

// Backlight control options and levels
#define LCD_BACKLIGHT_PIN CONFIG_LCD_BACKLIGHT_GPIO
#if defined(CONFIG_LCD_BACKLIGHT_CONTROL_PWM)
    #define LCD_BACKLIGHT_ON  128
    #define LCD_BACKLIGHT_OFF 0
// Set the backlight level (0-255).
void set_backlight(uint8_t level);
#elif defined(CONFIG_LCD_BACKLIGHT_CONTROL_SIMPLE)
    #define LCD_BACKLIGHT_ON  1
    #define LCD_BACKLIGHT_OFF !LCD_BACKLIGHT_ON
// Turn the backlight on or off.
void set_backlight(bool on);
#endif

// LVGL mutex lock
bool lvgl_lock(TickType_t timeout_ticks, const char *file, int line);

// LVGL mutex unlock
bool lvgl_unlock(const char *file, int line);

// Initializes the display, backlight,touch interface, and LVGL.
void display_init();

// Return whether or not the display is initialized.
bool display_ready();

#ifdef CONFIG_LCD_BACKLIGHT_CONTROL_PWM
/**
 * @brief Set the screen timeout configuration.
 *
 * @param action The desired screen timeout action.
 * @param level The desired backlight level (0-255) for dimming.
 * @param timeout The desired screen timeout in milliseconds.
 */
void set_screen_timeout_config(screen_timeout_t action, uint8_t level, uint32_t timeout);
#elif defined(CONFIG_LCD_BACKLIGHT_CONTROL_SIMPLE)
/**
 * @brief Set the screen timeout configuration.
 *
 * @param action The desired screen timeout action.
 * @param timeout The desired screen timeout setting.
 */
void set_screen_timeout_config(screen_timeout_t action, uint32_t timeout);
#endif

/**
 * @brief Start the screen timeout timer.
 */
void screen_timeout_start();

/**
 * @brief Stop the screen timeout timer.
 *
 * @param reset_level Whether to reset the backlight to its default level after stopping the timer.
 */
void screen_timeout_stop(bool reset_level);

/**
 * @brief Reset the screen timeout (and turn the backlight on if it was off).
 */
void screen_timeout_reset();

/**
 * @brief Check if the screen has timed out.
 *
 * @return true if the screen is dimmed/off due to timeout, false otherwise.
 */
bool is_screen_timed_out();

/**
 * @brief Callback function type for screen wake events.
 */
typedef void (*screen_wake_callback_t)(void);

/**
 * @brief Register a callback to be called when the screen wakes up from timeout.
 *
 * @param callback The callback function to register.
 */
void register_screen_wake_callback(screen_wake_callback_t callback);

// Get the display orientation parameters.
display_orientation_params_t get_params_for_display_orientation(display_orientation_t orientation);

// Get the display orientation.
display_orientation_t get_display_orientation();

// Get the current display orientation parameters.
display_orientation_params_t get_display_orientation_params();

// Set the display orientation.
void set_display_orientation(display_orientation_t orientation);

#ifdef __cplusplus
}
#endif
