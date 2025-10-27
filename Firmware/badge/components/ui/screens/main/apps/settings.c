#include "settings.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "badge.h"
#include "badge/led_patterns.h"
#include "badge_game.h"
#include "display.h"
#include "led.h"
#include "nav.h"
#include "theme.h"
#include "ui.h"
#include "version.h"
#include "registration.h"

#include "../content.h"

static const char *TAG = "screens/main [apps/settings]";

// -------------------------------------------------------------------------------------------------
// Internal State
// -------------------------------------------------------------------------------------------------
static nav_ctx_t *settings_nav_ctx  = NULL;
static lv_obj_t *settings_list      = NULL;
static lv_obj_t *brightness_slider  = NULL;
static lv_obj_t *brightness_label   = NULL;
static lv_obj_t *timeout_slider     = NULL;
static lv_obj_t *timeout_label      = NULL;
static lv_obj_t *faction_switch     = NULL;
static lv_obj_t *handle_value_label = NULL;

typedef struct {
    int16_t last;
    int16_t staged;
    bool has_staged;
    bool internal;
} slider_guard_t;

typedef struct {
    bool last;
    bool internal;
} switch_guard_t;

static inline bool nav_group_is_editing(void) {
    return settings_nav_ctx && settings_nav_ctx->group && lv_group_get_editing(settings_nav_ctx->group);
}

// Timeout values in seconds
static const uint32_t timeout_values[] = {30, 60, 90, 120};
static const size_t timeout_count      = sizeof(timeout_values) / sizeof(timeout_values[0]);

// Brightness mapping: 1-100 percentage to actual brightness values
#define BRIGHTNESS_MIN_PERCENT 1
#define BRIGHTNESS_MAX_PERCENT 100
#define BRIGHTNESS_MIN_VALUE   LCD_BACKLIGHT_ON / 3 // Minimum actual brightness
#define BRIGHTNESS_MAX_VALUE   255

static int32_t percent_to_brightness(int32_t percent) {
    if (percent < BRIGHTNESS_MIN_PERCENT) {
        percent = BRIGHTNESS_MIN_PERCENT;
    }
    if (percent > BRIGHTNESS_MAX_PERCENT) {
        percent = BRIGHTNESS_MAX_PERCENT;
    }

    return BRIGHTNESS_MIN_VALUE + ((percent - BRIGHTNESS_MIN_PERCENT) * (BRIGHTNESS_MAX_VALUE - BRIGHTNESS_MIN_VALUE)) /
                                      (BRIGHTNESS_MAX_PERCENT - BRIGHTNESS_MIN_PERCENT);
}

static int32_t brightness_to_percent(int32_t value) {
    if (value < BRIGHTNESS_MIN_VALUE) {
        value = BRIGHTNESS_MIN_VALUE;
    }
    if (value > BRIGHTNESS_MAX_VALUE) {
        value = BRIGHTNESS_MAX_VALUE;
    }

    return BRIGHTNESS_MIN_PERCENT + ((value - BRIGHTNESS_MIN_VALUE) * (BRIGHTNESS_MAX_PERCENT - BRIGHTNESS_MIN_PERCENT)) /
                                        (BRIGHTNESS_MAX_VALUE - BRIGHTNESS_MIN_VALUE);
}

// -------------------------------------------------------------------------------------------------
// Forward Declarations
// -------------------------------------------------------------------------------------------------
static void settings_nav_setup(lv_obj_t *content_area, nav_ctx_t *nav_ctx, void *user_data);
static void handle_click_event_cb(lv_event_t *e);
static void brightness_event_cb(lv_event_t *e);
static void timeout_event_cb(lv_event_t *e);
static void faction_switch_event_cb(lv_event_t *e);

// -------------------------------------------------------------------------------------------------
// Helper: Format Timeout Display
// -------------------------------------------------------------------------------------------------
static void format_timeout_string(char *buf, size_t buf_size, uint32_t seconds) {
    if (seconds < 60) {
        snprintf(buf, buf_size, "%lu sec", (unsigned long)seconds);
    } else {
        uint32_t minutes           = seconds / 60;
        uint32_t remaining_seconds = seconds % 60;
        if (remaining_seconds == 0) {
            snprintf(buf, buf_size, "%lu min", (unsigned long)minutes);
        } else {
            snprintf(buf, buf_size, "%lu min %lu sec", (unsigned long)minutes, (unsigned long)remaining_seconds);
        }
    }
}

// -------------------------------------------------------------------------------------------------
// Helper: Create Setting Row
// -------------------------------------------------------------------------------------------------
static lv_obj_t *create_setting_row(lv_obj_t *parent, const char *label_text) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(row, 6, LV_PART_MAIN);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_color(label, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &white_rabbit_18, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(label, 4, LV_PART_MAIN);

    return row;
}

// -------------------------------------------------------------------------------------------------
// Event Callbacks
// -------------------------------------------------------------------------------------------------

static void scroll_to_focused_cb(lv_event_t *e) {
    lv_obj_t *target     = lv_event_get_target(e);
    lv_obj_t *alt_object = lv_event_get_user_data(e);
    if (settings_list && lv_obj_is_valid(settings_list)) {
        lv_obj_scroll_to_view_recursive(alt_object ? alt_object : target, LV_ANIM_ON);
    }
}

static void handle_click_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Player name clicked, opening edit modal");
        open_player_name_modal("Edit Handle", "Save");
    }
}

static void brightness_event_cb(lv_event_t *e) {
    lv_obj_t *slider      = lv_event_get_target(e);
    slider_guard_t *guard = (slider_guard_t *)lv_obj_get_user_data(slider);
    lv_event_code_t code  = lv_event_get_code(e);

    if (!guard) {
        ESP_LOGW(TAG, "Brightness slider event with no guard data!");
        return;
    }

    if (code == LV_EVENT_FOCUSED) {
        if (guard) {
            guard->last       = lv_slider_get_value(slider);
            guard->has_staged = false;
            return;
        }
    }

    if (code == LV_EVENT_VALUE_CHANGED) {
        if (guard->has_staged) {
            return;
        }

        int32_t percent   = lv_slider_get_value(slider);
        guard->staged     = percent;
        guard->has_staged = true;

        guard->internal = true;
        lv_slider_set_value(slider, guard->last, LV_ANIM_OFF);
        guard->internal = false;

        ESP_LOGD(TAG, "Brightness slider value changed, reverting to last=%d for now, staged=%d", guard ? guard->last : -1,
                 guard ? guard->staged : -1);
        return;
    }

    if (code == LV_EVENT_KEY) {
        int32_t key = lv_event_get_key(e);
        if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
            if (guard && guard->internal) {
                return;
            }

            int32_t percent = guard && guard->has_staged ? guard->staged : lv_slider_get_value(slider);
            if (lv_slider_get_value(slider) != percent) {
                guard->internal = true;
                lv_slider_set_value(slider, percent, LV_ANIM_OFF);
                guard->internal = false;
            }

            int32_t value                  = percent_to_brightness(percent);
            guard->last                    = percent;
            guard->has_staged              = false;
            badge_config.screen_brightness = (uint8_t)value;
            save_badge_config();
            set_backlight((uint8_t)value);
            lv_label_set_text_fmt(brightness_label, "%ld%%", percent);
            ESP_LOGD(TAG, "Brightness committed via %s to: %ld%% (%ld actual)", (key == LV_KEY_LEFT ? "LEFT" : "RIGHT"), percent,
                     value);
            return;
        }
        if (key == LV_KEY_UP || key == LV_KEY_DOWN) {
            ESP_LOGD(TAG, "Brightness slider UP/DOWN pressed, stopping event processing");
            if (lv_group_get_editing(settings_nav_ctx->group)) {
                // Make sure we set the slider value to guard->last
                if (lv_slider_get_value(slider) != guard->last) {
                    guard->staged     = guard->last;
                    guard->has_staged = false;
                    guard->internal   = true;
                    lv_slider_set_value(slider, guard->last, LV_ANIM_OFF);
                    guard->internal = false;
                    lv_event_stop_processing(e);
                }
            }
            return;
        }
    }
}

static void settings_app_destroy() {
    if (brightness_slider && lv_obj_is_valid(brightness_slider)) {
        slider_guard_t *guard = (slider_guard_t *)lv_obj_get_user_data(brightness_slider);
        if (guard) {
            free(guard);
            lv_obj_set_user_data(brightness_slider, NULL);
        }
    }
    if (timeout_slider && lv_obj_is_valid(timeout_slider)) {
        slider_guard_t *guard = (slider_guard_t *)lv_obj_get_user_data(timeout_slider);
        if (guard) {
            free(guard);
            lv_obj_set_user_data(timeout_slider, NULL);
        }
    }
    if (faction_switch && lv_obj_is_valid(faction_switch)) {
        switch_guard_t *guard = (switch_guard_t *)lv_obj_get_user_data(faction_switch);
        if (guard) {
            free(guard);
            lv_obj_set_user_data(faction_switch, NULL);
        }
    }
}

static void timeout_event_cb(lv_event_t *e) {
    lv_obj_t *slider      = lv_event_get_target(e);
    slider_guard_t *guard = (slider_guard_t *)lv_obj_get_user_data(slider);
    lv_event_code_t code  = lv_event_get_code(e);

    if (!guard) {
        ESP_LOGW(TAG, "Timeout slider event with no guard data!");
        return;
    }

    if (code == LV_EVENT_FOCUSED) {
        guard->last       = lv_slider_get_value(slider);
        guard->has_staged = false;
        return;
    }

    if (code == LV_EVENT_VALUE_CHANGED) {
        if (guard->has_staged) {
            return;
        }

        int32_t index     = lv_slider_get_value(slider);
        guard->staged     = index;
        guard->has_staged = true;

        guard->internal = true;
        lv_slider_set_value(slider, guard->last, LV_ANIM_OFF);
        guard->internal = false;

        ESP_LOGD(TAG, "Timeout slider value changed, reverting to last=%d for now, staged=%d", guard->last, guard->staged);
        return;
    }

    if (code == LV_EVENT_KEY) {
        int32_t key = lv_event_get_key(e);
        if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
            if (guard->internal) {
                return;
            }

            int32_t index = guard->has_staged ? guard->staged : lv_slider_get_value(slider);
            if (lv_slider_get_value(slider) != index) {
                guard->internal = true;
                lv_slider_set_value(slider, index, LV_ANIM_OFF);
                guard->internal = false;
            }

            guard->last                 = index;
            guard->has_staged           = false;
            badge_config.screen_timeout = timeout_values[index];
            save_badge_config();
            set_screen_timeout(badge_config.screen_timeout);

            char buf[32];
            format_timeout_string(buf, sizeof(buf), timeout_values[index]);
            lv_label_set_text(timeout_label, buf);
            ESP_LOGD(TAG, "Timeout committed via %s to: %s (%ld seconds)", (key == LV_KEY_LEFT ? "LEFT" : "RIGHT"), buf,
                     timeout_values[index]);
            return;
        }
        if (key == LV_KEY_UP || key == LV_KEY_DOWN) {
            ESP_LOGD(TAG, "Timeout slider UP/DOWN pressed, stopping event processing");
            if (lv_group_get_editing(settings_nav_ctx->group)) {
                if (lv_slider_get_value(slider) != guard->last) {
                    guard->staged     = guard->last;
                    guard->has_staged = false;
                    guard->internal   = true;
                    lv_slider_set_value(slider, guard->last, LV_ANIM_OFF);
                    guard->internal = false;
                    lv_event_stop_processing(e);
                }
            }
            return;
        }
    }
}

static void faction_switch_event_cb(lv_event_t *e) {
    lv_obj_t *sw          = lv_event_get_target(e);
    switch_guard_t *guard = (switch_guard_t *)lv_obj_get_user_data(sw);
    lv_event_code_t code  = lv_event_get_code(e);

    if (!guard) {
        ESP_LOGW(TAG, "Faction switch event with no guard data!");
        return;
    }

    // // PREPROCESS runs before LVGL's internal handlers - intercept keys here
    // if (code == LV_EVENT_PREPROCESS) {
    //     lv_indev_t *indev = lv_indev_active();
    //     if (indev && lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER) {
    //         uint32_t key = lv_indev_get_key(indev);

    //         // Block left/right keys completely to prevent unwanted navigation
    //         if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
    //             ESP_LOGD(TAG, "Faction switch LEFT/RIGHT in preprocess, stopping");
    //             lv_indev_wait_release(indev);
    //             return;
    //         }
    //     }
    //     return;
    // }

    // PREPROCESS runs before LVGL's internal handlers
    if (code == LV_EVENT_PREPROCESS) {
        lv_indev_t *indev = lv_indev_active();
        if (indev && lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER) {
            uint32_t key = lv_indev_get_key(indev);

            // Block LEFT/RIGHT so LVGL never toggles from them
            if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
                ESP_LOGD(TAG, "Faction switch LEFT/RIGHT in preprocess, stopping");
                // Prevent both default handling and bubbling to nav
                lv_event_stop_bubbling(e);
                lv_event_stop_processing(e);
                lv_indev_wait_release(indev);
                return;
            }
        }
        return;
    }

    if (code == LV_EVENT_FOCUSED) {
        guard->last = lv_obj_has_state(sw, LV_STATE_CHECKED);
        return;
    }

    if (code == LV_EVENT_VALUE_CHANGED) {
        if (guard->internal) {
            return;
        }

        bool current = lv_obj_has_state(sw, LV_STATE_CHECKED);

        guard->internal = true;
        // Disable animations while reverting to prevent visible toggle
        uint32_t anim_time = lv_obj_get_style_anim_time(sw, LV_PART_MAIN);
        uint32_t anim_ind  = lv_obj_get_style_anim_time(sw, LV_PART_INDICATOR);
        uint32_t anim_knob = lv_obj_get_style_anim_time(sw, LV_PART_KNOB);
        lv_obj_set_style_anim_time(sw, 0, LV_PART_MAIN);
        lv_obj_set_style_anim_time(sw, 0, LV_PART_INDICATOR);
        lv_obj_set_style_anim_time(sw, 0, LV_PART_KNOB);

        if (current != guard->last) {
            if (guard->last) {
                lv_obj_add_state(sw, LV_STATE_CHECKED);
            } else {
                lv_obj_remove_state(sw, LV_STATE_CHECKED);
            }
        }

        // Restore animation time
        lv_obj_set_style_anim_time(sw, anim_time, LV_PART_MAIN);
        lv_obj_set_style_anim_time(sw, anim_ind, LV_PART_INDICATOR);
        lv_obj_set_style_anim_time(sw, anim_knob, LV_PART_KNOB);
        guard->internal = false;

        ESP_LOGD(TAG, "Faction switch value changed, reverting to last=%d for now", guard->last);
        return;
    }

    if (code == LV_EVENT_KEY) {
        int32_t key = lv_event_get_key(e);

        if (key == LV_KEY_ENTER) {
            if (guard->internal) {
                return;
            }

            // Stop event to prevent LVGL's default toggle
            lv_event_stop_bubbling(e);
            lv_event_stop_processing(e);

            bool new_state = !guard->last;

            // Validate requirements if trying to enable
            if (new_state) {
                bool faction_requirements_met = badge_config.team_id <= NUM_FACTIONS && badge_config.sorting_hat;
                if (!faction_requirements_met) {
                    ESP_LOGW(TAG, "Cannot enable faction LEDs: requirements not met (team_id=%d, sorting_hat=%d)",
                             badge_config.team_id, badge_config.sorting_hat);
                    return;
                }
            }

            guard->last = new_state;

            guard->internal = true;
            // Disable animation temporarily
            uint32_t anim_time = lv_obj_get_style_anim_time(sw, LV_PART_MAIN);
            lv_obj_set_style_anim_time(sw, 0, LV_PART_MAIN);

            if (new_state) {
                lv_obj_add_state(sw, LV_STATE_CHECKED);
            } else {
                lv_obj_remove_state(sw, LV_STATE_CHECKED);
            }

            lv_obj_set_style_anim_time(sw, anim_time, LV_PART_MAIN);
            guard->internal = false;

            badge_config.faction_leds = new_state;
            save_badge_config();

            ESP_LOGI(TAG, "Faction Indicator %s via ENTER", new_state ? "enabled" : "disabled");

            // Start or stop the faction LED pattern
            if (new_state) {
                led_pattern_faction_start();
            } else {
                led_pattern_faction_stop();
            }
            return;
        }
        if (key == LV_KEY_UP || key == LV_KEY_DOWN) {
            ESP_LOGD(TAG, "Faction switch UP/DOWN pressed, stopping event processing");
            if (lv_group_get_editing(settings_nav_ctx->group)) {
                bool current = lv_obj_has_state(sw, LV_STATE_CHECKED);
                if (current != guard->last) {
                    guard->internal = true;
                    // Disable animations while reverting
                    uint32_t anim_time = lv_obj_get_style_anim_time(sw, LV_PART_MAIN);
                    lv_obj_set_style_anim_time(sw, 0, LV_PART_MAIN);

                    if (guard->last) {
                        lv_obj_add_state(sw, LV_STATE_CHECKED);
                    } else {
                        lv_obj_remove_state(sw, LV_STATE_CHECKED);
                    }

                    lv_obj_set_style_anim_time(sw, anim_time, LV_PART_MAIN);
                    guard->internal = false;
                    lv_event_stop_processing(e);
                }
            }
            return;
        }
    }
}

// -------------------------------------------------------------------------------------------------
// Main App Creation
// -------------------------------------------------------------------------------------------------
void settings_app_create(lv_obj_t *parent) {
    content_register_nav_callback(APP_SETTINGS, settings_nav_setup, NULL);

    // Main container
    lv_obj_t *main_container = lv_obj_create(parent);
    lv_obj_set_size(main_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(main_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(main_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_top(main_container, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_left(main_container, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_right(main_container, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(main_container, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(main_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_remove_flag(main_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(main_container, settings_app_destroy, LV_EVENT_DELETE, NULL);

    // Scrollable settings list
    settings_list = lv_obj_create(main_container);
    lv_obj_set_width(settings_list, lv_pct(100));
    lv_obj_set_flex_grow(settings_list, 1);
    lv_obj_set_style_bg_color(settings_list, lv_color_hex(GRAY_SHADE_5), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(settings_list, LV_OPA_50, LV_PART_MAIN);
    // lv_obj_set_style_border_width(settings_list, 2, LV_PART_MAIN);
    // lv_obj_set_style_border_color(settings_list, lv_color_hex(MENU_BUTTON_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(settings_list, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(settings_list, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_all(settings_list, 8, LV_PART_MAIN);
    lv_obj_set_flex_flow(settings_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(settings_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(settings_list, 8, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(settings_list, LV_SCROLLBAR_MODE_AUTO);

    // -------------------------------------------------------------------------------------------------
    // Setting 1: Player Name
    // -------------------------------------------------------------------------------------------------
    lv_obj_t *handle_row = create_setting_row(settings_list, "Player Name");

    handle_value_label = lv_label_create(handle_row);
    if (badge_config.player_name[0] != '\0') {
        lv_label_set_text(handle_value_label, badge_config.player_name);
    } else {
        lv_label_set_text(handle_value_label, "Not Set");
    }
    lv_label_set_long_mode(handle_value_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_size(handle_value_label, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(handle_value_label, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(handle_value_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_bg_color(handle_value_label, lv_color_hex(GRAY_SHADE_6), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(handle_value_label, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_border_width(handle_value_label, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(handle_value_label, lv_color_hex(GRAY_SHADE_4), LV_PART_MAIN);
    lv_obj_set_style_border_opa(handle_value_label, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_radius(handle_value_label, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_all(handle_value_label, 8, LV_PART_MAIN);
    lv_obj_add_flag(handle_value_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(handle_value_label, handle_click_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(handle_value_label, scroll_to_focused_cb, LV_EVENT_FOCUSED, handle_row);

    // Focus styling
    lv_obj_set_style_outline_width(handle_value_label, 4, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_color(handle_value_label, lv_color_hex(BLUE_MAIN), LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_pad(handle_value_label, 3, LV_PART_MAIN | LV_STATE_FOCUS_KEY);

    // -------------------------------------------------------------------------------------------------
    // Setting 2: Screen Brightness
    // -------------------------------------------------------------------------------------------------
    lv_obj_t *brightness_row = create_setting_row(settings_list, "Brightness");

    brightness_slider = lv_slider_create(brightness_row);
    lv_obj_set_size(brightness_slider, lv_pct(100), 16);
    lv_slider_set_range(brightness_slider, BRIGHTNESS_MIN_PERCENT, BRIGHTNESS_MAX_PERCENT);
    int32_t current_percent = brightness_to_percent(badge_config.screen_brightness);
    lv_slider_set_value(brightness_slider, current_percent, LV_ANIM_OFF);
    lv_obj_remove_flag(brightness_slider, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(brightness_slider, LV_OBJ_FLAG_CLICKABLE);

    // Attach guard data
    slider_guard_t *brightness_guard = calloc(1, sizeof(slider_guard_t));
    brightness_guard->last           = current_percent;
    brightness_guard->staged         = 0;
    brightness_guard->has_staged     = false;
    brightness_guard->internal       = false;
    lv_obj_set_user_data(brightness_slider, brightness_guard);

    // Slider styling - thinner bar, larger knob
    // The main part is the background track
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(GRAY_SHADE_2), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(brightness_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(brightness_slider, 3, LV_PART_MAIN);

    // The indicator is the filled portion
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(BLUE_MAIN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(brightness_slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(brightness_slider, 3, LV_PART_INDICATOR);

    // The knob sits on top
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(WHITE), LV_PART_KNOB);
    lv_obj_set_style_pad_all(brightness_slider, 6, LV_PART_KNOB);
    lv_obj_set_style_radius(brightness_slider, 6, LV_PART_KNOB);

    // Focus styling
    lv_obj_set_style_outline_width(brightness_slider, 4, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_color(brightness_slider, lv_color_hex(BLUE_MAIN), LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_pad(brightness_slider, 3, LV_PART_MAIN | LV_STATE_FOCUS_KEY);

    lv_obj_add_event_cb(brightness_slider, brightness_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(brightness_slider, brightness_event_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(brightness_slider, brightness_event_cb, LV_EVENT_FOCUSED, NULL);

    brightness_label = lv_label_create(brightness_row);
    lv_label_set_text_fmt(brightness_label, "%ld%%", current_percent);
    lv_obj_set_style_text_color(brightness_label, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(brightness_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_top(brightness_label, 4, LV_PART_MAIN);
    lv_obj_set_width(brightness_label, lv_pct(100));

    // -------------------------------------------------------------------------------------------------
    // Setting 3: Screen Timeout
    // -------------------------------------------------------------------------------------------------
    lv_obj_t *timeout_row = create_setting_row(settings_list, "Screen Timeout");

    timeout_slider = lv_slider_create(timeout_row);
    lv_obj_set_size(timeout_slider, lv_pct(100), 16);
    lv_slider_set_range(timeout_slider, 0, timeout_count - 1);

    // Find current timeout index
    int32_t current_index = 0;
    for (size_t i = 0; i < timeout_count; i++) {
        if (timeout_values[i] == badge_config.screen_timeout) {
            current_index = i;
            break;
        }
    }
    lv_slider_set_value(timeout_slider, current_index, LV_ANIM_OFF);
    lv_obj_remove_flag(timeout_slider, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(timeout_slider, LV_OBJ_FLAG_CLICKABLE);

    // Attach guard data
    slider_guard_t *timeout_guard = calloc(1, sizeof(slider_guard_t));
    timeout_guard->last           = current_index;
    timeout_guard->staged         = 0;
    timeout_guard->has_staged     = false;
    timeout_guard->internal       = false;
    lv_obj_set_user_data(timeout_slider, timeout_guard);

    // Slider styling - same as brightness
    lv_obj_set_style_bg_color(timeout_slider, lv_color_hex(GRAY_SHADE_2), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(timeout_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(timeout_slider, 3, LV_PART_MAIN);

    lv_obj_set_style_bg_color(timeout_slider, lv_color_hex(BLUE_MAIN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(timeout_slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(timeout_slider, 3, LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(timeout_slider, lv_color_hex(WHITE), LV_PART_KNOB);
    lv_obj_set_style_pad_all(timeout_slider, 6, LV_PART_KNOB);
    lv_obj_set_style_radius(timeout_slider, 6, LV_PART_KNOB);

    // Focus styling
    lv_obj_set_style_outline_width(timeout_slider, 4, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_color(timeout_slider, lv_color_hex(BLUE_MAIN), LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_pad(timeout_slider, 3, LV_PART_MAIN | LV_STATE_FOCUS_KEY);

    lv_obj_add_event_cb(timeout_slider, timeout_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(timeout_slider, timeout_event_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(timeout_slider, timeout_event_cb, LV_EVENT_FOCUSED, NULL);

    timeout_label = lv_label_create(timeout_row);
    char timeout_buf[32];
    format_timeout_string(timeout_buf, sizeof(timeout_buf), timeout_values[current_index]);
    lv_label_set_text(timeout_label, timeout_buf);
    lv_obj_set_style_text_color(timeout_label, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(timeout_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(timeout_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_top(timeout_label, 4, LV_PART_MAIN);
    lv_obj_set_width(timeout_label, lv_pct(100));

    // -------------------------------------------------------------------------------------------------
    // Setting 4: Faction Indicator
    // -------------------------------------------------------------------------------------------------
    lv_obj_t *faction_row = create_setting_row(settings_list, "Faction Indicator");

    faction_switch = lv_switch_create(faction_row);
    lv_obj_set_size(faction_switch, 50, 24);

    bool initial_state = badge_config.faction_leds;
    if (initial_state) {
        lv_obj_add_state(faction_switch, LV_STATE_CHECKED);
    }

    // Attach guard data
    switch_guard_t *switch_guard = calloc(1, sizeof(switch_guard_t));
    switch_guard->last           = initial_state;
    switch_guard->internal       = false;
    lv_obj_set_user_data(faction_switch, switch_guard);

    // Disable if requirements aren't met: valid team ID and sorting hat completed
    bool faction_requirements_met = badge_config.team_id <= NUM_FACTIONS && badge_config.sorting_hat;
    if (!faction_requirements_met) {
        lv_obj_add_state(faction_switch, LV_STATE_DISABLED);
        lv_obj_set_style_opa(faction_switch, LV_OPA_30, LV_PART_MAIN | LV_STATE_DISABLED);
    }

    // Register PREPROCESS event first to intercept keys before LVGL's internal handlers
    lv_obj_add_event_cb(faction_switch, faction_switch_event_cb, LV_EVENT_PREPROCESS, NULL);
    lv_obj_add_event_cb(faction_switch, faction_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(faction_switch, faction_switch_event_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(faction_switch, faction_switch_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(faction_switch, scroll_to_focused_cb, LV_EVENT_FOCUSED, NULL);

    // Focus styling for switch
    lv_obj_set_style_outline_width(faction_switch, 4, LV_PART_INDICATOR | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_color(faction_switch, lv_color_hex(BLUE_MAIN), LV_PART_INDICATOR | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_pad(faction_switch, 3, LV_PART_INDICATOR | LV_STATE_FOCUS_KEY);

    // -------------------------------------------------------------------------------------------------
    // Version Display
    // -------------------------------------------------------------------------------------------------
    lv_obj_t *version_label = lv_label_create(main_container);
    version_t ver           = firmware_version();
    lv_label_set_text_fmt(version_label, "v%d.%d.%d", ver.major, ver.minor, ver.patch);
    lv_obj_set_style_text_font(version_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(version_label, lv_color_hex(GRAY_SHADE_3), LV_PART_MAIN);
    lv_obj_set_style_pad_top(version_label, 8, LV_PART_MAIN);
}
static void settings_nav_setup(lv_obj_t *content_area, nav_ctx_t *nav_ctx, void *user_data) {
    (void)content_area;
    (void)user_data;

    settings_nav_ctx = nav_ctx;

    // Register all interactive elements
    nav_register(nav_ctx, handle_value_label);
    nav_register(nav_ctx, brightness_slider);
    nav_register(nav_ctx, timeout_slider);
    nav_register(nav_ctx, faction_switch);

    // // Manually link vertically (autolink only does horizontal)
    // nav_bind_vertical(nav_ctx, handle_value_label, brightness_slider, false);
    // nav_bind_vertical(nav_ctx, brightness_slider, timeout_slider, false);
    // nav_bind_vertical(nav_ctx, timeout_slider, faction_switch, false);

    // Set initial focus
    nav_focus(nav_ctx, handle_value_label);

    ESP_LOGD(TAG, "Settings navigation setup complete");
}