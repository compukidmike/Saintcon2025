#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nfc.h"
#include "redeem_code.h"

#include "badge.h"
#include "badge/led_patterns.h"
#include "badge/ops_nfc.h"
#include "badge_game.h"
#include "ui.h"

static const char *TAG = "badge/ops/nfc";

// No-op: success/error LED patterns are now synchronized in the flow when the modal opens.
static void redeem_flow_cb(int button_idx, const api_result_t *result, void *user_data) {
    (void)button_idx;
    (void)user_data;
    (void)result;
}

// Deferred UI update callback for sorting hat
static void sorting_hat_ui_update_cb(lv_timer_t *timer) {
    (void)timer;
    notify_badge_config_updated();
    lv_timer_delete(timer);
}

// Sorting Hat callback: updates badge config and triggers UI refresh
static void sorting_hat_redeem_cb(int button_idx, const api_result_t *result, void *user_data) {
    (void)button_idx;
    (void)user_data;

    if (result && result->ok) {
        badge_config.sorting_hat = true;
        save_badge_config();

        // Trigger faction LED pattern if enabled and requirements met
        if (badge_config.faction_leds && badge_config.team_id <= NUM_FACTIONS && badge_config.sorting_hat) {
            ESP_LOGI(TAG, "Starting faction LED pattern after Sorting Hat assignment");
            led_pattern_faction_stop();
            led_pattern_faction_start();
        }

        // Defer UI update to avoid modifying LVGL objects during event handling
        if (ui_ready()) {
            lv_timer_create(sorting_hat_ui_update_cb, 10, NULL);
        }
    }
}

static void handle_tag_write(void) {
    nfc_ndef_record_t *records = NULL;
    size_t count               = 0;
    nfc_err_t err              = nfc_get_records(&records, &count);
    if (err != NFC_OK) {
        ESP_LOGE(TAG, "Failed to get NDEF records: %s", nfc_err_to_string(err));
        return;
    }
    if (count == 0) {
        ESP_LOGI(TAG, "No NDEF records found");
        nfc_free_records(records, count);
        return;
    }
    ESP_LOGI(TAG, "NFC tag has %zu NDEF record(s)", count);
    bool had_decode_error = false;
    char *found_code      = NULL;
    nut_type_t nut_type   = NUT_TYPE_UNKNOWN;
    for (size_t i = 0; i < count; i++) {
        if (records[i].payload && records[i].payload_len > 0) {
            char *text     = NULL;
            char *lang     = NULL;
            nfc_err_t terr = nfc_decode_text_record(&records[i], &text, &lang);
            if (terr == NFC_OK && text) {
                ESP_LOGI(TAG, "Decoded text record: lang='%s', text='%s'", lang ? lang : "(null)", text);
                size_t len = strlen(text);
                if (len == 5 && !found_code) {
                    found_code = strdup(text);
                } else if (len > 3 && nut_type == NUT_TYPE_UNKNOWN && strncmp(text, "NT:", 3) == 0) {
                    nut_type = get_nut_type_from_str(text + 3);
                    ESP_LOGI(TAG, "Detected nut type: %s (%d)", get_nut_type_label(nut_type), nut_type);
                } else if (len != 5) {
                    ESP_LOGW(TAG, "NFC code has invalid length: %zu", len);
                    had_decode_error = true;
                }
            } else {
                ESP_LOGW(TAG, "Failed to decode record: %s", nfc_err_to_string(terr));
                had_decode_error = true;
            }
            if (text) {
                free(text);
            }
            if (lang) {
                free(lang);
            }
        }
    }
    nfc_free_records(records, count);

    // Trigger decode error flash if there was any decode error and we didn't find a code
    if (had_decode_error && !found_code) {
        led_pattern_flash_red();
    }

    // If we have a code, build UI overrides based on nut type and redeem (note that we can't fit more than ~15 characters in the
    // titles)
    if (found_code) {
        redeem_ui_texts_t ui = {0};
        switch (nut_type) {
            case NUT_TYPE_TELECOM:
                ui.spinner_text  = "Attempting telecom node unlock...";
                ui.success_title = "Node Unlocked";
                ui.failure_title = "Unlock Failed";
                break;
            case NUT_TYPE_COMMUNITY:
                ui.spinner_text  = "Sending community level...";
                ui.success_title = "Level Redeemed";
                ui.failure_title = "Code Error";
                break;
            case NUT_TYPE_CONTEST:
                ui.spinner_text  = "Submitting contest code...";
                ui.success_title = "Code Accepted";
                ui.failure_title = "Code Error";
                break;
            case NUT_TYPE_EVENT:
                ui.spinner_text  = "Redeeming event code...";
                ui.success_title = "Code Accepted";
                ui.failure_title = "Code Error";
                break;
            case NUT_TYPE_VENDOR:
                ui.spinner_text  = "Redeeming vendor code...";
                ui.success_title = "Code Accepted";
                ui.failure_title = "Code Error";
                break;
            case NUT_TYPE_SORTING_HAT:
                ui.spinner_text  = "Consulting the Sorting Hat...";
                ui.success_title = "Assignment";
                ui.failure_title = "Error";
                break;
            default: break;
        }
        esp_err_t derr = try_redeem_code_ctx(found_code, 5, nut_type != NUT_TYPE_UNKNOWN ? &ui : NULL,
                                             nut_type == NUT_TYPE_SORTING_HAT ? sorting_hat_redeem_cb : redeem_flow_cb, NULL);
        if (derr != ESP_OK) {
            ESP_LOGW(TAG, "Redeem flow rejected code: %d", (int)derr);
            led_pattern_flash_red();
        }
        free(found_code);
    }

    // Overwrite tag with WiFi config placeholder for the conference attendee WiFi
    vTaskDelay(pdMS_TO_TICKS(100));
    nfc_err_t clear_err = nfc_write_wifi_config("SAINTCON25", "SAINTCON25");
    if (clear_err != NFC_OK) {
        ESP_LOGW(TAG, "Failed to write WiFi placeholder: %s", nfc_err_to_string(clear_err));
    } else {
        ESP_LOGD(TAG, "Tag WiFi placeholder written");
    }
}

static void badge_nfc_event_cb(nfc_event_t event, const uint8_t *data, size_t len, void *arg) {
    (void)data;
    (void)len;
    (void)arg;
    switch (event) {
        case NFC_EVENT_FIELD_DETECTED: ESP_LOGI(TAG, "NFC field detected"); break;
        case NFC_EVENT_FIELD_LOST: ESP_LOGI(TAG, "NFC field lost"); break;
        case NFC_EVENT_TAG_READ: ESP_LOGI(TAG, "NFC tag read"); break;
        case NFC_EVENT_TAG_WRITE:
            ESP_LOGI(TAG, "NFC tag write");
            handle_tag_write();
            break;
        case NFC_EVENT_WRITE_BLOCKED: ESP_LOGW(TAG, "NFC write blocked due to rate limiting"); break;
        case NFC_EVENT_ERROR: ESP_LOGE(TAG, "NFC error occurred"); break;
        default: ESP_LOGW(TAG, "Unknown NFC event: %d", event); break;
    }
}

esp_err_t badge_nfc_init(void) {
    nfc_set_event_callback(badge_nfc_event_cb, NULL);
    nfc_default_record_t def = {
        .type      = NFC_DEFAULT_WIFI,
        .text      = NULL,
        .url       = NULL,
        .wifi_ssid = "SAINTCON25",
        .wifi_key  = "SAINTCON25",
    };
    nfc_err_t nerr = nfc_init_with_default(&def);
    if (nerr != NFC_OK) {
        ESP_LOGE(TAG, "Failed to initialize NFC: %s", nfc_err_to_string(nerr));
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Badge NFC initialized");
    return ESP_OK;
}

void badge_nfc_deinit(void) {
    nfc_deinit();
}
