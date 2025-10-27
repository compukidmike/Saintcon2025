#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "lvgl.h"
#include "../../nav.h"

// clang-format off
/**
 * @brief Content app types
 */
#define CONTENT_APP_LIST                                \
    X(NONE,     "-",                LV_SYMBOL_DUMMY)    \
    X(MENU,     "Menu",             LV_SYMBOL_DUMMY)    \
    X(GAME,     "Game",             FA_DICE_D6)         \
    X(SETTINGS, "Settings",         FA_WRENCH)          \
    X(CODE,     "Code Entry",       FA_DIALPAD)         \
    X(CREDITS,  "Credit Transfer",  FA_COINS)           \
    X(MAP,      "Map",              FA_MAP)
#undef X
// clang-format on
typedef enum {
#define X(val, lbl, sym) APP_##val,
    CONTENT_APP_LIST
#undef X
} content_app_t;
extern const char *content_app_map[];
extern const char *content_app_labels[];
extern const char *content_app_symbols[];
// Get the content app type from a string
content_app_t get_content_app_type(const char *str);
// Get the content app label from a content app type
const char *get_content_app_label(content_app_t app);

/**
 * @brief Navigation setup callback for apps
 *
 * @param content_area The content area container
 * @param nav_ctx The navigation context
 * @param user_data Optional user data passed during registration
 */
typedef void (*content_nav_setup_cb_t)(lv_obj_t *content_area, nav_ctx_t *nav_ctx, void *user_data);

/**
 * @brief Register a navigation setup callback for an app
 *
 * @param app The app type
 * @param callback The callback function
 * @param user_data Optional user data to pass to the callback
 */
void content_register_nav_callback(content_app_t app, content_nav_setup_cb_t callback, void *user_data);

// Navigation freezing
void content_nav_freeze();
void content_nav_thaw();
bool content_nav_is_frozen();

// Get the current content app
content_app_t get_current_app();
// Set the current content app
void set_current_app(content_app_t app);
// Mark layout dirty
void content_mark_layout_dirty();

// Create a container for the main content on the right side of the main screen
lv_obj_t *create_content_area(lv_obj_t *parent, const lv_obj_t *status_bar);

// Update the content area
void update_content_area();

#ifdef __cplusplus
}
#endif
