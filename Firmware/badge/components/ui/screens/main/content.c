#include "display.h"
#include "esp_log.h"
#include "input.h"
#include "nav.h"
#include "theme.h"
#include "ui.h"

#include "content.h"

// Content area apps
#include "apps/code.h"
#include "apps/credits.h"
#include "apps/game.h"
#include "apps/map.h"
#include "apps/menu.h"
#include "apps/settings.h"

static const char *TAG = "screens/main [content]";

const char *content_app_map[] = {
#define X(val, lbl, sym) #val,
    CONTENT_APP_LIST
#undef X
};
const char *content_app_labels[] = {
#define X(val, lbl, sym) lbl,
    CONTENT_APP_LIST
#undef X
};

const char *content_app_symbols[] = {
#define X(val, lbl, sym) sym,
    CONTENT_APP_LIST
#undef X
};

content_app_t get_content_app_type(const char *str) {
    for (size_t i = 0; i < sizeof(content_app_map) / sizeof(content_app_map[0]); i++) {
        if (strcmp(content_app_map[i], str) == 0) {
            return i;
        }
    }
    return APP_NONE;
}
const char *get_content_app_label(content_app_t app) {
    if (app < 0 || app >= sizeof(content_app_labels) / sizeof(content_app_labels[0])) {
        return "Unknown";
    }
    return content_app_labels[app];
}

#define CONTENT_DEFAULT_APP APP_MENU

/**
 * @brief Content state
 */
typedef struct {
    content_app_t app;
    int nav_freeze;    // Incremented when navigation is frozen, decremented when thawed
    bool dirty_app;    // Mark app dirty
    bool dirty_layout; // Mark layout dirty
} content_state_t;
static content_state_t content_state = {
    .app          = CONTENT_DEFAULT_APP,
    .nav_freeze   = 0,
    .dirty_app    = true,
    .dirty_layout = true,
};

// Navigation callback storage
typedef struct {
    content_nav_setup_cb_t callback;
    void *user_data;
} nav_callback_entry_t;
#define MAX_NAV_CALLBACKS 32
static nav_callback_entry_t nav_callbacks[MAX_NAV_CALLBACKS] = {0};
void content_nav_freeze() {
    if (content_state.nav_freeze < 255) {
        content_state.nav_freeze++;
    }
    ESP_LOGD(TAG, "Content navigation frozen");
}

void content_nav_thaw() {
    if (content_state.nav_freeze > 0) {
        content_state.nav_freeze--;
    }
    ESP_LOGD(TAG, "Content navigation thawed");
}

bool content_nav_is_frozen() {
    return content_state.nav_freeze > 0;
}

content_app_t get_current_app() {
    return content_state.app;
}

void set_current_app(content_app_t app) {
    if (content_state.app == app) {
        return;
    }
    content_state.app       = app;
    content_state.dirty_app = true;
    ESP_LOGD(TAG, "Set content app to: %s", content_app_labels[app]);
}

void content_mark_layout_dirty() {
    content_state.dirty_layout = true;
    ESP_LOGD(TAG, "Marked content layout dirty");
}

void content_register_nav_callback(content_app_t app, content_nav_setup_cb_t callback, void *user_data) {
    if (app >= 0 && app < MAX_NAV_CALLBACKS) {
        nav_callbacks[app].callback  = callback;
        nav_callbacks[app].user_data = user_data;
        ESP_LOGD(TAG, "Registered nav callback for app: %s", content_app_labels[app]);
    }
}

// Main navigation context
static nav_ctx_t *nav_ctx = NULL;

// Keep track of the content area
lv_obj_t *content_area = NULL;
lv_obj_t *home_button  = NULL;

// Forward declarations
static void home_button_event_handler(lv_event_t *e);

lv_obj_t *create_content_area(lv_obj_t *parent, const lv_obj_t *status_bar) {
    content_area = lv_obj_create(parent);
    lv_obj_set_size(content_area, LV_HOR_RES, LV_VER_RES - lv_obj_get_style_height(status_bar, LV_PART_MAIN));
    lv_obj_set_style_bg_opa(content_area, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(content_area, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(content_area, 0, LV_PART_MAIN);
    lv_obj_align(content_area, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Create a global home button
    home_button = lv_button_create(lv_obj_get_parent(content_area));
    lv_obj_set_size(home_button, 35, 35);
    lv_obj_align_to(home_button, content_area, LV_ALIGN_OUT_TOP_MID, 0, 10);
    lv_obj_set_style_bg_color(home_button, lv_color_hex(ORANGE_LIGHT), LV_PART_MAIN);
    lv_obj_set_style_radius(home_button, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    // Base border (subtle when not focused)
    lv_obj_set_style_border_width(home_button, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(home_button, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_border_opa(home_button, LV_OPA_60, LV_PART_MAIN);
    // Focus outline & stronger border for joystick/nav focus
    lv_obj_set_style_outline_width(home_button, 4, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_color(home_button, lv_color_hex(ORANGE_LIGHTEST), LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_opa(home_button, LV_OPA_100, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_pad(home_button, 2, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    // lv_obj_set_style_border_width(home_button, 4, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    // lv_obj_set_style_border_color(home_button, lv_color_hex(WHITE), LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    // lv_obj_set_style_border_opa(home_button, LV_OPA_100, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    // Also apply when just focused (non-key) to be safe
    lv_obj_set_style_outline_width(home_button, 4, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(home_button, lv_color_hex(ORANGE_LIGHTEST), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_opa(home_button, LV_OPA_100, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(home_button, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
    // lv_obj_set_style_border_width(home_button, 4, LV_PART_MAIN | LV_STATE_FOCUSED);
    // lv_obj_set_style_border_color(home_button, lv_color_hex(WHITE), LV_PART_MAIN | LV_STATE_FOCUSED);
    // lv_obj_set_style_border_opa(home_button, LV_OPA_100, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_add_flag(home_button, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(home_button, home_button_event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t *home_icon = lv_label_create(home_button);
    lv_label_set_text(home_icon, LV_SYMBOL_HOME);
    lv_obj_set_style_text_color(home_icon, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(home_icon, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(home_icon);
    lv_obj_clear_flag(home_icon, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_clear_flag(home_icon, LV_OBJ_FLAG_CLICKABLE);

    // Hide the home button by default so we can show it when in non-menu apps
    lv_obj_add_flag(home_button, LV_OBJ_FLAG_HIDDEN);

    content_state.app = CONTENT_DEFAULT_APP;

    return content_area;
}

void update_content_area() {
    if (content_nav_is_frozen()) {
        ESP_LOGD(TAG, "Content navigation is frozen, skipping update");
        return;
    }
    if (!content_state.dirty_app && !content_state.dirty_layout) {
        ESP_LOGD(TAG, "Content area is not dirty, skipping update");
        return;
    }

    // Remove all objects from the default object group
    lv_group_t *group = lv_group_get_default();
    if (group != NULL) {
        lv_group_set_wrap(group, false);
        lv_group_remove_all_objs(group);
    } else {
        ESP_LOGW(TAG, "Default group is NULL, cannot update content area navigation");
        return;
    }

    // Clean the content area
    lv_obj_clean(content_area);

    // Clean up previous navigation context
    if (nav_ctx != NULL) {
        nav_ctx_destroy(nav_ctx);
        nav_ctx = NULL;
    }

    // Create navigation context for the content area
    nav_ctx = nav_ctx_create(input_get_device(), group, content_area);

    // Enable the home button if we're not in the menu app
    if (content_state.app != APP_MENU) {
        if (home_button != NULL) {
            lv_obj_clear_flag(home_button, LV_OBJ_FLAG_HIDDEN);
            nav_register(nav_ctx, home_button);
        }

        // Set the initial focus to the home button - can be overridden by other apps below
        nav_focus(nav_ctx, home_button);
    }

    switch (content_state.app) {
        case APP_MENU: //
            menu_app_create(content_area);
            lv_obj_add_flag(home_button, LV_OBJ_FLAG_HIDDEN);
            break;
        case APP_GAME: //
            game_app_create(content_area);
            break;
        case APP_SETTINGS: //
            settings_app_create(content_area);
            break;
        case APP_CODE: //
            code_app_create(content_area);
            break;
        case APP_CREDITS: //
            credits_app_create(content_area);
            break;
        case APP_MAP: //
            map_app_create(content_area);
            break;
        default: //
            ESP_LOGW(TAG, "Unknown content app: %d", content_state.app);
            break;
    }

    // Call app navigation callback if registered
    if (nav_callbacks[content_state.app].callback != NULL) {
        nav_callbacks[content_state.app].callback(content_area, nav_ctx, nav_callbacks[content_state.app].user_data);
    }

    // Auto-link navigation elements
    nav_autolink(nav_ctx);

    // Clear dirty flags
    content_state.dirty_app    = false;
    content_state.dirty_layout = false;
}

static void home_button_event_handler(lv_event_t *e) {
    // lv_obj_t *btn = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        ESP_LOGD(TAG, "Home button clicked, switching to menu");
        launch_app(APP_MENU);
    }
}