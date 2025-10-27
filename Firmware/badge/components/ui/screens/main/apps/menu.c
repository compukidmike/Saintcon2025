#include "esp_log.h"
#include "esp_timer.h"

#include "menu.h"
#include "nav.h"
#include "theme.h"
#include "ui.h"
#include "version.h"
#include "api.h"
#include "badge.h"
#include "badge_game.h"
#include "display.h"

#include "../content.h"

static const char *TAG __attribute__((unused)) = "screens/main [apps/menu]";

// List of apps in the menu
content_app_t menu_apps[] = {
    APP_GAME,
    APP_SETTINGS,
    APP_CODE,
    APP_CREDITS,
    /*
    APP_MAP,
    */
};

#define MENU_DEFAULT_ITEM APP_GAME

// Forward declarations
static void menu_event_handler(lv_event_t *e);
static void menu_focus_event_handler(lv_event_t *e);
static void menu_nav_setup(lv_obj_t *content_area, nav_ctx_t *nav_ctx, void *user_data);
static void menu_konami_key_handler(lv_event_t *e);

// -------------------------------------------------------------------------------------------------
// Konami Reporting
// -------------------------------------------------------------------------------------------------

// Stack size for Konami report task
#define KONAMI_REPORT_TASK_STACK 6144

static bool konami_reported = false;
static void konami_report_task(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "Reporting Konami achievement to API");
    api_result_t *res = api_konami_entered();
    if (!res) {
        ESP_LOGW(TAG, "Konami API call returned NULL result");
        konami_reported = false; // allow retry if needed
    } else {
        api_konami_response_t *kres = (api_konami_response_t *)res->data;
        ESP_LOGI(TAG, "Konami achievement reported: %s, first time: %s", res->ok ? "success" : "failure",
                 kres->first_time ? "yes" : "no");
        api_free_result(res, true);
    }
    vTaskDelete(NULL);
}

// Konami code sequence detection
typedef enum {
    KONAMI_UP1,
    KONAMI_UP2,
    KONAMI_DOWN1,
    KONAMI_DOWN2,
    KONAMI_LEFT1,
    KONAMI_RIGHT1,
    KONAMI_LEFT2,
    KONAMI_RIGHT2,
    KONAMI_CENTER,
    KONAMI_COMPLETE
} konami_step_t;

typedef struct {
    konami_step_t current_step;
    int64_t last_input_time;
    bool sequence_active;
} konami_state_t;

#define KONAMI_TIMEOUT_US (1000000) // 1 second timeout between inputs
static konami_state_t konami_state = {
    .current_step    = KONAMI_UP1,
    .last_input_time = 0,
    .sequence_active = false,
};

// Static state for focus restoration
static content_app_t last_focused_app = APP_NONE;

// Faction label for updates
static lv_obj_t *faction_label = NULL;

// Menu state tracking
typedef struct {
    content_app_t currently_focused_app;
    bool in_setup;
} menu_state_t;
static menu_state_t menu_state = {
    .currently_focused_app = APP_NONE,
    .in_setup              = false,
};

void menu_app_create(lv_obj_t *parent) {
    // Register our navigation callback
    content_register_nav_callback(APP_MENU, menu_nav_setup, NULL);

    lv_obj_t *menu = lv_obj_create(parent);
    lv_obj_set_size(menu, LV_PCT(100), LV_PCT(100));
    lv_obj_align(menu, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(menu, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(menu, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(menu, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(menu, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *menu_list = lv_list_create(menu);
    lv_obj_set_size(menu_list, LV_PCT(85), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(menu_list, lv_color_hex(GRAY_SHADE_5), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(menu_list, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_border_width(menu_list, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(menu_list, lv_color_hex(MENU_BUTTON_BORDER), LV_PART_MAIN);
    lv_obj_set_style_pad_all(menu_list, 0, LV_PART_MAIN);
    lv_obj_align(menu_list, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(menu_list, lv_color_hex(MENU_BUTTON_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(menu_list, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_radius(menu_list, 5, LV_PART_MAIN);

    // Add menu items
    for (size_t i = 0; i < sizeof(menu_apps) / sizeof(menu_apps[0]); i++) {
        content_app_t app = menu_apps[i];
        lv_obj_t *btn     = lv_list_add_button(menu_list, content_app_symbols[app], get_content_app_label(app));
        lv_obj_t *icon    = lv_obj_get_child(btn, 0);
        lv_obj_set_style_text_font(icon, &fa_duotone_icons_16, LV_PART_MAIN);
        lv_obj_set_style_opa(btn, LV_OPA_80, LV_PART_MAIN);
        lv_obj_set_user_data(btn, (void *)app);
        lv_obj_add_event_cb(btn, menu_konami_key_handler, LV_EVENT_KEY, NULL);
        lv_obj_add_event_cb(btn, menu_event_handler, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(btn, menu_focus_event_handler, LV_EVENT_FOCUSED, NULL);
    }

    // Show faction indicator at the bottom of the screen
    faction_label = lv_label_create(parent);
    lv_obj_set_style_text_font(faction_label, &white_rabbit_16, LV_PART_MAIN);
    lv_obj_set_style_pad_left(faction_label, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_right(faction_label, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_top(faction_label, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(faction_label, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(faction_label, 4, LV_PART_MAIN);

    // Check if faction should be displayed (team_id is 1-based, so valid factions are 1 to NUM_FACTIONS)
    bool faction_requirements_met = badge_config.team_id > 0 && badge_config.team_id <= NUM_FACTIONS && badge_config.sorting_hat;

    if (faction_requirements_met) {
        faction_t faction_info = get_faction(faction_from_team_id(badge_config.team_id));
        lv_label_set_text(faction_label, faction_info.name);

        // Set background to faction color
        lv_color_t bg_color = faction_info.screen_color;
        lv_obj_set_style_bg_color(faction_label, bg_color, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(faction_label, LV_OPA_COVER, LV_PART_MAIN);

        // Calculate luminance to determine text color (white vs black)
        uint8_t r             = bg_color.red;
        uint8_t g             = bg_color.green;
        uint8_t b             = bg_color.blue;
        float luma            = (0.299f * r + 0.587f * g + 0.114f * b) / 255.0f;
        lv_color_t text_color = (luma > 0.5f) ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF);
        lv_obj_set_style_text_color(faction_label, text_color, LV_PART_MAIN);
    } else {
        lv_label_set_text(faction_label, "Faction Unassigned");
        lv_obj_set_style_bg_color(faction_label, lv_color_hex(GRAY_SHADE_4), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(faction_label, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_color(faction_label, lv_color_hex(WHITE), LV_PART_MAIN);
    }

    lv_obj_align(faction_label, LV_ALIGN_BOTTOM_MID, 0, -15);
}

void menu_update_faction_label() {
    if (!faction_label || !lv_obj_is_valid(faction_label)) {
        return;
    }

    if (!lvgl_lock(portMAX_DELAY, __FILE__, __LINE__)) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock for faction label update");
        return;
    }

    // Check if faction should be displayed (team_id is 1-based, so valid factions are 1 to NUM_FACTIONS)
    bool faction_requirements_met = badge_config.team_id > 0 && badge_config.team_id <= NUM_FACTIONS && badge_config.sorting_hat;

    ESP_LOGD(TAG, "Updating faction label: team_id=%d, sorting_hat=%d, requirements_met=%d", badge_config.team_id,
             badge_config.sorting_hat, faction_requirements_met);

    if (faction_requirements_met) {
        faction_t faction_info = get_faction(faction_from_team_id(badge_config.team_id));
        lv_label_set_text(faction_label, faction_info.name);

        // Set background to faction color
        lv_color_t bg_color = faction_info.screen_color;
        lv_obj_set_style_bg_color(faction_label, bg_color, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(faction_label, LV_OPA_COVER, LV_PART_MAIN);

        // Calculate luminance to determine text color (white vs black)
        uint8_t r             = bg_color.red;
        uint8_t g             = bg_color.green;
        uint8_t b             = bg_color.blue;
        float luma            = (0.299f * r + 0.587f * g + 0.114f * b) / 255.0f;
        lv_color_t text_color = (luma > 0.5f) ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF);
        lv_obj_set_style_text_color(faction_label, text_color, LV_PART_MAIN);
    } else {
        lv_label_set_text(faction_label, "Faction Unassigned");
        lv_obj_set_style_bg_color(faction_label, lv_color_hex(GRAY_SHADE_4), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(faction_label, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_color(faction_label, lv_color_hex(WHITE), LV_PART_MAIN);
    }

    lvgl_unlock(__FILE__, __LINE__);
}

static void menu_focus_event_handler(lv_event_t *e) {
    lv_obj_t *obj        = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_FOCUSED) {
        // Don't update state if we're in the middle of setup
        if (menu_state.in_setup) {
            ESP_LOGD(TAG, "Ignoring focus event during setup");
            return;
        }

        // Update our state tracking when focus changes
        content_app_t app = (content_app_t)lv_obj_get_user_data(obj);
        if (app != APP_NONE) {
            menu_state.currently_focused_app = app;
            ESP_LOGD(TAG, "Focus changed to app: %d [%s]", app, get_content_app_label(app));
        } else {
            ESP_LOGW(TAG, "Focus changed to button with no app data");
        }
    }
}

static void menu_event_handler(lv_event_t *e) {
    lv_obj_t *obj __attribute__((unused)) = lv_event_get_target(e);
    lv_event_code_t code                  = lv_event_get_code(e);
    ESP_LOGD(TAG, "Event code: %d", code);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *btn     = lv_event_get_target(e);
        content_app_t app = (content_app_t)lv_obj_get_user_data(btn);
        ESP_LOGD(TAG, "Menu item clicked: %d [%s]", app, get_content_app_label(app));

        // Save the focused app for restoration later
        last_focused_app = app;
        ESP_LOGD(TAG, "Set last_focused_app to: %d", last_focused_app);

        launch_app(app);
    }

    return;
}

static void menu_konami_key_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) {
        return;
    }

    lv_key_t key         = lv_event_get_key(e);
    int64_t current_time = esp_timer_get_time();

    // Check if we've timed out since the last input
    if (konami_state.sequence_active && (current_time - konami_state.last_input_time) > KONAMI_TIMEOUT_US) {
        ESP_LOGD(TAG, "Konami sequence timed out, resetting");
        konami_state.current_step    = KONAMI_UP1;
        konami_state.sequence_active = false;
    }

    // Check if this key matches the expected step
    bool key_matches = false;
    lv_key_t expected_key;

    switch (konami_state.current_step) {
        case KONAMI_UP1:
        case KONAMI_UP2: expected_key = LV_KEY_UP; break;
        case KONAMI_DOWN1:
        case KONAMI_DOWN2: expected_key = LV_KEY_DOWN; break;
        case KONAMI_LEFT1:
        case KONAMI_LEFT2: expected_key = LV_KEY_LEFT; break;
        case KONAMI_RIGHT1:
        case KONAMI_RIGHT2: expected_key = LV_KEY_RIGHT; break;
        case KONAMI_CENTER: expected_key = LV_KEY_ENTER; break;
        default:
            expected_key = LV_KEY_ENTER; // Should not reach here
            break;
    }

    key_matches = (key == expected_key);

    if (key_matches) {
        // Advance to next step
        konami_state.last_input_time = current_time;
        konami_state.sequence_active = true;

        switch (konami_state.current_step) {
            case KONAMI_UP1:
                konami_state.current_step = KONAMI_UP2;
                ESP_LOGD(TAG, "Konami: UP1 -> UP2");
                break;
            case KONAMI_UP2:
                konami_state.current_step = KONAMI_DOWN1;
                ESP_LOGD(TAG, "Konami: UP2 -> DOWN1");
                break;
            case KONAMI_DOWN1:
                konami_state.current_step = KONAMI_DOWN2;
                ESP_LOGD(TAG, "Konami: DOWN1 -> DOWN2");
                break;
            case KONAMI_DOWN2:
                konami_state.current_step = KONAMI_LEFT1;
                ESP_LOGD(TAG, "Konami: DOWN2 -> LEFT1");
                break;
            case KONAMI_LEFT1:
                konami_state.current_step = KONAMI_RIGHT1;
                ESP_LOGD(TAG, "Konami: LEFT1 -> RIGHT1");
                break;
            case KONAMI_RIGHT1:
                konami_state.current_step = KONAMI_LEFT2;
                ESP_LOGD(TAG, "Konami: RIGHT1 -> LEFT2");
                break;
            case KONAMI_LEFT2:
                konami_state.current_step = KONAMI_RIGHT2;
                ESP_LOGD(TAG, "Konami: LEFT2 -> RIGHT2");
                break;
            case KONAMI_RIGHT2:
                konami_state.current_step = KONAMI_CENTER;
                ESP_LOGD(TAG, "Konami: RIGHT2 -> CENTER");
                break;
            case KONAMI_CENTER:
                ESP_LOGI(TAG, "Konami code ACTIVATED! Switching to hardware test...");
                konami_state.current_step    = KONAMI_UP1;
                konami_state.sequence_active = false;
                set_screen(SCREEN_HWTEST);
                // Fire off achievement report asynchronously (avoid blocking UI path)
                if (!konami_reported) {
                    konami_reported = true;
                    if (xTaskCreate(konami_report_task, "konami_report", KONAMI_REPORT_TASK_STACK, NULL, 5, NULL) != pdPASS) {
                        ESP_LOGW(TAG, "Failed to create konami_report task");
                        konami_reported = false; // allow retry if creation failed
                    }
                } else {
                    ESP_LOGD(TAG, "Konami achievement already reported (skipping)");
                }
                return;
            default: break;
        }
    } else {
        // Wrong key - reset sequence if we had started
        if (konami_state.sequence_active || key == LV_KEY_UP) {
            // Special case: if they press UP when not in sequence, start tracking
            if (key == LV_KEY_UP && !konami_state.sequence_active) {
                konami_state.current_step    = KONAMI_UP2;
                konami_state.last_input_time = current_time;
                konami_state.sequence_active = true;
                ESP_LOGD(TAG, "Konami: Starting sequence with UP1 -> UP2");
            } else {
                ESP_LOGD(TAG, "Konami: Wrong key, resetting sequence");
                konami_state.current_step    = KONAMI_UP1;
                konami_state.sequence_active = false;
            }
        }
    }
}

static void menu_nav_setup(lv_obj_t *content_area, nav_ctx_t *nav_ctx, void *user_data) {
    ESP_LOGD(TAG, "[menu_nav_setup] - current_state: %d, last_focused: %d", menu_state.currently_focused_app, last_focused_app);

    // Reset Konami code sequence when entering menu
    konami_state.current_step    = KONAMI_UP1;
    konami_state.sequence_active = false;
    konami_state.last_input_time = 0;

    // Set flag to prevent focus events from doing anything to the state during setup
    menu_state.in_setup = true;

    // Find the menu components
    lv_obj_t *menu      = lv_obj_get_child(content_area, 0);
    lv_obj_t *menu_list = lv_obj_get_child_by_type(menu, 0, &lv_list_class);

    if (menu != NULL && menu_list != NULL) {
        // Register all menu buttons
        nav_register_children(nav_ctx, menu_list);
        nav_autolink(nav_ctx);

        // Try to restore focus based on our internal state tracking
        lv_obj_t *focus_target   = NULL;
        content_app_t target_app = menu_state.currently_focused_app;

        // If we don't have a state or it's invalid, try the last focused app
        if (target_app == APP_NONE || target_app == APP_MENU) {
            target_app = last_focused_app;
            ESP_LOGD(TAG, "Using last_focused_app: %d", target_app);
        }

        // If we still don't have a valid target, use default
        if (target_app == APP_NONE || target_app == APP_MENU) {
            target_app = MENU_DEFAULT_ITEM;
            ESP_LOGD(TAG, "Defaulting to MENU_DEFAULT_ITEM: %s", get_content_app_label(target_app));
        }

        // Find the button for our target app
        uint32_t child_count = lv_obj_get_child_count(menu_list);
        ESP_LOGD(TAG, "Looking for app %d in %u children", target_app, child_count);

        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t *btn = lv_obj_get_child(menu_list, i);
            if (btn != NULL) {
                content_app_t btn_app = (content_app_t)lv_obj_get_user_data(btn);
                ESP_LOGD(TAG, "Child %u has app %d", i, btn_app);
                if (btn_app == target_app) {
                    focus_target = btn;
                    ESP_LOGD(TAG, "Found target button at index %u", i);
                    break;
                }
            }
        }

        // Fallback to first button if we couldn't find our target
        if (focus_target == NULL) {
            focus_target = lv_obj_get_child(menu_list, 0);
            ESP_LOGD(TAG, "Fallback to first button");
            // Update our state to match the fallback
            if (focus_target != NULL) {
                target_app = (content_app_t)lv_obj_get_user_data(focus_target);
                ESP_LOGD(TAG, "Fallback target is app %d", target_app);
            }
        }

        // Update our state to match what we're about to focus
        menu_state.currently_focused_app = target_app;
        ESP_LOGD(TAG, "Final state set to app %d", target_app);

        // Clear setup flag before focusing
        menu_state.in_setup = false;

        if (focus_target != NULL) {
            ESP_LOGD(TAG, "Focusing target button");
            nav_focus(nav_ctx, focus_target);
        } else {
            ESP_LOGW(TAG, "No focus target found!");
        }
    } else {
        ESP_LOGW(TAG, "Could not find menu components");
        menu_state.in_setup = false;
    }
}