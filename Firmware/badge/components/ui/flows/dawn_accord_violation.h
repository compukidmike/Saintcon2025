#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "dawn_accord.h"
#include "lvgl.h"

/**
 * @brief Shows a non-dismissible full-screen violation modal.
 *
 * This modal displays a Dawn Accord violation alert with bright red background,
 * centered text, and cannot be dismissed by user input.
 *
 * @param violation_type The type of violation detected (INHIBITOR or STRIP)
 * @return lv_obj_t* The modal overlay object, or NULL on failure
 */
lv_obj_t *dawn_accord_violation_show(dawn_accord_violation_t violation_type);

/**
 * @brief Shows a temporary "violation cleared" success modal.
 *
 * This modal displays a green success message and auto-dismisses after a few seconds.
 *
 * @return lv_obj_t* The modal overlay object, or NULL on failure
 */
lv_obj_t *dawn_accord_violation_cleared_show(void);

/**
 * @brief Shows a one-time "bypass success" modal when both protections are disabled.
 *
 * This modal informs the user they have successfully disabled both protections.
 *
 * @return lv_obj_t* The modal overlay object, or NULL on failure
 */
lv_obj_t *dawn_accord_bypass_show(void);

/**
 * @brief Closes/destroys a Dawn Accord modal overlay.
 *
 * @param overlay The overlay object to close (safe to pass NULL)
 */
void dawn_accord_modal_close(lv_obj_t *overlay);

#ifdef __cplusplus
}
#endif
