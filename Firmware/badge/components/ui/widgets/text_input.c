#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

#include "text_input.h"
#include "theme.h"
#include "input.h"

typedef struct {
    lv_obj_t *container;
    lv_obj_t **cells;
    lv_obj_t *roller;
    lv_obj_t *backspace_label;
    text_input_config_t config;
    text_input_cb_t callback;
    void *user_data;
    char *buffer;
    uint8_t length;
    uint8_t caret;
    bool editing;
    nav_ctx_t *nav;
    lv_group_t *group;
} text_input_t;

static bool commit_selection(text_input_t *ti) {
    uint16_t sel = lv_roller_get_selected(ti->roller);
    char ch      = ti->config.charset[sel % strlen(ti->config.charset)];

    if (ti->caret < ti->config.max_len) {
        if (ti->caret < ti->length) {
            ti->buffer[ti->caret] = ch;
        } else {
            ti->buffer[ti->length++] = ch;
            ti->buffer[ti->length]   = '\0';
        }
        return true;
    }
    return false;
}
static void delete_left(text_input_t *ti) {
    if (ti->length == 0) {
        ti->caret = 0;
        return;
    }

    if (ti->caret < ti->length) {
        for (uint8_t i = ti->caret; i + 1 < ti->length; ++i) {
            ti->buffer[i] = ti->buffer[i + 1];
        }
        ti->buffer[--ti->length] = '\0';
    }

    if (ti->caret > 0) {
        ti->caret--;
    }
}

static void caret_anum_exec(void *obj, int32_t v) {
    lv_obj_set_style_text_opa((lv_obj_t *)obj, (lv_opa_t)v, LV_PART_MAIN);
}

static void caret_anim_start(lv_obj_t *label) {
    lv_anim_del(label, caret_anum_exec);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, LV_PART_MAIN);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, label);
    lv_anim_set_exec_cb(&a, caret_anum_exec);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_0);
    lv_anim_set_duration(&a, 300);
    lv_anim_set_reverse_duration(&a, 300);
    lv_anim_set_repeat_delay(&a, 300);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void caret_anim_stop(lv_obj_t *label) {
    lv_anim_del(label, caret_anum_exec);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, LV_PART_MAIN);
}

static void update_cells(text_input_t *ti) {
    bool editing      = ti->editing;
    lv_obj_t *focused = ti->group ? lv_group_get_focused(ti->group) : NULL;

    for (uint8_t i = 0; i < ti->config.max_len; i++) {
        lv_obj_t *cell  = ti->cells[i];
        lv_obj_t *label = lv_obj_get_child(cell, 0);

        const bool has_char     = (i < ti->length);
        const bool cell_focused = (focused == cell);

        if (has_char) {
            lv_label_set_text(label, ti->config.mask ? "*" : (char[2]){ti->buffer[i], 0});
            caret_anim_stop(label);
        } else {
            lv_label_set_text(label, (cell_focused && !editing) ? "_" : " ");
            if (cell_focused && !editing) {
                caret_anim_start(label);
            } else {
                caret_anim_stop(label);
            }
        }

        if (cell_focused) {
            if (editing) {
                lv_obj_add_state(cell, LV_STATE_EDITED);
                lv_obj_clear_state(cell, LV_STATE_FOCUS_KEY);
            } else {
                lv_obj_add_state(cell, LV_STATE_FOCUS_KEY);
                lv_obj_clear_state(cell, LV_STATE_EDITED);
            }
        } else {
            lv_obj_clear_state(cell, LV_STATE_EDITED | LV_STATE_FOCUS_KEY);
        }

        if (cell_focused && editing) {
            lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void cell_focus_cb(lv_event_t *e) {
    text_input_t *ti     = (text_input_t *)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *cell       = lv_event_get_target(e);

    if (code == LV_EVENT_FOCUSED) {
        for (uint8_t i = 0; i < ti->config.max_len; i++) {
            if (ti->cells[i] == cell) {
                ti->caret = i;
                break;
            }
        }
        uint8_t max_idx = (ti->length < ti->config.max_len) ? ti->length : ti->config.max_len - 1;
        if (ti->caret > max_idx) {
            ti->caret = max_idx;
        }
        update_cells(ti);
    } else if (code == LV_EVENT_DEFOCUSED) {
        lv_async_call((lv_async_cb_t)update_cells, ti);
    }
}

static void position_roller(text_input_t *ti) {
    lv_obj_t *current_cell = ti->cells[ti->caret];

    // Ensure layouts are up to date
    lv_obj_update_layout(current_cell);
    lv_obj_update_layout(ti->container);

    // Get cell and content coords
    lv_area_t cell_coords;
    lv_area_t cont_coords;
    lv_obj_get_coords(current_cell, &cell_coords);
    lv_obj_get_content_coords(ti->container, &cont_coords);

    // Get sizes
    const int cell_w   = lv_obj_get_style_width(current_cell, LV_PART_MAIN | LV_STATE_EDITED);
    const int roller_h = lv_obj_get_height(ti->roller);

    // Set roller width to match cell
    lv_obj_set_width(ti->roller, cell_w);

    // Center of roller should match center of cell
    int cell_center_x = (cell_coords.x1 + cell_coords.x2 + 1) / 2;
    int cell_center_y = (cell_coords.y1 + cell_coords.y2 + 1) / 2;

    // Screen to content coords
    int local_x = cell_center_x - cont_coords.x1 - (cell_w / 2);
    int local_y = cell_center_y - cont_coords.y1 - (roller_h / 2);

    // Center overlay directly over the cell
    lv_obj_set_pos(ti->roller, local_x, local_y);

    // Also position the backspace label
    int bs_h = lv_obj_get_height(ti->backspace_label);
    lv_obj_align_to(ti->backspace_label, ti->roller, LV_ALIGN_OUT_LEFT_MID, 1, -7 - bs_h);
}

static void set_editing(text_input_t *ti, bool on) {
    if (ti->group) {
        lv_group_set_editing(ti->group, on);
    }
    ti->editing = on;

    if (on) {
        if (ti->nav) {
            nav_suspend(ti->nav, true);
        }
        position_roller(ti);
        lv_obj_clear_flag(ti->roller, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ti->roller);
        lv_obj_clear_flag(ti->backspace_label, LV_OBJ_FLAG_HIDDEN);

        char cur        = (ti->caret < ti->length) ? ti->buffer[ti->caret] : ti->config.charset[0];
        const char *pos = strchr(ti->config.charset, cur);
        lv_roller_set_selected(ti->roller, pos ? (uint16_t)(pos - ti->config.charset) : 0, LV_ANIM_OFF);
    } else {
        lv_obj_add_flag(ti->roller, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ti->backspace_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lv_obj_get_child(ti->cells[ti->caret], 0), LV_OBJ_FLAG_HIDDEN);
        if (ti->nav) {
            nav_suspend(ti->nav, false);
        }
    }

    update_cells(ti);
}

static void focus_cell(text_input_t *ti, uint8_t idx) {
    if (!ti || !ti->nav) {
        return;
    }
    if (idx >= ti->config.max_len) {
        idx = ti->config.max_len - 1;
    }
    ti->caret = idx;

    bool editing = ti->editing;
    nav_focus(ti->nav, ti->cells[idx]);
    if (editing) {
        if (ti->group) {
            lv_group_set_editing(ti->group, true);
        }
        char cur        = (ti->caret < ti->length) ? ti->buffer[ti->caret] : ti->config.charset[0];
        const char *pos = strchr(ti->config.charset, cur);
        lv_roller_set_selected(ti->roller, pos ? (uint16_t)(pos - ti->config.charset) : 0, LV_ANIM_OFF);
        position_roller(ti);
    }

    update_cells(ti);
}

static void text_changed(text_input_t *ti) {
    if (ti->callback) {
        ti->callback(ti->buffer, ti->user_data);
    }
}

static void input_key_handler(lv_event_t *e) {
    text_input_t *ti  = (text_input_t *)lv_event_get_user_data(e);
    lv_key_t key      = lv_event_get_key(e);
    lv_obj_t *focused = ti->group ? lv_group_get_focused(ti->group) : NULL;
    bool editing      = ti->editing;

    // Identify if a cell is focused + which index
    int8_t focused_cell = -1;
    for (uint8_t i = 0; i < ti->config.max_len; i++) {
        if (ti->cells[i] == focused) {
            focused_cell = (int8_t)i;
            break;
        }
    }
    if (focused_cell >= 0) {
        ti->caret = (uint8_t)focused_cell;
    }

    if (!editing) {
        if (focused_cell >= 0) {
            if (key == LV_KEY_RIGHT) {
                uint8_t limit = ti->length;
                if (limit >= ti->config.max_len) {
                    limit = ti->config.max_len - 1;
                }

                uint8_t next = focused_cell + 1;
                if (next > limit) {
                    next = limit;
                }

                lv_event_stop_processing(e);
                focus_cell(ti, next);
                return;
            }

            if (key == LV_KEY_LEFT) {
                if (focused_cell > 0) {
                    lv_event_stop_processing(e);
                    focus_cell(ti, focused_cell - 1);
                    return;
                }
                // Let navigation handle moving to previous element
                return;
            }

            if (key == LV_KEY_UP || key == LV_KEY_DOWN) {
                // Let navigation handle moving to other components
                return;
            }

            if (key == LV_KEY_ENTER) {
                set_editing(ti, true);
                lv_event_stop_processing(e);
                return;
            }
        }
        return;
    }

    // EDIT MODE
    if (key == LV_KEY_UP || key == LV_KEY_DOWN) {
        uint16_t sel = lv_roller_get_selected(ti->roller);
        uint16_t n   = (uint16_t)strlen(ti->config.charset);
        sel          = (key == LV_KEY_UP) ? (sel + n - 1) % n : (sel + 1) % n;
        lv_roller_set_selected(ti->roller, sel, LV_ANIM_OFF);
        lv_event_stop_processing(e);
        return;
    }

    if (key == LV_KEY_LEFT) {
        delete_left(ti);
        if (ti->length == 0 && ti->caret == 0) {
            set_editing(ti, false);
        } else {
            focus_cell(ti, ti->caret);
        }
        text_changed(ti);
        lv_event_stop_processing(e);
        return;
    }

    if (key == LV_KEY_RIGHT) {
        uint16_t prev_sel = lv_roller_get_selected(ti->roller);
        commit_selection(ti);
        if (ti->caret + 1 < ti->config.max_len) {
            ti->caret++;
        }
        focus_cell(ti, ti->caret);
        if (ti->config.carry_repeat) {
            lv_roller_set_selected(ti->roller, prev_sel, LV_ANIM_OFF);
        }
        text_changed(ti);
        lv_event_stop_processing(e);
        return;
    }

    if (key == LV_KEY_ENTER) {
        commit_selection(ti);
        update_cells(ti);
        set_editing(ti, false);
        if (ti->caret < ti->config.max_len) {
            focus_cell(ti, ti->caret + 1);
        }
        text_changed(ti);
        lv_event_stop_processing(e);
        return;
    }
}

static void row_scroll_cb(lv_event_t *e) {
    text_input_t *ti = (text_input_t *)lv_event_get_user_data(e);
    if (!lv_obj_has_flag(ti->roller, LV_OBJ_FLAG_HIDDEN)) {
        position_roller(ti);
    }
}

static void start_edit_async_cb(void *ud) {
    text_input_t *ti_async = (text_input_t *)ud;
    if (!ti_async) {
        return;
    }
    focus_cell(ti_async, 0);
    set_editing(ti_async, true);
}

lv_obj_t *text_input_create(lv_obj_t *parent, text_input_config_t *config, const char *initial_text, text_input_cb_t callback,
                            void *user_data) {
    text_input_t *ti = calloc(1, sizeof(text_input_t));
    if (!ti) {
        return NULL;
    }

    ti->config = *config;
    // Default carry_repeat to true if caller left it uninitialized (struct may come from static literal)
    if (!config->carry_repeat) {
        ti->config.carry_repeat = true;
    }
    ti->callback  = callback;
    ti->user_data = user_data;
    ti->buffer    = calloc(config->max_len + 1, 1);
    if (!ti->buffer) {
        free(ti);
        return NULL;
    }

    if (initial_text) {
        strncpy(ti->buffer, initial_text, config->max_len);
        ti->buffer[config->max_len] = '\0';
        ti->length                  = strlen(ti->buffer);
    }

    ti->container = lv_obj_create(parent);
    lv_obj_set_size(ti->container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(ti->container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ti->container, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(ti->container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ti->container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(ti->container, 3, LV_PART_MAIN);
    lv_obj_center(ti->container);
    lv_obj_add_flag(ti->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ti->container, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_scrollbar_mode(ti->container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ti->container, LV_DIR_HOR);
    lv_obj_add_event_cb(ti->container, row_scroll_cb, LV_EVENT_SCROLL, ti);
    lv_obj_add_event_cb(ti->container, row_scroll_cb, LV_EVENT_SCROLL_END, ti);

    ti->cells = malloc(sizeof(lv_obj_t *) * config->max_len);
    for (uint8_t i = 0; i < config->max_len; i++) {
        lv_obj_t *cell = lv_obj_create(ti->container);
        lv_obj_set_size(cell, 30, 35);
        lv_obj_set_style_bg_color(cell, lv_color_hex(GRAY_SHADE_6), LV_PART_MAIN);
        lv_obj_set_style_bg_color(cell, lv_color_hex(BLUE_MAIN), LV_PART_MAIN | LV_STATE_FOCUS_KEY);
        lv_obj_set_style_bg_color(cell, lv_color_hex(YELLOW_MAIN), LV_PART_MAIN | LV_STATE_EDITED);
        lv_obj_set_style_bg_opa(cell, LV_OPA_60, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(cell, LV_OPA_100, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
        lv_obj_set_style_bg_opa(cell, LV_OPA_100, LV_PART_MAIN | LV_STATE_EDITED);
        lv_obj_set_style_border_color(cell, lv_color_hex(GRAY_TINT_2), LV_PART_MAIN);
        lv_obj_set_style_border_color(cell, lv_color_hex(WHITE), LV_PART_MAIN | LV_STATE_FOCUS_KEY);
        lv_obj_set_style_border_color(cell, lv_color_hex(YELLOW_LIGHT), LV_PART_MAIN | LV_STATE_EDITED);
        lv_obj_set_style_border_width(cell, 2, LV_PART_MAIN);
        lv_obj_set_style_border_side(cell, LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_TOP, LV_PART_MAIN);
        lv_obj_set_style_border_width(cell, 2, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
        lv_obj_set_style_border_side(cell, LV_BORDER_SIDE_FULL, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
        lv_obj_set_style_border_width(cell, 3, LV_PART_MAIN | LV_STATE_EDITED);
        lv_obj_set_style_border_side(cell, LV_BORDER_SIDE_FULL, LV_PART_MAIN | LV_STATE_EDITED);
        lv_obj_set_style_radius(cell, 5, LV_PART_MAIN);

        lv_obj_t *label = lv_label_create(cell);
        lv_obj_center(label);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_hex(WHITE), LV_PART_MAIN);

        // Make each cell focusable/clickable
        lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(cell, cell_focus_cb, LV_EVENT_FOCUSED, ti);
        lv_obj_add_event_cb(cell, cell_focus_cb, LV_EVENT_DEFOCUSED, ti);
        lv_obj_add_event_cb(cell, input_key_handler, LV_EVENT_KEY, ti);

        ti->cells[i] = cell;
    }

    size_t n   = strlen(config->charset);
    char *opts = malloc(n * 2 + 1);
    char *p    = opts;
    for (size_t i = 0; i < n; ++i) {
        *p++ = config->charset[i];
        *p++ = '\n';
    }
    if (p != opts) {
        p--;
    }
    *p = '\0';

    ti->roller = lv_roller_create(ti->container);
    lv_roller_set_options(ti->roller, opts, LV_ROLLER_MODE_INFINITE);
    free(opts);
    lv_roller_set_visible_row_count(ti->roller, 3);
    lv_obj_add_flag(ti->roller, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_opa(ti->roller, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ti->roller, lv_color_hex(GRAY_SHADE_4), LV_PART_MAIN);
    lv_obj_set_style_border_width(ti->roller, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(ti->roller, 5, LV_PART_SELECTED);
    lv_obj_set_style_border_color(ti->roller, lv_color_hex(WHITE), LV_PART_SELECTED);
    lv_obj_set_style_border_width(ti->roller, 2, LV_PART_SELECTED);
    lv_obj_set_style_text_color(ti->roller, lv_color_hex(GRAY_TINT_2), LV_PART_MAIN);
    lv_obj_set_style_text_color(ti->roller, lv_color_hex(WHITE), LV_PART_SELECTED);
    lv_obj_set_style_text_font(ti->roller, &lv_font_montserrat_16, LV_PART_MAIN);

    // Calculate and set a suitable height for the container based on roller height
    lv_obj_update_layout(ti->roller);
    int32_t roller_h = lv_obj_get_height(ti->roller);
    lv_obj_set_height(ti->container, roller_h + 10);
    lv_obj_update_layout(ti->container);
    lv_obj_add_flag(ti->roller, LV_OBJ_FLAG_HIDDEN);

    // Backspace label
    ti->backspace_label = lv_label_create(parent);
    lv_label_set_text(ti->backspace_label, LV_SYMBOL_BACKSPACE);
    lv_obj_set_style_text_font(ti->backspace_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(ti->backspace_label, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_opa(ti->backspace_label, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ti->backspace_label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ti->backspace_label, 2, LV_PART_MAIN);
    lv_obj_add_flag(ti->backspace_label, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(ti->backspace_label, LV_OBJ_FLAG_HIDDEN);

    // Key handler on container to intercept high-level keys
    lv_obj_add_event_cb(ti->container, input_key_handler, LV_EVENT_KEY, ti);

    lv_obj_set_user_data(ti->container, ti);
    update_cells(ti);

    // Defer edit mode activation until after the first LVGL cycle so layout is stable
    if (ti->config.start_editing) {
        lv_async_call(start_edit_async_cb, ti);
    }

    return ti->container;
}

void text_input_setup_nav(lv_obj_t *text_input, nav_ctx_t *nav_ctx) {
    text_input_t *ti = (text_input_t *)lv_obj_get_user_data(text_input);
    if (!ti || !nav_ctx) {
        return;
    }

    ti->nav   = nav_ctx;
    ti->group = nav_ctx->group;

    // Register all cells with navigation
    for (uint8_t i = 0; i < ti->config.max_len; i++) {
        nav_register(nav_ctx, ti->cells[i]);
    }

    // nav_ctx_set_wrap(nav_ctx, false, false);
    // nav_ctx_set_vert_fallback(nav_ctx, false);
}

void text_input_focus_cell(lv_obj_t *text_input, nav_ctx_t *nav_ctx, uint8_t cell_idx) {
    text_input_t *ti = (text_input_t *)lv_obj_get_user_data(text_input);
    if (ti && ti->cells[cell_idx] && nav_ctx) {
        nav_focus(nav_ctx, ti->cells[cell_idx]);
    }
}

char *text_input_get_text(lv_obj_t *text_input) {
    text_input_t *ti = (text_input_t *)lv_obj_get_user_data(text_input);
    return ti ? ti->buffer : NULL;
}

void text_input_set_text(lv_obj_t *text_input, const char *text) {
    text_input_t *ti = (text_input_t *)lv_obj_get_user_data(text_input);
    if (!ti) {
        return;
    }

    if (text) {
        strncpy(ti->buffer, text, ti->config.max_len);
        ti->buffer[ti->config.max_len] = '\0';
        ti->length                     = strlen(ti->buffer);
    } else {
        ti->buffer[0] = '\0';
        ti->length    = 0;
    }
    ti->caret = 0;
    update_cells(ti);
}

void text_input_focus_and_edit(lv_obj_t *text_input, uint8_t cell_idx) {
    text_input_t *ti = (text_input_t *)lv_obj_get_user_data(text_input);
    if (!ti) {
        return;
    }
    if (cell_idx >= ti->config.max_len) {
        cell_idx = ti->config.max_len - 1;
    }
    focus_cell(ti, cell_idx);
    if (!ti->editing) {
        set_editing(ti, true);
    } else {
        position_roller(ti);
    }
}

void text_input_set_caret(lv_obj_t *text_input, uint8_t cell_idx) {
    text_input_t *ti = (text_input_t *)lv_obj_get_user_data(text_input);
    if (!ti) {
        return;
    }
    if (cell_idx >= ti->config.max_len) {
        cell_idx = ti->config.max_len - 1;
    }
    focus_cell(ti, cell_idx);
    update_cells(ti);
}

void text_input_destroy(lv_obj_t *text_input) {
    text_input_t *ti = (text_input_t *)lv_obj_get_user_data(text_input);
    if (!ti) {
        return;
    }

    // Don't destroy nav or group - they're external
    if (ti->cells) {
        free(ti->cells);
    }
    if (ti->buffer) {
        free(ti->buffer);
    }
    free(ti);

    lv_obj_delete(text_input);
}
