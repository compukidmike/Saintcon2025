#include <stdbool.h>
#include <stddef.h>
#include <stddef.h>
#include "lvgl.h"
#include "theme.h"
#include <stddef.h> // for NULL
#include <stddef.h> // for NULL
#include "lvgl.h"
#include "theme.h" // for color macros and LVGL enums
#include "nav.h"
#include "input.h"
#include <string.h>
#include "modal.h"
#include "esp_log.h"

static const char *TAG = "ui/modal";

// Forward declarations
static void ensure_styles(void);

// Static style objects
static bool modal_styles_inited = false;
static lv_style_t style_overlay_dim;
static lv_style_t style_panel_base;
static lv_style_t style_panel_success;
static lv_style_t style_panel_error;

// Async modal context
typedef struct {
    nav_scope_guard_t nav_guard;
    lv_indev_t *keypad;
    nav_ctx_t *nav_ctx;
    lv_obj_t *panel;
} modal_async_ctx_t;

// Ensure modal styles are initialized
static void ensure_styles() {
    if (modal_styles_inited) {
        return;
    }
    lv_style_init(&style_overlay_dim);
    lv_style_set_bg_color(&style_overlay_dim, lv_color_hex(BLACK));
    lv_style_set_bg_opa(&style_overlay_dim, LV_OPA_60);

    lv_style_init(&style_panel_base);
    lv_style_set_bg_color(&style_panel_base, lv_color_hex(GRAY_SHADE_7));
    lv_style_set_bg_opa(&style_panel_base, LV_OPA_COVER);
    lv_style_set_border_width(&style_panel_base, 2);
    lv_style_set_border_color(&style_panel_base, lv_color_hex(GRAY_SHADE_4));
    lv_style_set_radius(&style_panel_base, 5);
    lv_style_set_pad_all(&style_panel_base, 0);
    lv_style_set_pad_row(&style_panel_base, 8);
    lv_style_set_pad_column(&style_panel_base, 8);

    lv_style_init(&style_panel_success);
    lv_style_set_border_color(&style_panel_success, lv_color_hex(GREEN_MAIN));

    lv_style_init(&style_panel_error);
    lv_style_set_border_color(&style_panel_error, lv_color_hex(RED_MAIN));

    modal_styles_inited = true;
}

lv_obj_t *modal_async_open(const char *message) {
    ensure_styles();

    lv_obj_t *overlay = lv_obj_create(lv_scr_act());
    lv_obj_add_style(overlay, &style_overlay_dim, LV_PART_MAIN);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(overlay, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_obj_set_style_pad_all(overlay, 15, LV_PART_MAIN);
    lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(overlay, 0, LV_PART_MAIN);

    modal_async_ctx_t *ctx = (modal_async_ctx_t *)lv_malloc(sizeof(modal_async_ctx_t));
    if (!ctx) {
        lv_obj_del(overlay);
        return NULL;
    }
    memset(ctx, 0, sizeof(*ctx));
    lv_obj_set_user_data(overlay, ctx);

    // Navigation scope (no nav objects registered; disables navigation until closed)
    lv_indev_t *keypad = input_get_device();
    lv_group_t *group  = lv_group_create();
    ctx->keypad        = keypad;
    ctx->nav_guard     = nav_scope_push(keypad, group);
    ctx->nav_ctx       = nav_ctx_create(keypad, group, overlay);
    nav_ctx_set_wrap(ctx->nav_ctx, 0, 0);

    // Inner panel
    lv_obj_t *panel = lv_obj_create(overlay);
    ctx->panel      = panel;
    lv_obj_add_style(panel, &style_panel_base, LV_PART_MAIN);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(panel, lv_pct(60));
    lv_obj_center(panel);

    // Spinner
    lv_obj_t *spinner = lv_spinner_create(panel);
    lv_spinner_set_anim_params(spinner, 800, 200);
    lv_obj_remove_flag(spinner, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(spinner, 48, 48);
    lv_obj_set_style_bg_opa(spinner, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(spinner, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(spinner, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(BLUE_MAIN), LV_PART_INDICATOR);

    if (message) {
        lv_obj_t *msg = lv_label_create(panel);
        lv_label_set_text(msg, message);
        lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(msg, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_set_style_text_color(msg, lv_color_hex(WHITE), LV_PART_MAIN);
        lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_width(msg, lv_pct(100));
    }

    return overlay;
}

void modal_async_close(lv_obj_t *overlay) {
    if (!overlay) {
        return;
    }
    modal_async_ctx_t *ctx = (modal_async_ctx_t *)lv_obj_get_user_data(overlay);
    if (!ctx) {
        lv_obj_del(overlay);
        return;
    }
    if (ctx->keypad) {
        nav_scope_pop(ctx->keypad, ctx->nav_guard);
    }
    lv_free(ctx);
    lv_obj_del(overlay);
}

static void modal_button_event_cb(lv_event_t *e) {
    lv_obj_t *btn     = lv_event_get_target(e);
    lv_obj_t *overlay = lv_event_get_user_data(e);
    if (!overlay) {
        return;
    }
    modal_ctx_t *ctx = (modal_ctx_t *)lv_obj_get_user_data(overlay);
    if (!ctx) {
        return;
    }
    // Determine button index
    uint8_t idx      = 0;
    lv_obj_t *parent = lv_obj_get_parent(btn);
    if (parent) {
        uint32_t cnt = lv_obj_get_child_count(parent);
        for (uint32_t i = 0; i < cnt; i++) {
            if (lv_obj_get_child(parent, i) == btn) {
                idx = (uint8_t)i;
                break;
            }
        }
    }
    modal_button_cb_t cb = ctx->cb;
    void *user_data      = ctx->user_data;
    modal_close(overlay);
    if (cb) {
        cb(idx, user_data);
    }
}

void modal_close(lv_obj_t *overlay) {
    if (!overlay) {
        return;
    }
    modal_ctx_t *ctx = (modal_ctx_t *)lv_obj_get_user_data(overlay);
    if (ctx) {
        if (ctx->nav_ctx) {
            nav_ctx_destroy(ctx->nav_ctx);
        }
        if (ctx->keypad) {
            nav_scope_pop(ctx->keypad, ctx->nav_guard);
        }
        lv_free(ctx);
    }
    lv_obj_del(overlay);
}

lv_obj_t *modal_message_open(const modal_message_config_t *cfg) {
    if (!cfg || !cfg->buttons[0]) {
        ESP_LOGE(TAG, "Invalid modal config");
        return NULL;
    }
    ensure_styles();

    lv_obj_t *overlay = lv_obj_create(lv_scr_act());
    lv_obj_add_style(overlay, &style_overlay_dim, LV_PART_MAIN);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(overlay, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_obj_set_style_pad_all(overlay, 15, LV_PART_MAIN);
    lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(overlay, 0, LV_PART_MAIN);

    modal_ctx_t *ctx = (modal_ctx_t *)lv_malloc(sizeof(modal_ctx_t));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to allocate modal context");
        lv_obj_del(overlay);
        return NULL;
    }
    memset(ctx, 0, sizeof(*ctx));
    lv_obj_set_user_data(overlay, ctx);

    // Navigation scope
    lv_indev_t *keypad = input_get_device();
    lv_group_t *group  = lv_group_create();
    ctx->keypad        = keypad;
    ctx->nav_guard     = nav_scope_push(keypad, group);
    ctx->nav_ctx       = nav_ctx_create(keypad, group, overlay);
    nav_ctx_set_wrap(ctx->nav_ctx, false, false);

    // Inner panel
    lv_obj_t *panel = lv_obj_create(overlay);
    ctx->panel      = panel;
    lv_obj_add_style(panel, &style_panel_base, LV_PART_MAIN);
    if (cfg->success) {
        lv_obj_add_style(panel, &style_panel_success, LV_PART_MAIN);
    } else {
        lv_obj_add_style(panel, &style_panel_error, LV_PART_MAIN);
    }
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(panel, lv_pct(100));
    lv_obj_set_height(panel, lv_pct(100));
    lv_obj_set_style_pad_gap(panel, 0, LV_PART_MAIN);

    if (cfg->title) {
        lv_obj_t *title_container = lv_obj_create(panel);
        lv_obj_set_flex_flow(title_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(title_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_flex_grow(title_container, 0);
        lv_obj_set_width(title_container, lv_pct(100));
        lv_obj_set_height(title_container, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(title_container, 10, LV_PART_MAIN);
        lv_obj_set_style_border_width(title_container, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(title_container, 0, LV_PART_MAIN);
        lv_obj_remove_flag(title_container, LV_OBJ_FLAG_SCROLLABLE);
        if (cfg->success) {
            lv_obj_set_style_bg_color(title_container, lv_color_hex(GREEN_MAIN), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(title_container, lv_color_hex(RED_MAIN), LV_PART_MAIN);
        }

        lv_obj_t *title = lv_label_create(title_container);
        lv_label_set_text(title, cfg->title);
        lv_obj_set_style_text_font(title, &white_rabbit_22, LV_PART_MAIN);
        lv_obj_set_style_text_color(title, lv_color_hex(WHITE), LV_PART_MAIN);
    }

    lv_obj_t *msg_container = lv_obj_create(panel);
    lv_obj_set_flex_flow(msg_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(msg_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_flex_grow(msg_container, 1);
    lv_obj_set_width(msg_container, lv_pct(100));
    lv_obj_set_style_pad_all(msg_container, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(msg_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(msg_container, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(msg_container, 0, LV_PART_MAIN);
    if (cfg->message) {
        lv_obj_t *msg = lv_label_create(msg_container);
        lv_label_set_text(msg, cfg->message);
        lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(msg, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_set_style_text_color(msg, lv_color_hex(WHITE), LV_PART_MAIN);
        lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_width(msg, lv_pct(100));
    }

    // Button row
    lv_obj_t *btn_row = lv_obj_create(panel);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(btn_row, 0);
    lv_obj_remove_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(btn_row, lv_pct(100));
    lv_obj_set_height(btn_row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(btn_row, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(btn_row, 15, LV_PART_MAIN);

    for (int i = 0; i < 2; i++) {
        const char *label = cfg->buttons[i];
        if (!label) {
            continue;
        }
        lv_obj_t *btn = lv_button_create(btn_row);
        lv_obj_add_style(btn, &button_style, LV_PART_MAIN);
        if (cfg->success) {
            lv_obj_add_style(btn, &button_green, LV_PART_MAIN);
        } else if (i == 0 && cfg->buttons[1]) {
            lv_obj_add_style(btn, &button_red, LV_PART_MAIN);
        } else {
            lv_obj_add_style(btn, &button_bluegrey, LV_PART_MAIN);
        }
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label);
        lv_obj_add_event_cb(btn, modal_button_event_cb, LV_EVENT_CLICKED, overlay);
        nav_register(ctx->nav_ctx, btn);
    }

    nav_autolink(ctx->nav_ctx);
    // Focus first button
    lv_obj_t *first_btn = lv_obj_get_child(btn_row, 0);
    if (first_btn) {
        nav_focus(ctx->nav_ctx, first_btn);
    }

    ctx->cb        = cfg->callback;
    ctx->user_data = (void *)cfg->user_data;

    return overlay;
}
