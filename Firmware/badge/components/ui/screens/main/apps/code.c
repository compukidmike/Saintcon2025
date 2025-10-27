#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "api.h"
#include "display.h"
#include "badge/led_patterns.h"

#include "../content.h"
#include "code.h"
#include "modal.h"
#include "nav.h"
#include "redeem_code.h"
#include "text_input.h"
#include "theme.h"

static const char *TAG = "screens/main [apps/code]";

static lv_obj_t *text_input     = NULL;
static lv_obj_t *footer_row     = NULL;
static lv_obj_t *submit_btn     = NULL;
static lv_obj_t *main_container = NULL;

static void code_nav_setup(lv_obj_t *content_area, nav_ctx_t *nav_ctx, void *user_data);
static void code_input_callback(const char *text, void *user_data);
static void submit_callback(lv_event_t *e);

static void redeem_flow_cb(int button_idx, const api_result_t *result, void *user_data) {
    if (!result) {
        ESP_LOGE(TAG, "Redeem flow completed with no result (btn=%d)", button_idx);
        return;
    }
    if (result->ok) {
        ESP_LOGI(TAG, "Redeem success (btn=%d)", button_idx);
    } else {
        ESP_LOGW(TAG, "Redeem failed (btn=%d): %s", button_idx, result->detail ? result->detail : "(no detail)");
    }
}

static void code_input_callback(const char *text, void *user_data) {
    ESP_LOGD(TAG, "Code input changed: %s", text ? text : "(empty)");
}

static void submit_callback(lv_event_t *e) {
    char *code = text_input_get_text(text_input);
    if (code && strlen(code) == 5) {
        const char *p = code;
        while (*p) {
            if (!strchr(TI_BASE32_NC, *p)) {
                ESP_LOGD(TAG, "Invalid character found in code: %c", *p);
                modal_message_config_t mc = {.title     = "Invalid Code",
                                             .message   = "The code contains an invalid character.",
                                             .buttons   = {"OK", NULL},
                                             .success   = false,
                                             .callback  = NULL,
                                             .user_data = NULL};
                modal_message_open(&mc);
                return;
            }
            p++;
        }

        // Use shared redeem code flow
        esp_err_t err = try_redeem_code(code, 5, redeem_flow_cb, NULL);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "try_redeem_code failed: %d", (int)err);
        }
    } else {
        ESP_LOGD(TAG, "No code to submit");
        modal_message_config_t mc = {.title     = "Incomplete",
                                     .message   = "Enter a 5 character code first.",
                                     .buttons   = {"OK", NULL},
                                     .success   = false,
                                     .callback  = NULL,
                                     .user_data = NULL};
        modal_message_open(&mc);
    }
}

void code_app_create(lv_obj_t *parent) {
    ESP_LOGD(TAG, "Creating code entry app");

    content_register_nav_callback(APP_CODE, code_nav_setup, NULL);

    main_container = lv_obj_create(parent);
    lv_obj_set_size(main_container, lv_pct(95), lv_pct(95));
    lv_obj_center(main_container);
    lv_obj_set_flex_flow(main_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(main_container, 20, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(main_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(main_container, 0, LV_PART_MAIN);
    lv_obj_set_flex_align(main_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    text_input_config_t config = {
        .charset = TI_BASE32_NC,
        .max_len = 5,
        .mask    = false,
    };
    text_input = text_input_create(main_container, &config, NULL, code_input_callback, NULL);

    footer_row = lv_obj_create(main_container);
    lv_obj_set_size(footer_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(footer_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(footer_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(footer_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(footer_row, 12, LV_PART_MAIN);
    lv_obj_set_flex_align(footer_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(footer_row, LV_SCROLLBAR_MODE_OFF);

    submit_btn = lv_button_create(footer_row);
    lv_obj_add_style(submit_btn, &button_style, LV_PART_MAIN);
    lv_obj_add_style(submit_btn, &button_green, LV_PART_MAIN);
    lv_obj_set_style_text_font(submit_btn, &white_rabbit_18, LV_PART_MAIN);
    lv_obj_t *btn_label = lv_label_create(submit_btn);
    lv_label_set_text(btn_label, "Submit Code");
    lv_obj_add_event_cb(submit_btn, submit_callback, LV_EVENT_CLICKED, NULL);
}

static void code_nav_setup(lv_obj_t *content_area, nav_ctx_t *nav_ctx, void *user_data) {
    if (!text_input || !submit_btn) {
        ESP_LOGE(TAG, "Components not initialized");
        return;
    }

    text_input_setup_nav(text_input, nav_ctx);
    nav_register(nav_ctx, submit_btn);
    nav_autolink(nav_ctx);
    text_input_focus_cell(text_input, nav_ctx, 0);
    nav_bind_vertical(nav_ctx, text_input, footer_row, true);

    // Bind home button vertically after other relationships so lane memory tracks text_input cells.
    lv_obj_t *home_button = lv_obj_get_child_by_type(lv_obj_get_parent(content_area), 0, &lv_button_class);
    if (!home_button) {
        ESP_LOGE(TAG, "Failed to find home button");
    } else {
        nav_bind_vertical(nav_ctx, home_button, text_input, true);
    }

    nav_ctx_set_vert_fallback(nav_ctx, false);
}
