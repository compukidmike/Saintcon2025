#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "ui.h"

/**
 * @brief UI event types
 */
#define UI_EVENT_TYPE_LIST \
    X(NONE)                \
    X(SET_SCREEN)          \
    X(SET_WIFI_STATE)      \
    X(SET_BATTERY_LEVEL)   \
    X(SET_APP)             \
    X(OTA_STATE)           \
    X(BADGE_CONFIG_UPDATED)
#undef X
typedef enum {
#define X(val) UI_EVENT_##val,
    UI_EVENT_TYPE_LIST
#undef X
} ui_event_type_t;
extern const char *ui_event_type_map[];
ui_event_type_t get_ui_event_type(const char *type_str);

// UI event data
typedef struct {
    ui_event_type_t type;
    union {
        ui_screen_t screen;
        wifi_status_t wifi_state;
        battery_level_t battery_level;
        content_app_t app;
        ota_state_t ota_state;
    } data;
} __attribute__((aligned(4))) ui_event_t;

#ifdef __cplusplus
}
#endif
