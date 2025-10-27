#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "nav.h"

#define TI_BASE32_NC   "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
#define TI_ALPHA_UPPER "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define TI_ALPHA_LOWER "abcdefghijklmnopqrstuvwxyz"
#define TI_NUMERIC     "0123456789"
#define TI_SYMBOLS     " .,_-+@#$/\\&*()[]{}!?~:^;\"'"
#define TI_COMBINED    TI_ALPHA_UPPER TI_ALPHA_LOWER TI_NUMERIC TI_SYMBOLS

typedef struct {
    const char *charset;
    uint8_t max_len;
    bool mask;
    bool carry_repeat;  // if true, keep current roller selection when advancing to next cell in edit mode
    bool start_editing; // if true, enter edit mode immediately (roller visible on first render)
} text_input_config_t;

typedef void (*text_input_cb_t)(const char *text, void *user_data);

lv_obj_t *text_input_create(lv_obj_t *parent, text_input_config_t *config, const char *initial_text, text_input_cb_t callback,
                            void *user_data);
void text_input_setup_nav(lv_obj_t *text_input, nav_ctx_t *nav_ctx);
void text_input_focus_cell(lv_obj_t *text_input, nav_ctx_t *nav_ctx, uint8_t cell_idx);
char *text_input_get_text(lv_obj_t *text_input);
void text_input_set_text(lv_obj_t *text_input, const char *text);

// Convenience: focus a cell then ensure edit mode is active so roller is visible.
void text_input_focus_and_edit(lv_obj_t *text_input, uint8_t cell_idx);
// Set caret to index (clamped) and focus that cell without entering edit mode.
void text_input_set_caret(lv_obj_t *text_input, uint8_t cell_idx);

void text_input_destroy(lv_obj_t *text_input);

#ifdef __cplusplus
}
#endif
