#include "registration.h"

#include <string.h>
#include "lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "badge.h"
#include "input.h"
#include "widgets/text_input.h"
#include "widgets/modal.h"
#include "nav.h"
#include "theme.h"
#include "api.h"

static const char *TAG = "ui/registration";

// -------------------------------------------------------------------------------------------------
// Internal State
// -------------------------------------------------------------------------------------------------
static bool modal_opened = false; // Prevent duplicate openings while unregistered

typedef struct {
    nav_scope_guard_t nav_guard;
    lv_indev_t *keypad;
    nav_ctx_t *nav_ctx;
    lv_obj_t *overlay;
    lv_obj_t *panel;
    lv_obj_t *register_btn;
    lv_obj_t *text_input_obj; // root returned by text_input_create
} reg_ctx_t;

static void destroy_ctx(reg_ctx_t *ctx) {
    if (!ctx) {
        return;
    }
    if (ctx->keypad) {
        nav_scope_pop(ctx->keypad, ctx->nav_guard);
    }
    if (ctx->nav_ctx) {
        nav_ctx_destroy(ctx->nav_ctx);
    }
    lv_free(ctx);

    // Reset flag to allow modal to be opened again
    modal_opened = false;
}

// -------------------------------------------------------------------------------------------------
// Event / Callbacks
// -------------------------------------------------------------------------------------------------
static void api_register_task(void *arg) {
    char *player_name = (char *)arg;
    if (!player_name) {
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Registering player name with API: %s", player_name);
    api_result_t *result = api_register(player_name);
    if (result == NULL) {
        ESP_LOGE(TAG, "Failed to register player name with API");
    } else if (!result->ok) {
        ESP_LOGE(TAG, "API registration failed: %s", result->detail ? result->detail : "Unknown error");
    } else {
        ESP_LOGI(TAG, "Successfully registered player name with API");
    }
    api_free_result(result, true);
    free(player_name);
    vTaskDelete(NULL);
}

static void text_changed_cb(const char *text, void *user_data) {
    reg_ctx_t *ctx = (reg_ctx_t *)user_data;
    if (!ctx || !ctx->register_btn) {
        return;
    }
    bool enable = (text && text[0] != '\0');
    if (enable) {
        lv_obj_clear_state(ctx->register_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(ctx->register_btn, LV_STATE_DISABLED);
    }
}

static void register_btn_event_cb(lv_event_t *e) {
    reg_ctx_t *ctx = (reg_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) {
        return;
    }
    char *name = text_input_get_text(ctx->text_input_obj);
    if (!name || name[0] == '\0') {
        return;
    }

    // Use set_badge_handle to update and persist the player name
    esp_err_t err = set_badge_handle(name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save badge handle: %s", esp_err_to_name(err));
        modal_message_config_t cfg = {
            .title     = "Error",
            .message   = "Failed to save name",
            .buttons   = {"OK", NULL},
            .success   = false,
            .callback  = NULL,
            .user_data = NULL,
        };
        modal_message_open(&cfg);
        return;
    }

    ESP_LOGI(TAG, "Badge handle saved: %s", name);

    // Async API registration task (safe to call even if WiFi not connected)
    char *name_copy = strdup(name);
    if (name_copy) {
        BaseType_t task_created = xTaskCreate(api_register_task, "api_register", 8192, name_copy, 5, NULL);
        if (task_created != pdPASS) {
            ESP_LOGE(TAG, "Failed to create API registration task");
            free(name_copy);
        }
    } else {
        ESP_LOGE(TAG, "Failed to allocate memory for API registration");
    }

    // Close registration overlay
    if (ctx->overlay) {
        lv_obj_del(ctx->overlay);
    }
    destroy_ctx(ctx);
}

// -------------------------------------------------------------------------------------------------
// Modal Construction
// -------------------------------------------------------------------------------------------------
static void open_registration_modal(const char *title, const char *button_text) {
    if (modal_opened) {
        return;
    }
    modal_opened = true;

    // Overlay
    lv_obj_t *overlay = lv_obj_create(lv_scr_act());
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(overlay, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_obj_set_style_bg_color(overlay, lv_color_hex(BLACK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_pad_all(overlay, 15, LV_PART_MAIN);
    lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(overlay, 0, LV_PART_MAIN);

    reg_ctx_t *ctx = (reg_ctx_t *)lv_malloc(sizeof(reg_ctx_t));
    if (!ctx) {
        lv_obj_del(overlay);
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->overlay = overlay;
    lv_obj_set_user_data(overlay, ctx);

    // Navigation scope
    lv_indev_t *keypad = input_get_device();
    lv_group_t *group  = lv_group_create();
    ctx->keypad        = keypad;
    ctx->nav_guard     = nav_scope_push(keypad, group);
    ctx->nav_ctx       = nav_ctx_create(keypad, group, overlay);
    nav_ctx_set_wrap(ctx->nav_ctx, false, false);

    // Panel
    lv_obj_t *panel = lv_obj_create(overlay);
    ctx->panel      = panel;
    lv_obj_set_width(panel, lv_pct(100));
    lv_obj_set_height(panel, lv_pct(100));
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(panel, lv_color_hex(GRAY_SHADE_7), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(GRAY_SHADE_4), LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(panel, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_column(panel, 8, LV_PART_MAIN);

    // Title bar
    lv_obj_t *title_row = lv_obj_create(panel);
    lv_obj_set_width(title_row, lv_pct(100));
    lv_obj_set_height(title_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(title_row, lv_color_hex(BLUE_MAIN), LV_PART_MAIN);
    lv_obj_set_style_border_width(title_row, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(title_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(title_row, 10, LV_PART_MAIN);
    lv_obj_remove_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(title_row, 0);
    lv_obj_t *title_lbl = lv_label_create(title_row);
    lv_label_set_text(title_lbl, title ? title : "Registration");
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_lbl, &white_rabbit_22, LV_PART_MAIN);

    // Body
    lv_obj_t *body = lv_obj_create(panel);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(body, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(body, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(body, 10, LV_PART_MAIN);
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *instr = lv_label_create(body);
    lv_label_set_text(instr, "Choose your player name:");
    lv_obj_set_style_text_font(instr, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(instr, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_align(instr, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(instr, lv_pct(100));

    // Text input
    bool have_existing = (badge_config.player_name[0] != '\0');
    static text_input_config_t ti_cfg;
    memset(&ti_cfg, 0, sizeof(ti_cfg));
    ti_cfg.charset       = TI_COMBINED;
    ti_cfg.max_len       = 32;
    ti_cfg.mask          = false;
    ti_cfg.start_editing = !have_existing;

    const char *initial = have_existing ? badge_config.player_name : NULL;

    if ((ctx->text_input_obj = text_input_create(body, &ti_cfg, initial, text_changed_cb, ctx)) != NULL) {
        lv_obj_set_flex_align(ctx->text_input_obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        text_input_setup_nav(ctx->text_input_obj, ctx->nav_ctx);
        if (have_existing) {
            size_t len        = strnlen(badge_config.player_name, ti_cfg.max_len);
            uint8_t caret_idx = (len < ti_cfg.max_len) ? (uint8_t)len : (uint8_t)(ti_cfg.max_len - 1);
            text_input_set_caret(ctx->text_input_obj, caret_idx);
        }
    }

    // Button row
    lv_obj_t *btn_row = lv_obj_create(panel);
    lv_obj_set_width(btn_row, lv_pct(100));
    lv_obj_set_height(btn_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(btn_row, 0);
    lv_obj_set_style_pad_gap(btn_row, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(btn_row, 15, LV_PART_MAIN);
    lv_obj_remove_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    ctx->register_btn = lv_button_create(btn_row);
    lv_obj_add_style(ctx->register_btn, &button_style, LV_PART_MAIN);
    lv_obj_add_style(ctx->register_btn, &button_green, LV_PART_MAIN);
    lv_obj_add_state(ctx->register_btn, LV_STATE_DISABLED);
    lv_obj_t *lbl = lv_label_create(ctx->register_btn);
    lv_label_set_text(lbl, button_text ? button_text : "Register");
    lv_obj_add_event_cb(ctx->register_btn, register_btn_event_cb, LV_EVENT_CLICKED, ctx);
    nav_register(ctx->nav_ctx, ctx->register_btn);

    if (ctx->text_input_obj) {
        nav_autolink(ctx->nav_ctx);
        nav_bind_vertical(ctx->nav_ctx, ctx->text_input_obj, btn_row, true);
        nav_ctx_set_vert_fallback(ctx->nav_ctx, false);
        if (have_existing) {
            // Existing text means enable button immediately
            lv_obj_clear_state(ctx->register_btn, LV_STATE_DISABLED);
        }
        uint8_t focus_idx = 0;
        if (have_existing) {
            size_t len = strnlen(badge_config.player_name, ti_cfg.max_len);
            focus_idx  = (len < ti_cfg.max_len) ? (uint8_t)len : (uint8_t)(ti_cfg.max_len - 1);
        }
        text_input_focus_cell(ctx->text_input_obj, ctx->nav_ctx, focus_idx);
    }

    ESP_LOGI(TAG, "Opened registration modal title='%s' existing=%d", title ? title : "Registration", have_existing);
}

// -------------------------------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------------------------------
void check_registration() {
    if (modal_opened) {
        return;
    }
    // Only open if not registered yet
    if (badge_config.registered || badge_config.player_name[0] != '\0') {
        return;
    }
    open_registration_modal(NULL, NULL);
}

void open_player_name_modal(const char *title, const char *button_text) {
    if (modal_opened) {
        return;
    }
    open_registration_modal(title ? title : "Edit Handle", button_text ? button_text : "Save");
}
