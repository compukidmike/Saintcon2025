#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

#ifdef CONFIG_INPUT_TOUCH_ENABLED
    #include "touch.h"
#endif

/**
 * @brief Initialize any enabled inputs (touch and/or joystick)
 *        and register them with LVGL.
 *
 * @param display The LVGL display object to register the input devices with.
 */
void input_init(lv_display_t *display);

/**
 * @brief Get the currently active input device.
 *
 * @return The currently active LVGL input device.
 */
lv_indev_t *input_get_device();

#ifdef __cplusplus
}
#endif