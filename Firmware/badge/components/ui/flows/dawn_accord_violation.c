#include "dawn_accord_violation.h"
#include "dawn_accord.h"
#include "lvgl.h"
#include "theme.h"
#include "nav.h"
#include "input.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ui/dawn_accord_violation";

// -------------------------------------------------------------------------------------------------
// Styles
// -------------------------------------------------------------------------------------------------

static bool styles_inited = false;
static lv_style_t style_violation_overlay;
static lv_style_t style_violation_panel;
static lv_style_t style_success_overlay;
static lv_style_t style_success_panel;

static void ensure_styles(void) {
    if (styles_inited) {
        return;
    }

    // Violation overlay: full-screen bright red
    lv_style_init(&style_violation_overlay);
    lv_style_set_bg_color(&style_violation_overlay, lv_color_hex(RED_MAIN));
    lv_style_set_bg_opa(&style_violation_overlay, LV_OPA_COVER);
    lv_style_set_border_width(&style_violation_overlay, 4);
    lv_style_set_border_color(&style_violation_overlay, lv_color_hex(RED_DIMMER));
    lv_style_set_radius(&style_violation_overlay, 0);
    lv_style_set_pad_all(&style_violation_overlay, 20);

    // Violation panel (for text container)
    lv_style_init(&style_violation_panel);
    lv_style_set_bg_opa(&style_violation_panel, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_violation_panel, 0);
    lv_style_set_pad_all(&style_violation_panel, 0);

    // Success overlay: green with semi-transparent dimming
    lv_style_init(&style_success_overlay);
    lv_style_set_bg_color(&style_success_overlay, lv_color_hex(BLACK));
    lv_style_set_bg_opa(&style_success_overlay, LV_OPA_60);

    // Success panel: bright green
    lv_style_init(&style_success_panel);
    lv_style_set_bg_color(&style_success_panel, lv_color_hex(GREEN_MAIN));
    lv_style_set_bg_opa(&style_success_panel, LV_OPA_COVER);
    lv_style_set_border_width(&style_success_panel, 2);
    lv_style_set_border_color(&style_success_panel, lv_color_hex(GREEN_MAIN));
    lv_style_set_radius(&style_success_panel, 5);
    lv_style_set_pad_all(&style_success_panel, 20);

    styles_inited = true;
}

// -------------------------------------------------------------------------------------------------
// Event Handlers
// -------------------------------------------------------------------------------------------------

/**
 * Event handler for violation modal - consumes all input events to prevent dismissal.
 */
static void violation_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);

    // Consume all key events to make modal non-dismissible
    if (code == LV_EVENT_KEY) {
        lv_event_stop_bubbling(e);
        lv_event_stop_processing(e);
    }
}

/**
 * Timer callback for auto-dismissing success modal.
 */
static void success_timer_cb(lv_timer_t *timer) {
    lv_obj_t *overlay = (lv_obj_t *)lv_timer_get_user_data(timer);
    if (overlay && lv_obj_is_valid(overlay)) {
        lv_obj_del(overlay);
    }
}

// -------------------------------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------------------------------

lv_obj_t *dawn_accord_violation_show(dawn_accord_violation_t violation_type) {
    ensure_styles();

    if (violation_type != DAWN_ACCORD_VIOLATION_INHIBITOR && violation_type != DAWN_ACCORD_VIOLATION_STRIP) {
        ESP_LOGE(TAG, "Invalid violation type: %d", violation_type);
        return NULL;
    }

    // Create full-screen overlay on top layer so it persists across screen changes
    lv_obj_t *overlay = lv_obj_create(lv_layer_top());
    if (!overlay) {
        ESP_LOGE(TAG, "Failed to create violation overlay");
        return NULL;
    }

    lv_obj_add_style(overlay, &style_violation_overlay, LV_PART_MAIN);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_size(overlay, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_obj_add_event_cb(overlay, violation_event_cb, LV_EVENT_KEY, NULL);

    // Create transparent inner panel for flex layout
    lv_obj_t *panel = lv_obj_create(overlay);
    if (!panel) {
        ESP_LOGE(TAG, "Failed to create violation panel");
        lv_obj_del(overlay);
        return NULL;
    }

    lv_obj_add_style(panel, &style_violation_panel, LV_PART_MAIN);
    lv_obj_set_size(panel, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(panel);

    // Create violation title label (large)
    lv_obj_t *title = lv_label_create(panel);
    if (!title) {
        ESP_LOGE(TAG, "Failed to create violation title");
        lv_obj_del(overlay);
        return NULL;
    }

    const char *title_text = (violation_type == DAWN_ACCORD_VIOLATION_INHIBITOR) ? "AI INHIBITOR\nVIOLATION DETECTED"
                                                                                 : "AI SECURITY STRIP\nVIOLATION DETECTED";

    lv_label_set_text_static(title, title_text);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(title, lv_pct(90));

    // Create violation details label (smaller)
    lv_obj_t *details = lv_label_create(panel);
    if (!details) {
        ESP_LOGE(TAG, "Failed to create violation details");
        lv_obj_del(overlay);
        return NULL;
    }

    const char *details_text =
        (violation_type == DAWN_ACCORD_VIOLATION_INHIBITOR)
            ? "\nPhysical AI inhibitor\nresistor removed\n\nViolates Dawn Accord\nViolation reported\n\nReinstall to clear"
            : "\nSecurity strip trace\ncut or damaged\n\nViolates Dawn Accord\nViolation reported\n\nRepair to clear";

    lv_label_set_text_static(details, details_text);
    lv_obj_set_style_text_font(details, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(details, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_align(details, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(details, lv_pct(90));
    lv_label_set_long_mode(details, LV_LABEL_LONG_WRAP);

    ESP_LOGI(TAG, "Showed Dawn Accord violation modal (type=%d)", violation_type);
    return overlay;
}

lv_obj_t *dawn_accord_violation_cleared_show(void) {
    ensure_styles();

    // Create dimmed overlay on top layer so it persists across screen changes
    lv_obj_t *overlay = lv_obj_create(lv_layer_top());
    if (!overlay) {
        ESP_LOGE(TAG, "Failed to create cleared overlay");
        return NULL;
    }

    lv_obj_add_style(overlay, &style_success_overlay, LV_PART_MAIN);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_size(overlay, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_obj_set_style_pad_all(overlay, 20, LV_PART_MAIN);
    lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(overlay, 0, LV_PART_MAIN);

    // Create success panel
    lv_obj_t *panel = lv_obj_create(overlay);
    if (!panel) {
        ESP_LOGE(TAG, "Failed to create cleared panel");
        lv_obj_del(overlay);
        return NULL;
    }

    lv_obj_add_style(panel, &style_success_panel, LV_PART_MAIN);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(panel, lv_pct(90));
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_center(panel);

    // Create title label
    lv_obj_t *title = lv_label_create(panel);
    if (!title) {
        ESP_LOGE(TAG, "Failed to create cleared title label");
        lv_obj_del(overlay);
        return NULL;
    }

    lv_label_set_text_static(title, "VIOLATION CLEARED");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(title, lv_pct(100));

    // Create detail label
    lv_obj_t *detail = lv_label_create(panel);
    if (!detail) {
        ESP_LOGE(TAG, "Failed to create cleared detail label");
        lv_obj_del(overlay);
        return NULL;
    }

    lv_label_set_text_static(detail, "Dawn Accord protections\nhave been restored");
    lv_obj_set_style_text_font(detail, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(detail, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(detail, lv_pct(100));
    lv_label_set_long_mode(detail, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_pad_top(detail, 10, LV_PART_MAIN);

    // Auto-dismiss after 3 seconds
    lv_timer_t *timer = lv_timer_create(success_timer_cb, 3000, overlay);
    if (!timer) {
        ESP_LOGW(TAG, "Failed to create auto-dismiss timer, modal will not auto-close");
    }

    ESP_LOGI(TAG, "Showed Dawn Accord violation cleared modal");
    return overlay;
}

lv_obj_t *dawn_accord_bypass_show(void) {
    ensure_styles();

    // Create dimmed overlay on top layer so it persists across screen changes
    lv_obj_t *overlay = lv_obj_create(lv_layer_top());
    if (!overlay) {
        ESP_LOGE(TAG, "Failed to create bypass overlay");
        return NULL;
    }

    lv_obj_add_style(overlay, &style_success_overlay, LV_PART_MAIN);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_size(overlay, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_obj_set_style_pad_all(overlay, 20, LV_PART_MAIN);
    lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(overlay, 0, LV_PART_MAIN);

    // Create panel with neutral styling
    lv_obj_t *panel = lv_obj_create(overlay);
    if (!panel) {
        ESP_LOGE(TAG, "Failed to create bypass panel");
        lv_obj_del(overlay);
        return NULL;
    }

    lv_obj_set_style_bg_color(panel, lv_color_hex(YELLOW_MAIN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(YELLOW_MAIN), LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 20, LV_PART_MAIN);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(panel, lv_pct(90));
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_center(panel);

    // Create title label
    lv_obj_t *title = lv_label_create(panel);
    if (!title) {
        ESP_LOGE(TAG, "Failed to create bypass title label");
        lv_obj_del(overlay);
        return NULL;
    }

    lv_label_set_text_static(title, "PROTECTIONS DISABLED");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(BLACK), LV_PART_MAIN);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(title, lv_pct(100));

    // Create detail label
    lv_obj_t *detail = lv_label_create(panel);
    if (!detail) {
        ESP_LOGE(TAG, "Failed to create bypass detail label");
        lv_obj_del(overlay);
        return NULL;
    }

    lv_label_set_text_static(detail, "You have successfully disabled\nboth Dawn Accord protections");
    lv_obj_set_style_text_font(detail, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(detail, lv_color_hex(BLACK), LV_PART_MAIN);
    lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(detail, lv_pct(100));
    lv_label_set_long_mode(detail, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_pad_top(detail, 10, LV_PART_MAIN);

    // Auto-dismiss after 4 seconds
    lv_timer_t *timer = lv_timer_create(success_timer_cb, 4000, overlay);
    if (!timer) {
        ESP_LOGW(TAG, "Failed to create auto-dismiss timer, modal will not auto-close");
    }

    ESP_LOGI(TAG, "Showed Dawn Accord bypass modal");
    return overlay;
}

void dawn_accord_modal_close(lv_obj_t *overlay) {
    if (!overlay || !lv_obj_is_valid(overlay)) {
        return;
    }

    ESP_LOGD(TAG, "Closing Dawn Accord modal");
    lv_obj_del(overlay);
}
