#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "sdkconfig.h"

// Display orientation (matching LVGL display orientation in lv_diplay_rotation_t)
typedef enum {
    DISPLAY_ORIENTATION_LANDSCAPE,
    DISPLAY_ORIENTATION_PORTRAIT,
    DISPLAY_ORIENTATION_LANDSCAPE_FLIP,
    DISPLAY_ORIENTATION_PORTRAIT_FLIP,
} display_orientation_t;

// Orientation parameters
typedef struct {
    uint16_t h_res;
    uint16_t v_res;
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
} display_orientation_params_t;

// Screen timeout configuration
typedef enum {
    SCREEN_TIMEOUT_NONE = 0, // No timeout
#ifdef CONFIG_LCD_BACKLIGHT_CONTROL_PWM
    SCREEN_TIMEOUT_DIM = 1, // Dim the screen after timeout
#endif
    SCREEN_TIMEOUT_OFF = 2, // Turn off the screen after timeout
} screen_timeout_t;

#ifdef __cplusplus
}
#endif
