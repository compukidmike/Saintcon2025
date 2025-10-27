#include <string.h>
#include "esp_err.h"
#include "esp_log.h"

// Main UI headers
#include "ui.h"
#include "ui_events.h"
#include "theme.h"

// Component dependencies
#include "api.h"
#include "badge.h"
#include "battery.h"
#include "display.h"
#include "input.h"

// UI screens
#include "screens/hwtest.h"
#include "screens/main.h"
#include "screens/main/apps/menu.h"
#include "screens/splash.h"
#include "screens/update.h"
#include "flows/registration.h"

static const char *TAG = "ui";

const char *screen_labels[] = {
    "None", "Splash", "Main", "Update", "HW Test",
};

#define UI_EVENT_QUEUE_SIZE     30
#define UI_SCREEN_FADE_DURATION 500

static bool ui_initialized          = false;
static ui_state_t state             = {0};
static ui_screen_t screen_trans_to  = SCREEN_NONE; // While transitioning to a new screen
static ui_screen_t screen_next      = SCREEN_NONE; // The next screen to transition to if we're currently transitioning
static QueueHandle_t ui_event_queue = NULL;
static TaskHandle_t ui_task_handle  = NULL;

// Function prototypes
static void ui_task(void *_arg);
static void update_screen(ui_screen_t screen);

bool ui_ready() {
    return ui_initialized;
}

void ui_init() {
    if (ui_initialized) {
        ESP_LOGW(TAG, "UI already initialized");
        return;
    }

    // Initialize LVGL plugins and features
    lv_bin_decoder_init();
    lv_lodepng_init();
    lv_fs_stdio_init();
    lv_fs_posix_init();

    // Initialize styles
    style_init();

    // Intialize the event queue
    if (ui_event_queue == NULL && (ui_event_queue = xQueueCreate(UI_EVENT_QUEUE_SIZE, sizeof(ui_event_t))) == NULL) {
        ESP_LOGE(TAG, "Failed to create UI event queue");
        return;
    }

    // Try to get current battery and charging status
    battery_status_t battery_status = battery_get_status();
    state.battery_level             = battery_status.level;

    // Create the UI task
    if (xTaskCreate(ui_task, "ui_task", 8192, NULL, 6, &ui_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UI task");
        return;
    }

    // Render the splash screen initially
    update_screen(SCREEN_SPLASH);

    ui_initialized = true;
}

static void screen_event_cb(lv_event_t *e) {
    // Make sure it's an event we care about
    const lv_event_code_t handle_codes[] = {
        LV_EVENT_SCREEN_UNLOAD_START,
        LV_EVENT_SCREEN_LOAD_START,
        LV_EVENT_SCREEN_LOADED,
        LV_EVENT_SCREEN_UNLOADED,
    };
    lv_event_code_t code = lv_event_get_code(e);
    bool handling        = false;
    for (int i = 0; i < sizeof(handle_codes) / sizeof(handle_codes[0]); i++) {
        if (code == handle_codes[i]) {
            handling = true;
            break;
        }
    }
    if (!handling) {
        return;
    }

    // Get the screen and user data
    lv_obj_t *screen        = lv_event_get_target(e);
    ui_screen_t screen_type = SCREEN_NONE;
    void *data              = lv_obj_get_user_data(screen);
    if (data == NULL) {
        ESP_LOGW(TAG, "No user data provided for screen event: %d", code);
        // if (code == LV_EVENT_SCREEN_LOAD_START || code == LV_EVENT_SCREEN_LOADED) {
        //     ESP_LOGD(TAG, "... setting screen type to: %s", screen_labels[screen_trans_to]);
        //     screen_type = screen_trans_to;
        // } else {
        //     ESP_LOGD(TAG, "... setting screen type to current state: %s", screen_labels[state.screen]);
        //     screen_type = state.screen;
        // }
    } else {
        screen_type = *(ui_screen_t *)data;
        ESP_LOGD(TAG, "Screen type: %d", screen_type);
    }

    if (code == LV_EVENT_SCREEN_LOAD_START) {
        ESP_LOGD(TAG, "Screen load start: %p, type: %s", screen, screen_labels[screen_type]);
        screen_trans_to = screen_type;
    } else if (code == LV_EVENT_SCREEN_LOADED) {
        ESP_LOGD(TAG, "Screen loaded: %p, type: %s", screen, screen_labels[screen_type]);

        // If we're transitioning to a new screen, update the state
        if (screen_trans_to != SCREEN_NONE) {
            ESP_LOGD(TAG, "Setting screen state to: %s", screen_labels[screen_type]);
            state.screen    = screen_type;
            screen_trans_to = SCREEN_NONE;

            // If the main screen just loaded, try to trigger badge sync and OTA check
            if (screen_type == SCREEN_MAIN) {
                badge_boot_deferred_tasks();
                badge_check_ready_tasks();
            }
        }

        // If we're transitioning to a new screen, do it now
        if (screen_next != SCREEN_NONE) {
            ESP_LOGD(TAG, "Transitioning to next screen: %s", screen_labels[screen_next]);
            set_screen(screen_next);
            screen_next = SCREEN_NONE;
        }
    } else if (code == LV_EVENT_SCREEN_UNLOAD_START) {
        ESP_LOGD(TAG, "Screen unload start: %p, type: %s", screen, screen_labels[screen_type]);
        switch (screen_type) {
            case SCREEN_SPLASH: //
                splash_screen_shutdown();
                break;
            default: //
                ESP_LOGD(TAG, "No specific cleanup required for screen type: %s", screen_labels[screen_type]);
                break;
        }
    } else if (code == LV_EVENT_SCREEN_UNLOADED) {
        ESP_LOGD(TAG, "Screen unloaded: %p, type: %s", screen, screen_labels[screen_type]);
        if (state.screen == SCREEN_MAIN && screen_trans_to == SCREEN_NONE) {
            lv_async_call(render_main, NULL);
            vTaskDelay(1);
        }
    }
}

static void update_screen(ui_screen_t new_screen) {
    if (screen_trans_to != SCREEN_NONE) {
        if (screen_trans_to != new_screen) {
            ESP_LOGW(TAG, "Already transitioning to screen: %s ... queueing %s for next screen change",
                     screen_labels[screen_trans_to], screen_labels[new_screen]);
            screen_next = new_screen;
        } else {
            ESP_LOGW(TAG, "Already transitioning to screen: %s ... ignoring duplicate request", screen_labels[new_screen]);
        }
        return;
    }

    if (lvgl_lock(portMAX_DELAY, __FILE__, __LINE__)) {
        lv_obj_t *screen                = NULL;
        lv_screen_load_anim_t anim_type = LV_SCR_LOAD_ANIM_NONE;

        switch (new_screen /*  == SCREEN_NONE ? state.screen : new_screen */) {
            case SCREEN_SPLASH:
                screen    = create_splash_screen();
                anim_type = LV_SCR_LOAD_ANIM_FADE_IN;
                break;
            case SCREEN_UPDATE:
                screen    = create_update_screen();
                anim_type = LV_SCR_LOAD_ANIM_MOVE_LEFT;
                break;
            case SCREEN_MAIN:
                screen    = create_main_screen();
                anim_type = state.screen == SCREEN_UPDATE   ? LV_SCR_LOAD_ANIM_MOVE_RIGHT
                            : state.screen == SCREEN_HWTEST ? LV_SCR_LOAD_ANIM_MOVE_LEFT
                                                            : LV_SCR_LOAD_ANIM_FADE_IN;
                break;
            case SCREEN_HWTEST:
                screen    = create_hwtest_screen();
                anim_type = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
                break;
            default: //
                ESP_LOGW(TAG, "Unknown screen type: %d", new_screen);
                break;
        }

        // Load the new screen with an animation and event callback
        if (screen != NULL) {
            ESP_LOGD(TAG, "Loading screen: %s", screen_labels[new_screen]);
            lv_obj_add_event_cb(screen, screen_event_cb, LV_EVENT_ALL, NULL);
            lv_screen_load_anim(screen, anim_type, UI_SCREEN_FADE_DURATION, 0, true);
            // lv_screen_load_anim(screen, anim_type, 0, 0, true);
            ESP_LOGD(TAG, "Screen load initiated: %s", screen_labels[new_screen]);
        }

        lvgl_unlock(__FILE__, __LINE__);
    }
}

void enqueue_ui_event(ui_event_t *event) {
    if (!ui_initialized) {
        ESP_LOGW(TAG, "UI not initialized ... not enqueuing event: %s", ui_event_type_map[event->type]);
        return;
    } else {
        ESP_LOGD(TAG, "Enqueuing UI event: %s", ui_event_type_map[event->type]);
    }

    // Ensure the event pointer is 4-byte aligned
    assert(((uintptr_t)event & 0x3) == 0);

    if (ui_event_queue != NULL) {
        if (xQueueSend(ui_event_queue, event, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to enqueue UI event: %d", event->type);
        }
    } else {
        ESP_LOGW(TAG, "UI event queue not initialized");
    }
}

void set_screen(ui_screen_t new_screen) {
    if (state.screen == new_screen) {
        ESP_LOGW(TAG, "Already on screen: %s ... not enqueueing SET_SCREEN event", screen_labels[new_screen]);
        return;
    }
    ui_event_t event = {
        .type        = UI_EVENT_SET_SCREEN,
        .data.screen = new_screen,
    };
    enqueue_ui_event(&event);
}

ui_state_t get_ui_state() {
    return state;
}

void set_ui_state_wifi(wifi_status_t state) {
    ui_event_t event = {
        .type            = UI_EVENT_SET_WIFI_STATE,
        .data.wifi_state = state,
    };
    enqueue_ui_event(&event);
}

void set_ui_state_battery(battery_level_t level) {
    ui_event_t event = {
        .type               = UI_EVENT_SET_BATTERY_LEVEL,
        .data.battery_level = level,
    };
    enqueue_ui_event(&event);
}

void set_ui_state_ota(ota_state_t state) {
    ui_event_t event = {
        .type           = UI_EVENT_OTA_STATE,
        .data.ota_state = state,
    };
    enqueue_ui_event(&event);
}

void notify_badge_config_updated() {
    ui_event_t event = {
        .type = UI_EVENT_BADGE_CONFIG_UPDATED,
    };
    enqueue_ui_event(&event);
}

void launch_app(content_app_t app) {
    ui_event_t event = {
        .type     = UI_EVENT_SET_APP,
        .data.app = app,
    };
    enqueue_ui_event(&event);
}

static void ui_task(void *_arg) {
    ui_event_t event;
    while (true) {
        if (xQueueReceive(ui_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            ESP_LOGD(TAG, "Received UI event: %s", ui_event_type_map[event.type]);
            switch (event.type) {
                case UI_EVENT_SET_SCREEN: //
                    ESP_LOGD(TAG, "Event type: %d, screen: %d", event.type, event.data.screen);
                    if (state.screen != event.data.screen) {
                        ESP_LOGD(TAG, "Setting screen to: %s", screen_labels[event.data.screen]);
                        update_screen(event.data.screen);
                    }
                    break;
                case UI_EVENT_SET_WIFI_STATE: //
                    ESP_LOGD(TAG, "Setting wifi state to: %d", event.data.wifi_state);
                    state.wifi_state = event.data.wifi_state;
                    if (state.screen == SCREEN_MAIN) {
                        render_main();
                    }
                    break;
                case UI_EVENT_SET_BATTERY_LEVEL: //
                    ESP_LOGD(TAG, "Setting battery level to: %d", event.data.battery_level);
                    state.battery_level = event.data.battery_level;
                    if (state.screen == SCREEN_MAIN) {
                        render_main();
                    }
                    break;
                case UI_EVENT_SET_APP: //
                    ESP_LOGD(TAG, "Setting content app to: %d", event.data.app);
                    set_current_app(event.data.app);
                    if (state.screen == SCREEN_MAIN) {
                        render_main();
                    }
                    break;
                case UI_EVENT_OTA_STATE: //
                    ESP_LOGD(TAG, "Setting OTA state");
                    ota_state_t ota_state = event.data.ota_state;
                    bool firmware_updating =
                        (ota_state.status == OTA_STATUS_DOWNLOADING || ota_state.status == OTA_STATUS_INSTALLING);

                    // If we're downloading/installing an OTA update, make sure we're on the update screen
                    if (firmware_updating && state.screen != SCREEN_UPDATE) {
                        set_screen(SCREEN_UPDATE);
                    }

                    // Send the updated OTA state to the update screen for rendering
                    if (state.screen == SCREEN_UPDATE) {
                        update_ota_state(ota_state);
                    }

                    if ((ota_state.status == OTA_STATUS_FAILED || ota_state.status == OTA_STATUS_SUCCESS) &&
                        state.screen == SCREEN_UPDATE) {
                        // If the update is complete, switch back to the main screen
                        set_screen(SCREEN_MAIN);
                    }

                    // Switch back to the main screen if we're not downloading or installing (usually due to an error)
                    if (!firmware_updating && state.screen == SCREEN_UPDATE) {
                        set_screen(SCREEN_MAIN);
                    }
                    break;
                case UI_EVENT_BADGE_CONFIG_UPDATED: //
                    ESP_LOGD(TAG, "Badge config updated, refreshing UI elements");
                    if (state.screen == SCREEN_MAIN) {
                        menu_update_faction_label();
                    }
                    break;
                default: //
                    ESP_LOGW(TAG, "Unknown UI event type: %d", event.type);
                    break;
            }
        }
    }
}
