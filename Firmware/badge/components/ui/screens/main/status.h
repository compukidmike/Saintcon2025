#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "ui.h"

// Create the status bar for the main screen
lv_obj_t *create_status_bar(lv_obj_t *parent);

// Update the status bar with the current state
void update_status_bar(ui_state_t *state);

#ifdef __cplusplus
}
#endif
