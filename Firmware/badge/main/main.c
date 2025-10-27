#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "badge.h"
#include "battery.h"
#include "dawn_accord.h"
#include "display.h"
#include "i2c_manager.h"
#include "ops_nfc.h"
#include "minibadge.h"
#include "nvs.h"
#include "secure_element.h"
#include "ui.h"
#include "wifi_manager.h"
#include "led.h"

#include "cryptoauthlib.h"
#include <math.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

static const char *TAG = "main";

#ifndef BOOT_PROGRESS_MAX_BRIGHTNESS
    #define BOOT_PROGRESS_MAX_BRIGHTNESS 40
#endif
#ifndef BOOT_PROGRESS_HOLD_MS
    #define BOOT_PROGRESS_HOLD_MS 150
#endif
#ifndef BOOT_PROGRESS_FALLBACK_TOL_MS
    #define BOOT_PROGRESS_FALLBACK_TOL_MS 250
#endif

// -------------------------------------------------------------------------------------------------
// Boot Progress (Mirrored Bottom-Up Gradient: red -> yellow -> green)
// -------------------------------------------------------------------------------------------------
typedef struct {
    uint32_t total_steps;   // total boot progress steps
    uint32_t current_step;  // completed steps
    bool done;              // reached 100%
    uint32_t hold_ms;       // how long to hold before clearing
    uint64_t done_start_ms; // timestamp when we first hit 100%
    bool fading;
    uint64_t fade_start_ms;
    uint32_t fade_duration_ms;
} boot_progress_state_t;

static boot_progress_state_t boot_progress = {
    .total_steps      = 10,
    .current_step     = 0,
    .done             = false,
    .hold_ms          = BOOT_PROGRESS_HOLD_MS,
    .done_start_ms    = 0,
    .fading           = false,
    .fade_start_ms    = 0,
    .fade_duration_ms = 350,
};
static int boot_progress_layer = -1;

static void boot_progress_destroy_now() {
    if (boot_progress_layer >= 0) {
        int h               = boot_progress_layer;
        boot_progress_layer = -1;
        led_layer_destroy(h);
    }
}

static void boot_progress_start_fade(uint64_t now_ms) {
    if (boot_progress_layer < 0) {
        return;
    }
    if (!boot_progress.fading) {
        boot_progress.fading        = true;
        boot_progress.fade_start_ms = now_ms;
        led_layer_request_refresh();
    }
}

/**
 * @brief Boot-time deferred tasks to be run once the main screen is visible
 */
void badge_boot_deferred_tasks() {
    uint64_t now_ms = esp_timer_get_time() / 1000ULL;
    boot_progress_start_fade(now_ms);
}

static inline void boot_progress_set_total(uint32_t total) {
    if (total == 0) {
        total = 1;
    }
    boot_progress.total_steps = total;
    if (boot_progress.current_step > boot_progress.total_steps) {
        boot_progress.current_step = boot_progress.total_steps;
    }
}
static inline void boot_progress_step() {
    if (boot_progress.current_step < boot_progress.total_steps) {
        boot_progress.current_step++;
        if (boot_progress.current_step >= boot_progress.total_steps && !boot_progress.done) {
            boot_progress.done          = true;
            boot_progress.done_start_ms = esp_timer_get_time() / 1000ULL;
        }
        ESP_LOGD(TAG, "Boot progress step %u/%u", boot_progress.current_step, boot_progress.total_steps);
        led_layer_request_refresh();
    }
}
static void boot_progress_render(uint32_t now_ms, void *user_ctx) {
    (void)user_ctx;
    float ratio = (float)boot_progress.current_step / (float)boot_progress.total_steps;
    if (ratio >= 1.0f) {
        ratio = 1.0f;
        if (!boot_progress.done) {
            boot_progress.done          = true;
            boot_progress.done_start_ms = now_ms;
        }
    }
    const int levels = 10; // 10 vertical levels per side
    float filled     = ratio * levels;
    int full_levels  = (int)filled;
    float partial    = filled - full_levels;
    if (full_levels > levels) {
        full_levels = levels;
    }

    for (int lvl = 0; lvl < levels; lvl++) {
        // Color gradient from red (bottom) to green (top)
        float t = (float)lvl / (float)(levels - 1);
        uint8_t r, g, b;
        if (t < 0.5f) {
            float k = t / 0.5f;
            r       = 255;
            g       = (uint8_t)(k * 255.0f + 0.5f);
            b       = 0;
        } else {
            float k = (t - 0.5f) / 0.5f;
            r       = (uint8_t)((1.0f - k) * 255.0f + 0.5f);
            g       = 255;
            b       = 0;
        }

        float fill;
        if (lvl < full_levels) {
            fill = 1.0f;
        } else if (lvl == full_levels && full_levels < levels) {
            fill = partial;
        } else {
            fill = 0.0f;
        }

        float fade_factor = 1.0f;
        if (boot_progress.fading) {
            uint32_t fade_elapsed = (uint32_t)(now_ms - boot_progress.fade_start_ms);
            if (fade_elapsed >= boot_progress.fade_duration_ms) {
                fade_factor = 0.0f;
            } else {
                float t = (float)fade_elapsed / (float)boot_progress.fade_duration_ms; // 0..1
                // Smooth ease (accelerate end) using cosine: f = (1 - cos(pi * t)) / 2 then invert for fade-out
                float ease  = (1.0f - cosf((float)M_PI * t)) * 0.5f; // 0..1
                fade_factor = 1.0f - ease;
            }
        }
        uint8_t brightness = (uint8_t)(fill * (float)BOOT_PROGRESS_MAX_BRIGHTNESS * fade_factor);

        int left_index  = 9 - lvl;  // invert for left side physical mapping
        int right_index = 10 + lvl; // direct ascending for right side
        if (left_index >= 0 && left_index < NUM_LEDS) {
            led_set_with_brightness(left_index, r, g, b, brightness);
        }
        if (right_index >= 0 && right_index < NUM_LEDS) {
            led_set_with_brightness(right_index, r, g, b, brightness);
        }
    }

    if (boot_progress.fading) {
        uint32_t fade_elapsed = (uint32_t)(now_ms - boot_progress.fade_start_ms);
        if (fade_elapsed >= boot_progress.fade_duration_ms) {
            boot_progress_destroy_now();
            return;
        } else {
            led_layer_request_refresh();
        }
    }
}

void battery_monitor_task(void *pvParameters);

void app_main(void) {
    esp_err_t err;
    int64_t start_time = esp_timer_get_time();

    // Global log level override
    // esp_log_set_level_master(ESP_LOG_DEBUG);
    // esp_log_level_set("*", ESP_LOG_DEBUG);

    // Initialize LEDs first so progress is visible immediately
    if (led_init() == ESP_OK) {
        boot_progress_set_total(10); // we expect 10 steps
        boot_progress_layer = led_layer_create(LED_PRIORITY_MEDIUM, boot_progress_render, NULL, true, false, true);
        if (boot_progress_layer < 0) {
            ESP_LOGW(TAG, "Failed to create boot progress layer");
        }
    } else {
        ESP_LOGE(TAG, "Failed to initialize LEDs (boot progress unavailable)");
    }

    err = nvs_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
        return;
    }
    boot_progress_step();
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Install the ISR service if needed
    err = gpio_install_isr_service(ESP_INTR_FLAG_LOWMED);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
    }
    boot_progress_step();

    // Initialize the I2C manager
    err = i2c_manager_init_auto();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C manager: %s", esp_err_to_name(err));
        return;
    }
    boot_progress_step();

    // // DEBUG: Scan for all devices on the I2C bus and show their addresses
    // i2c_manager_device_criteria_t criteria = {
    //     .bus_index = I2C_BUS_OTHER, // Use the other bus for scanning
    //     .address   = 0,             // Scan all addresses
    // };
    // i2c_manager_device_config_t *found = NULL;
    // uint8_t device_count               = i2c_manager_discover(criteria, &found);
    // ESP_LOGI(TAG, "Discovered %d I2C devices:", device_count);
    // for (uint8_t i = 0; i < device_count; i++) {
    //     ESP_LOGI(TAG, "Bus %d, Address 0x%02X (0x%02X)", found[i].bus_index, found[i].config.device_address,
    //              found[i].config.device_address << 1);
    // }
    // if (found) {
    //     free(found);
    //     found = NULL;
    // }
    // // END DEBUG

    // Initialize the secure element
    err = se_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize secure element: %s", esp_err_to_name(err));
        return;
    }
    boot_progress_step();

    // Initialize the badge configuration
    err = badge_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize badge configuration: %s", esp_err_to_name(err));
        return;
    }
    boot_progress_step();

    // Initialize battery monitor
    err = battery_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize battery monitor: %s", esp_err_to_name(err));
    }
    boot_progress_step();

    // Initialize the display - includes input init and LVGL init
    display_init();

    // Wait for the display to be ready - maybe this should be a callback instead but display features and the UI won't work
    // without at minimum the lv_display object being created
    ESP_LOGI(TAG, "Waiting for display to be ready...");
    while (!display_ready()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "Display ready, continuing...");
    boot_progress_step();

    // Apply saved brightness level (with lower limit make sure it's not off)
    if (badge_config.screen_brightness <= 50) {
        badge_config.screen_brightness = LCD_BACKLIGHT_ON;
        save_badge_config();
    }
    set_backlight(badge_config.screen_brightness);

    // Initialize badge NFC handling (register callback, init NFC)
    if (badge_nfc_init() != ESP_OK) {
        ESP_LOGE(TAG, "Badge NFC init failed");
        return;
    }
    boot_progress_step();

    // Initialize the Dawn Accord
    err = dawn_accord_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Dawn Accord: %s", esp_err_to_name(err));
    }
    // boot_progress_step();

    // Initialize the minibadge handling
    err = minibadge_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize minibadge handling: %s", esp_err_to_name(err));
    }
    // boot_progress_step();

    // Initialize the UI
    ui_init();
    boot_progress_step();

    // Initialize the WiFi manager
    err = wifi_manager_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager: %s", esp_err_to_name(err));
        return;
    }
    set_ui_state_wifi(WIFI_STATUS_CONNECTING);
    boot_progress_step();

    // Now that all the boot steps are done, fade out the boot progress
    if (boot_progress_layer >= 0 && boot_progress.done && !boot_progress.fading) {
        uint64_t now_ms = esp_timer_get_time() / 1000ULL;
        if ((now_ms - boot_progress.done_start_ms) > (boot_progress.hold_ms + BOOT_PROGRESS_FALLBACK_TOL_MS)) {
            boot_progress_start_fade(now_ms);
        }
    }

    // Create battery monitoring task
    if (battery_event_group != NULL) {
        xTaskCreate(battery_monitor_task, "battery_monitor", 4096, NULL, 5, NULL);
        ESP_LOGI(TAG, "Battery monitor task created");
    }

    // Switch to the hwtest if not passed or home screen otherwise after it's been a few seconds since boot
    int64_t elapsed_time     = esp_timer_get_time() - start_time;
    const int64_t delay_time = 5000 - (elapsed_time / 1000);
    if (delay_time > 0) {
        vTaskDelay(pdMS_TO_TICKS(delay_time));
    }
    if (!badge_config.hw_pass) {
        set_screen(SCREEN_HWTEST);
    } else {
        set_screen(SCREEN_MAIN);
    }
}

void battery_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "Battery monitor task started");
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(battery_event_group, BATTERY_UPDATE, pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & BATTERY_UPDATE) {
            battery_status_t status = battery_get_status();
            set_ui_state_battery(status.level);
        }
    }
}