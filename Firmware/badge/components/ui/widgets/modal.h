#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "nav.h"

// Modal button callback signature. User data passed from modal_open() call.
typedef void (*modal_button_cb_t)(uint8_t btn_index, void *user_data);

// Internal context for message modal (opaque to user)
typedef struct {
    nav_scope_guard_t nav_guard;
    lv_indev_t *keypad;
    nav_ctx_t *nav_ctx;
    modal_button_cb_t cb;
    void *user_data;
    lv_obj_t *panel;
} modal_ctx_t;

// Configuration for a basic message modal.
typedef struct {
    const char *title;          // Optional title (NULL to omit)
    const char *message;        // Body text (required for message variant)
    const char *buttons[2];     // Up to 2 button labels; first must be non-NULL. If second NULL -> single button.
    bool success;               // If true apply success styling, else error/neutral styling
    modal_button_cb_t callback; // Invoked on button press (index 0 or 1). Modal auto-closes before callback.
    void *user_data;            // Opaque pointer passed to callback
} modal_message_config_t;

// Open a standard message modal. Returns overlay root object or NULL on failure.
lv_obj_t *modal_message_open(const modal_message_config_t *cfg);

// Show an async modal overlay with spinner and optional message. Returns overlay object.
lv_obj_t *modal_async_open(const char *message);

// Close/destroy an async modal overlay.
void modal_async_close(lv_obj_t *overlay);

// Destroy/close a modal overlay previously created (safe to pass NULL).
void modal_close(lv_obj_t *overlay);

#ifdef __cplusplus
}
#endif
