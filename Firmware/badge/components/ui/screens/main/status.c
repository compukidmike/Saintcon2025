#include <time.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#include "theme.h"
#include "wifi_manager.h"
#include "battery.h"

#include "status.h"

static const char *TAG __attribute__((unused)) = "screens/main [status]";

#define STATUS_BAR_HEIGHT 35

static lv_timer_t *clock_timer          = NULL;
static lv_timer_t *wifi_animation_timer = NULL;
static lv_obj_t *clock_label            = NULL;
static lv_obj_t *wifi_icon              = NULL;
static lv_obj_t *battery_icon           = NULL;

// WiFi animation state
static uint8_t wifi_animation_frame      = 0;
static wifi_status_t current_wifi_status = WIFI_STATUS_DISCONNECTED;

static void cleanup_timers(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        if (clock_timer != NULL) {
            lv_timer_delete(clock_timer);
            clock_timer = NULL;
        }
        if (wifi_animation_timer != NULL) {
            lv_timer_delete(wifi_animation_timer);
            wifi_animation_timer = NULL;
        }
    }
}

static const char *get_formatted_time() {
    time_t now;
    struct tm timeinfo;
    static char time_buffer[20];
    time(&now);

    // Times before 2000-01-01 00:00:00 UTC (946684800) are obviously invalid
    // so we'll just return a placeholder since SNTP might not have run yet
    if (now < 946684800) {
        return "--:-- --";
    }

    localtime_r(&now, &timeinfo);
    strftime(time_buffer, sizeof(time_buffer), "%I:%M %p", &timeinfo);
    return time_buffer;
}

static void clock_timer_cb(lv_timer_t *timer) {
    lv_label_set_text(clock_label, get_formatted_time());
}

static void wifi_animation_timer_cb(lv_timer_t *timer) {
    (void)timer;
    static int icon_index = 0;

    if (wifi_icon) {
        switch (icon_index) {
            case 0: lv_label_set_text(wifi_icon, FA_WIFI_WEAK); break;
            case 1: lv_label_set_text(wifi_icon, FA_WIFI_FAIR); break;
            case 2: lv_label_set_text(wifi_icon, FA_WIFI); break;
        }
        icon_index = (icon_index + 1) % 3;
    }
}

lv_obj_t *create_status_bar(lv_obj_t *parent) {
    lv_obj_t *status_bar = lv_obj_create(parent);
    lv_obj_set_size(status_bar, LV_HOR_RES, STATUS_BAR_HEIGHT);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(RED_MAIN), LV_PART_MAIN);
    lv_obj_set_style_border_width(status_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(status_bar, lv_color_hex(RED_DIMMER), LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(status_bar, LV_SCROLLBAR_MODE_OFF);

    // Time label
    clock_label = lv_label_create(status_bar);
    lv_label_set_text(clock_label, get_formatted_time());
    lv_obj_set_style_text_color(clock_label, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(clock_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(clock_label, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(clock_label, cleanup_timers, LV_EVENT_DELETE, NULL);

    // Start the clock timer
    if (clock_timer == NULL) {
        clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
    }

    // Container for icons on the right side (a sort of tray to contain them)
    lv_obj_t *icon_tray = lv_obj_create(status_bar);
    lv_obj_set_size(icon_tray, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(icon_tray, LV_ALIGN_RIGHT_MID, 10, 0);
    lv_obj_add_event_cb(icon_tray, cleanup_timers, LV_EVENT_DELETE, NULL);

    lv_obj_set_flex_flow(icon_tray, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(icon_tray, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_pad_all(icon_tray, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(icon_tray, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_column(icon_tray, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_row(icon_tray, 0, LV_PART_MAIN);

    // Styling
    lv_obj_set_style_bg_color(icon_tray, lv_color_hex(RED_DIMMER), LV_PART_MAIN);
    lv_obj_set_style_border_width(icon_tray, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(icon_tray, 5, LV_PART_MAIN);

    // WiFi status
    wifi_icon = lv_label_create(icon_tray);
    lv_obj_set_size(wifi_icon, 18, 18);
    lv_label_set_text(wifi_icon, FA_WIFI_EXCLAMATION);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(wifi_icon, &fa_duotone_icons_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(wifi_icon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    current_wifi_status = WIFI_STATUS_DISCONNECTED;

    // Battery status
    battery_icon = lv_label_create(icon_tray);
    lv_obj_set_size(battery_icon, 18, 18);
    lv_label_set_text(battery_icon, FA_BATTERY_EXCLAMATION);
    lv_obj_set_style_text_color(battery_icon, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(battery_icon, &fa_duotone_icons_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(battery_icon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_CLICKABLE);

    return status_bar;
}

void update_status_bar(ui_state_t *state) {
    // Update WiFi status icon based on state
    if (wifi_icon != NULL) {
        // Handle WiFi status change
        if (current_wifi_status != state->wifi_state) {
            current_wifi_status = state->wifi_state;

            switch (current_wifi_status) {
                case WIFI_STATUS_DISCONNECTED:
                    lv_label_set_text(wifi_icon, FA_WIFI_EXCLAMATION);
                    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(RED_MAIN), LV_PART_MAIN);
                    // Stop animation timer if running
                    if (wifi_animation_timer != NULL) {
                        lv_timer_delete(wifi_animation_timer);
                        wifi_animation_timer = NULL;
                    }
                    break;

                case WIFI_STATUS_CONNECTING:
                    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(YELLOW_MAIN), LV_PART_MAIN);
                    wifi_animation_frame = 0;
                    lv_label_set_text(wifi_icon, FA_WIFI_WEAK);
                    // Start animation timer
                    if (wifi_animation_timer == NULL) {
                        wifi_animation_timer = lv_timer_create(wifi_animation_timer_cb, 500, NULL);
                    }
                    break;

                case WIFI_STATUS_CONNECTED:
                    lv_label_set_text(wifi_icon, FA_WIFI);
                    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(GREEN_MAIN), LV_PART_MAIN);
                    // Stop animation timer if running
                    if (wifi_animation_timer != NULL) {
                        lv_timer_delete(wifi_animation_timer);
                        wifi_animation_timer = NULL;
                    }
                    break;
            }
        }
    }

    // Update battery status icon based on state
    if (battery_icon != NULL) {
        lv_color_t battery_color;

        switch (state->battery_level) {
            case BATTERY_LEVEL_FULL:
                lv_label_set_text(battery_icon, FA_BATTERY_FULL);
                battery_color = lv_color_hex(GREEN_MAIN);
                break;
            case BATTERY_LEVEL_3:
                lv_label_set_text(battery_icon, FA_BATTERY_THREE_QUARTERS);
                battery_color = lv_color_hex(GREEN_MAIN);
                break;
            case BATTERY_LEVEL_2:
                lv_label_set_text(battery_icon, FA_BATTERY_HALF);
                battery_color = lv_color_hex(YELLOW_MAIN);
                break;
            case BATTERY_LEVEL_1:
                lv_label_set_text(battery_icon, FA_BATTERY_QUARTER);
                battery_color = lv_color_hex(ORANGE_MAIN);
                break;
            case BATTERY_LEVEL_EMPTY:
            default:
                lv_label_set_text(battery_icon, FA_BATTERY_EXCLAMATION);
                battery_color = lv_color_hex(RED_MAIN);
                break;
        }

        lv_obj_set_style_text_color(battery_icon, battery_color, LV_PART_MAIN);
    }
}