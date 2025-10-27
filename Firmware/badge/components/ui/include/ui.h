#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "esp_log.h"
#include "lvgl.h"
#include "lvgl_private.h"

#include "badge.h"
#include "battery.h"
#include "wifi_manager.h"

// Macro to log detailed information about an LVGL object for debugging
#define LVGL_DEBUG_OBJECT(obj, label)                                                                                          \
    do {                                                                                                                       \
        if (esp_log_level_get(TAG) >= ESP_LOG_DEBUG) {                                                                         \
            printf("[%s:%d] %s%s", __FUNCTION__, __LINE__, (label) ? (label) : "", (label) ? ": " : "");                       \
            if (obj) {                                                                                                         \
                const lv_obj_class_t *cls = lv_obj_get_class(obj);                                                             \
                lv_area_t coords;                                                                                              \
                lv_obj_get_coords(obj, &coords);                                                                               \
                printf("%s at (%ld,%ld) size (%ldx%ld) ptr=%p\n", cls->name, coords.x1, coords.y1, lv_area_get_width(&coords), \
                       lv_area_get_height(&coords), obj);                                                                      \
            } else {                                                                                                           \
                printf("NULL object\n");                                                                                       \
            }                                                                                                                  \
        }                                                                                                                      \
    } while (0)

typedef enum {
    SCREEN_NONE,   // No screen
    SCREEN_SPLASH, // The splash screen
    SCREEN_MAIN,   // The main screen
    SCREEN_UPDATE, // The OTA firmware update screen
    SCREEN_HWTEST, // The hardware test screen
} ui_screen_t;

typedef struct {
    ui_screen_t screen;            // The current screen to display
    wifi_status_t wifi_state;      // We only have a single wifi icon built in... we'll just change the color
    battery_level_t battery_level; // The battery level to display (0 - empty, 1, 2, 3, 4 - full)
} ui_state_t;

// Status labels for each screen type
extern const char *screen_labels[];

void ui_init();
bool ui_ready();

/**
 * State management functions
 */

void set_screen(ui_screen_t screen);
void set_ui_state_wifi(wifi_status_t state);
void set_ui_state_battery(battery_level_t level);
void set_ui_state_ota(ota_state_t state);
void notify_badge_config_updated();
ui_state_t get_ui_state();

// Expose additional things from the main screen
#include "../screens/main.h"
void launch_app(content_app_t app);

#ifdef __cplusplus
}
#endif
