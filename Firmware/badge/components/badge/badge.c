#include <dirent.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "api.h"
#include "badge.h"
#include "badge/led_patterns.h"
#include "badge_game.h"
#include "dawn_accord.h"
#include "dawn_accord_violation.h"
#include "display.h"
#include "led.h"
#include "secure_element.h"
#include "nvs.h"
#include "ui.h"

static const char *TAG = "badge";

// For fun let's obscure the "Llama" and "Badge" strings which we'll use to decode the "real" SSID
static const uint8_t ssid_key[] = {
    'L' ^ 'B' ^ 0xA5, 'l' ^ 'a' ^ 0xA5, 'a' ^ 'd' ^ 0xA5, 'm' ^ 'g' ^ 0xA5, 'a' ^ 'e' ^ 0xA5,
};

// Badge state struct
badge_state_t badge_state = {
    .ready       = false,
    .wifi_status = WIFI_STATUS_DISCONNECTED,
    .provision_state =
        {
            .api_secrets = false,
            .wifi_creds  = false,
        },
    .ready_tasks_complete = false,
};

// Badge events to be handled by the main event badge task
#define BADGE_EVENT_QUEUE_SIZE      10
#define BADGE_EVENT_TASK_STACK_SIZE 8 * 1024

typedef enum {
    BADGE_EVENT_NONE,
    BADGE_EVENT_READY,
    BADGE_EVENT_WIFI_STATUS,
    BADGE_EVENT_OTA,
    BADGE_EVENT_OTA_CHECK,
    BADGE_EVENT_STATUS_SYNC,
} badge_event_type_t;

// Badge event data
typedef struct {
    badge_event_type_t type;
    union {
        wifi_status_t wifi_status;
        ota_state_t ota;
    } data;
} badge_event_t;

// Badge event queue
QueueHandle_t badge_event_queue = NULL;

// Periodic status sync
#define STATUS_SYNC_ACTIVE_INTERVAL_MS   (60 * 1000)     // 1 minute when active
#define STATUS_SYNC_INACTIVE_INTERVAL_MS (5 * 60 * 1000) // 5 minutes when inactive
static esp_timer_handle_t status_sync_timer = NULL;
static esp_timer_handle_t ota_splay_timer   = NULL;
static int64_t last_status_sync_time        = 0; // Timestamp of last sync (microseconds)

// Dawn Accord violation tracking
static dawn_accord_violation_t current_violation = DAWN_ACCORD_VIOLATION_NONE;
static lv_obj_t *violation_modal                 = NULL;
static lv_obj_t *cleared_modal                   = NULL;
static bool bypass_shown                         = false;
static bool dawn_accord_monitoring_started       = false;

// Dawn Accord pending reports (for offline operation)
typedef struct {
    bool has_pending_violation;
    bool has_pending_repair;
    bool inhibitor_enabled;
    bool strip_enabled;
} dawn_accord_pending_t;

static dawn_accord_pending_t pending_dawn_accord = {
    .has_pending_violation = false,
    .has_pending_repair    = false,
    .inhibitor_enabled     = false,
    .strip_enabled         = false,
};

// Forward declarations
static bool save_config_from_api(api_status_response_t *status);
static void trigger_immediate_status_sync();
static void screen_wake_handler();
static void dawn_accord_violation_handler(dawn_accord_violation_t violation, dawn_accord_state_t state);
static void dawn_accord_process_pending_reports();

// Badge event handling task
void badge_event_task(void *_args);

// Wifi status callback function
void wifi_status_callback(wifi_status_t wifi_status) {
    badge_event_t event = {.type = BADGE_EVENT_WIFI_STATUS, .data.wifi_status = wifi_status};
    xQueueSend(badge_event_queue, &event, 0);
}

void ota_state_callback(ota_state_t ota_state) {
    ESP_LOGD(TAG, "OTA state callback: %d", ota_state.status);
    set_ui_state_ota(ota_state);
    badge_event_t event = {.type = BADGE_EVENT_OTA, .data.ota = ota_state};
    xQueueSend(badge_event_queue, &event, 0);
}

// Set to 1 to enable color test mode
#define LED_COLOR_TEST_MODE       0
#define LED_COLOR_TEST_BRIGHTNESS 25

#if LED_COLOR_TEST_MODE
typedef struct {
    faction_id_t current;
    uint32_t last_switch_ms;
} color_cycle_state_t;
static color_cycle_state_t color_cycle_state = {0};

static void led_color_cycle_render_cb(uint32_t now_ms, void *user_ctx) {
    color_cycle_state_t *st = (color_cycle_state_t *)user_ctx;
    if (now_ms - st->last_switch_ms > 5000) {
        st->current        = (st->current + 1) % NUM_FACTIONS;
        st->last_switch_ms = now_ms;
    }
    faction_t info = get_faction(st->current);
    for (int i = 0; i < NUM_LEDS; i++) {
        led_set_with_brightness(i, info.led_color.red, info.led_color.green, info.led_color.blue, LED_COLOR_TEST_BRIGHTNESS);
    }
}

static void start_led_color_test() {
    color_cycle_state.current        = 0;
    color_cycle_state.last_switch_ms = esp_timer_get_time() / 1000ULL;
    if (led_color_cycle_layer_handle >= 0) {
        led_layer_set_active(led_color_cycle_layer_handle, true);
        led_layer_bump(led_color_cycle_layer_handle);
        return;
    }
    led_color_cycle_layer_handle =
        led_layer_create(LED_PRIORITY_LOW, led_color_cycle_render_cb, &color_cycle_state, true, false, true);
    if (led_color_cycle_layer_handle < 0) {
        ESP_LOGE(TAG, "Failed to create color cycle LED layer (err=%d)", led_color_cycle_layer_handle);
    }
}
#endif

esp_err_t badge_init() {
    esp_err_t err = load_badge_config();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Loading default badge config due to error");
        badge_config = BADGE_DEFAULTS;
    }
    ESP_LOGI(TAG,
             "Badge config: version=%lu, hw_pass=%d, registered=%d, screen_brightness=%d, screen_timeout=%lu, faction_leds=%d, "
             "player_name=%s, "
             "team_id=%d, team_name=%s, credits=%d, level=%d",
             badge_config.version, badge_config.hw_pass, badge_config.registered, badge_config.screen_brightness,
             badge_config.screen_timeout, badge_config.faction_leds, badge_config.player_name, badge_config.team_id,
             badge_config.team_name, badge_config.credits, badge_config.level);

    // Create the badge event queue
    badge_event_queue = xQueueCreate(BADGE_EVENT_QUEUE_SIZE, sizeof(badge_event_t));

    // Print the firmware version
    const esp_app_desc_t *app_desc = esp_app_get_description();
    ESP_LOGI(TAG,
             "Badge firmware information:\n"
             "  - Version: %s\n"
             "  - Project name: %s\n"
             "  - Compile time: %s\n"
             "  - Compile date: %s\n"
             "  - IDF version: %s\n"
             "  - SHA256: %s",
             app_desc->version, app_desc->project_name, app_desc->time, app_desc->date, app_desc->idf_ver,
             esp_app_get_elf_sha256_str());

    // Configure SPIFFS on the storage partition
    esp_vfs_spiffs_conf_t spiffs_conf = {
        .base_path              = "/spiffs",
        .partition_label        = NULL,
        .max_files              = 5,
        .format_if_mount_failed = true,
    };
    err = esp_vfs_spiffs_register(&spiffs_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(err));
    } else {
        // Print SPIFFS info
        size_t total_bytes, used_bytes;
        err = esp_spiffs_info(NULL, &total_bytes, &used_bytes);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get SPIFFS info (%s)", esp_err_to_name(err));
        } else {
            ESP_LOGD(TAG, "SPIFFS: %d/%d bytes used", used_bytes, total_bytes);
            if (esp_log_level_get(TAG) >= ESP_LOG_DEBUG) {
                // List files and their sizes in SPIFFS
                DIR *dir = opendir("/spiffs");
                if (dir == NULL) {
                    ESP_LOGE(TAG, "Failed to open SPIFFS directory");
                } else {
                    struct dirent *entry;
                    while ((entry = readdir(dir)) != NULL) {
                        struct stat st;
                        stat(entry->d_name, &st);
                        ESP_LOGD(TAG, "  - File: %s, Size: %ld", entry->d_name, st.st_size);
                    }
                    closedir(dir);
                }
            }
        }
    }

    // Check to see if we already have provisioned API secrets
    uint8_t slot_data[32] = {0};
    if ((err = se_read_slot(SE_KEY_HASH_PREFIX, slot_data, sizeof(slot_data))) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read hash prefix slot");
    }
    // If it was read and has been marked as touched then consider it provisioned
    else if (se_slot_touched(SE_KEY_HASH_PREFIX)) {
        badge_state.provision_state.api_secrets = true;
        ESP_LOGD(TAG, "HMAC secret and hash prefix are already provisioned");
        ESP_LOG_BUFFER_HEXDUMP(TAG, slot_data, sizeof(slot_data), ESP_LOG_DEBUG);
    }

    // See if we have WiFi credentials provisioned
    memset(slot_data, 0, sizeof(slot_data));
    if ((err = se_read_slot(SE_KEY_WIFI_SSID, slot_data, sizeof(slot_data))) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WiFi SSID slot");
    }
    // If it was read and has been marked as touched then we might consider it provisioned but in this case
    // we need to do a sanity check on the data since some badges have garbage in this slot from some unknown previous
    // write I'm guessing.
    else if (se_slot_touched(SE_KEY_WIFI_SSID)) {
        enum { SSID_LEN = sizeof(CONFIG_CONFERENCE_WIFI_SSID) - 1 };
        enum { OFFSET = 5 };
        const uint8_t compare_len = ((int)SSID_LEN > (int)OFFSET) ? (uint8_t)(SSID_LEN - OFFSET) : 0;
        bool differs              = false;
        if (compare_len > 0) {
            if (OFFSET + compare_len <= sizeof(slot_data)) {
                differs = memcmp(slot_data + OFFSET, (const uint8_t *)CONFIG_CONFERENCE_WIFI_SSID + OFFSET, compare_len) != 0;
            } else {
                differs = true;
            }
        }
        if (differs) {
            ESP_LOGW(TAG, "WiFi SSID slot data is invalid, clearing slot");
            se_clear_slot(SE_KEY_WIFI_SSID);
            badge_state.provision_state.wifi_creds = false;
        } else {
            badge_state.provision_state.wifi_creds = true;
        }
        ESP_LOGD(TAG, "WiFi credentials are already provisioned");
        ESP_LOG_BUFFER_HEXDUMP(TAG, slot_data, sizeof(slot_data), ESP_LOG_DEBUG);
    }

    // Check to see if we have the CTF password provisioned
    uint8_t big_slot_data[64] = {0};
    memset(big_slot_data, 0, sizeof(big_slot_data));
    if ((err = se_read_slot(SE_KEY_CTF_PASSWORD, big_slot_data, sizeof(big_slot_data))) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read CTF password slot");
    }
    // If it was read and has been marked as touched then consider it provisioned
    else if (se_slot_touched(SE_KEY_CTF_PASSWORD)) {
        badge_state.provision_state.ctf_password = true;
        ESP_LOGD(TAG, "CTF password is already provisioned");
        ESP_LOG_BUFFER_HEXDUMP(TAG, big_slot_data, sizeof(big_slot_data), ESP_LOG_DEBUG);
    }

    // Add wifi status callback
    add_wifi_status_callback(wifi_status_callback);

    // Initialize OTA and add a state callback
    ota_init();
    ota_add_state_callback(ota_state_callback);

    // Configure the screen timeout
    if (badge_config.screen_timeout > 0) {
        ESP_LOGD(TAG, "Configuring screen timeout: %d seconds", badge_config.screen_timeout);
        set_screen_timeout_config(SCREEN_TIMEOUT_DIM, 20, badge_config.screen_timeout * 1000);
        screen_timeout_start();
    }

    // Register screen wake callback to optimize status sync timing
    register_screen_wake_callback(screen_wake_handler);

    // Create the badge event handling task
    xTaskCreate(badge_event_task, "badge_event_task", BADGE_EVENT_TASK_STACK_SIZE, NULL, 5, NULL);

    // Send a ready event
    badge_event_t event = {.type = BADGE_EVENT_READY};
    xQueueSend(badge_event_queue, &event, 0);

    return err;
}

esp_err_t set_badge_handle(const char *handle) {
    if (strlen(handle) >= PLAYER_NAME_LENGTH) {
        ESP_LOGE(TAG, "Handle too long");
        return ESP_ERR_INVALID_ARG;
    }
    snprintf(badge_config.player_name, sizeof(badge_config.player_name), "%s", handle);
    return save_badge_config();
}

esp_err_t set_screen_timeout(uint32_t timeout) {
    ESP_LOGD(TAG, "Setting screen timeout: %ld", timeout);
    badge_config.screen_timeout = timeout;
    return save_badge_config();
}

/**
 * Provision API secrets from the server and store them in the secure element
 */
static bool provision_api() {
    esp_err_t err;

    // Check which slots are already locked in case of a retry scenario
    bool lock_states[NUM_API_SLOTS] = {false};
    bool any_locked                 = false;
    bool all_locked                 = true;
    uint8_t locked_count            = 0;
    for (size_t i = 0; i < NUM_API_SLOTS; i++) {
        if (se_slot_locked(api_secrets[i].key)) {
            ESP_LOGI(TAG, "%s slot already locked", api_secrets[i].name);
            lock_states[i] = true;
            any_locked     = true;
            locked_count++;
        } else {
            all_locked = false;
        }
    }

    // Rely exclusively on locked state of the slots rather than touched state
    if (all_locked) {
        ESP_LOGI(TAG, "All API secret slots already locked, skipping provisioning");
        badge_state.provision_state.api_secrets = true;
        return true;
    }
    // If some (but not all) slots are already locked we know a previous run reached post-verify locking.
    // In that case do NOT re-provision or re-verify with the server... just attempt to lock remaining slots.
    if (any_locked && !all_locked) {
        ESP_LOGI(TAG, "Some API secret slots locked (%d/%d). Attempting to lock remaining without re-provision...", locked_count,
                 NUM_API_SLOTS);
        all_locked = true;
        for (size_t i = 0; i < NUM_API_SLOTS; i++) {
            if (lock_states[i]) {
                continue;
            }
            ESP_LOGI(TAG, "Locking previously unlocked %s slot...", api_secrets[i].name);
            if ((err = se_lock_slot(api_secrets[i].key)) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to lock %s slot: %d", api_secrets[i].name, err);
                all_locked = false;
            } else {
                lock_states[i] = true;
            }
        }
        if (all_locked) {
            ESP_LOGI(TAG, "All API secret slots now locked after partial retry");
            badge_state.provision_state.api_secrets = true;
            return true;
        } else {
            ESP_LOGW(TAG, "Still have unlocked API secret slots... will retry on next attempt");
            return false;
        }
    }
    if (!any_locked) {
        ESP_LOGI(TAG, "No API secret slots locked, starting fresh provision");
        badge_state.provision_state.api_secrets = false;
    }

    // Attempt to provision the API secrets from the server
    api_result_t *result = api_provision_secrets();
    if (result == NULL) {
        ESP_LOGE(TAG, "Failed to get API secrets");
        return false;
    } else if (!result->ok) {
        ESP_LOGE(TAG, "Failed to get API secrets: %s", result->detail != NULL ? result->detail : "unknown error");
        api_free_result(result, true);
        return false;
    }

    // Get the data from the result so we can loop through all the secrets
    api_provision_response_t *data = (api_provision_response_t *)result->data;
    if (data == NULL) {
        ESP_LOGE(TAG, "No data in API provision response");
        api_free_result(result, true);
        return false;
    }
    struct {
        se_slot_id_t key;
        const char *name;
        const uint8_t *value;
        size_t value_len;
    } secrets[] = {
        // clang-format off
        {SE_KEY_HMAC,        "HMAC Secret",        data->hmac_secret,        sizeof(data->hmac_secret)},
        {SE_KEY_HASH_PREFIX, "Hash Prefix",        data->hash_prefix,        sizeof(data->hash_prefix)},
        {SE_KEY_HMAC_CTF,    "CTF HMAC Secret",    data->ctf_hmac_secret,    sizeof(data->ctf_hmac_secret)},
        {SE_KEY_HASH_CTF,    "CTF Hash Prefix",    data->ctf_hash_prefix,    sizeof(data->ctf_hash_prefix)},
        // clang-format on
    };
    static const uint8_t num_secrets = sizeof(secrets) / sizeof(secrets[0]);
    ESP_LOGD(TAG, "Badge provision response:");
    for (size_t i = 0; i < num_secrets; i++) {
        ESP_LOGD(TAG, "  - %s:", secrets[i].name);
        ESP_LOG_BUFFER_HEX(TAG, secrets[i].value, secrets[i].value_len);
    }

    // ---------------------------------------------------------------------------------------------
    // Write all secrets to the secure element in the correct slots
    // ---------------------------------------------------------------------------------------------
    for (size_t i = 0; i < num_secrets; i++) {
        ESP_LOGI(TAG, "Writing %s to secure element...", secrets[i].name);
        if ((err = se_write_slot(secrets[i].key, secrets[i].value, secrets[i].value_len)) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write %s to secure element: %d", secrets[i].name, err);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Verify that the contents of all slots were written correctly
    // ---------------------------------------------------------------------------------------------
    bool slots_ok = true;
    for (size_t i = 0; i < num_secrets; i++) {
        ESP_LOGI(TAG, "Verifying %s slot...", secrets[i].name);
        if ((err = se_validate_slot(secrets[i].key, secrets[i].value, secrets[i].value_len)) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to validate %s slot after writing: %d", secrets[i].name, err);
            slots_ok = false;
            break;
        }
    }
    if (slots_ok) {
        ESP_LOGI(TAG, "Successfully validated all slots after writing");
    }

    // ---------------------------------------------------------------------------------------------
    // Try to verify with the server
    // ---------------------------------------------------------------------------------------------
    bool verified = false;
    if (slots_ok) {
        api_result_t *verify_result = api_provision_verify();
        if (verify_result == NULL) {
            ESP_LOGE(TAG, "Failed to verify provisioning secrets");
        } else if (!verify_result->ok) {
            ESP_LOGE(TAG, "Failed to verify badge: %s", verify_result->detail != NULL ? verify_result->detail : "Unknown error");
            api_free_result(verify_result, true);
        } else {
            ESP_LOGI(TAG, "Successfully verified provisioning secrets");
            verified = true;
        }

        if (verified) {
            ESP_LOGI(TAG, "Successfully provisioned API secrets to secure element");
        }
    }

    // Clean up
    api_free_result(result, true);
    for (size_t i = 0; i < num_secrets; i++) {
        secrets[i].value     = NULL;
        secrets[i].value_len = 0;
    }

    // ---------------------------------------------------------------------------------------------
    // Lock the slots if everything was successful
    // ---------------------------------------------------------------------------------------------
    if (verified) {
        all_locked = true;
        for (size_t i = 0; i < num_secrets; i++) {
            ESP_LOGI(TAG, "Locking %s slot...", secrets[i].name);
            if ((err = se_lock_slot(secrets[i].key)) != ESP_OK) {
                all_locked = false;
                ESP_LOGE(TAG, "Failed to lock %s slot: %d", secrets[i].name, err);
                break; // stop trying to lock if even one fails
            } else {
                lock_states[i] = true;
                any_locked     = true;
            }
        }
    }
    if (all_locked) {
        ESP_LOGI(TAG, "Successfully locked all API secret slots");
    }

    // If we failed at any step, erase any slots that we wrote to that aren't locked... but only if none were locked
    // to avoid erasing a potentially valid provisioning
    if (!verified || !any_locked) {
        ESP_LOGW(TAG, "API secrets were not successfully provisioned and verified... erasing any already "
                      "stored secrets");
        for (size_t i = 0; i < sizeof(secrets) / sizeof(secrets[0]); i++) {
            if (!se_slot_locked(secrets[i].key)) {
                ESP_LOGI(TAG, "Erasing %s slot...", secrets[i].name);
                if ((err = se_clear_slot(secrets[i].key)) != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to erase %s slot: %d", secrets[i].name, err);
                }
            }
        }
        return false;
    }

    return (badge_state.provision_state.api_secrets = verified && all_locked);
}

/**
 * Provision WiFi credentials from the server and store them in the secure element
 */
static bool provision_wifi() {
    if (badge_state.provision_state.wifi_creds) {
        ESP_LOGD(TAG, "WiFi credentials already provisioned, skipping");
        return true;
    }
    ESP_LOGI(TAG, "Provisioning WiFi credentials...");

    // Attempt to provision the WiFi credentials from the server
    api_result_t *result = api_provision_wireless_psk();
    if (result == NULL) {
        ESP_LOGE(TAG, "Failed to get WiFi credentials");
        return false;
    } else if (!result->ok) {
        ESP_LOGE(TAG, "Failed to get WiFi credentials: %s", result->detail != NULL ? result->detail : "unknown error");
        api_free_result(result, true);
        return false;
    }

    // Get the data from the result so we can loop through all the secrets
    api_provision_psk_response_t *data = (api_provision_psk_response_t *)result->data;
    if (data == NULL) {
        ESP_LOGE(TAG, "No data in API WiFi provision response");
        api_free_result(result, true);
        return false;
    }
    ESP_LOGD(TAG, "WiFi provision response:");
    ESP_LOGD(TAG, "  - Password: %s", data->psk);
    // Get the SSID by replacing 'Llama' with 'Badge' in the default SSID
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "%s", CONFIG_CONFERENCE_WIFI_SSID);
    for (size_t i = 0; i < 5; i++) {
        ssid[i] = ssid[i] ^ ssid_key[i] ^ 0xA5;
    }
    ESP_LOGD(TAG, "  - SSID: %s", ssid);

    // ---------------------------------------------------------------------------------------------
    // Write the WiFi creds to the corresponding slots
    // ---------------------------------------------------------------------------------------------
    if (se_write_slot(SE_KEY_WIFI_SSID, (const uint8_t *)ssid, sizeof(ssid)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write WiFi SSID to secure element");
    }
    if (se_write_slot(SE_KEY_WIFI_PASSWORD, (const uint8_t *)data->psk, sizeof(data->psk)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write WiFi PSK to secure element");
    }

    // Clean up
    api_free_result(result, true);

    return (badge_state.provision_state.wifi_creds = true);
}

// -------------------------------------------------------------------------------------------------
// Periodic Status Sync
// -------------------------------------------------------------------------------------------------

static void status_sync_timer_callback(void *arg) {
    badge_event_t event = {.type = BADGE_EVENT_STATUS_SYNC};
    xQueueSend(badge_event_queue, &event, 0);
}

static void start_status_sync_timer() {
    if (status_sync_timer == NULL) {
        esp_timer_create_args_t timer_args = {
            .callback = status_sync_timer_callback,
            .arg      = NULL,
            .name     = "status_sync",
        };
        esp_timer_create(&timer_args, &status_sync_timer);
    }

    uint64_t interval_us =
        is_screen_timed_out() ? STATUS_SYNC_INACTIVE_INTERVAL_MS * 1000ULL : STATUS_SYNC_ACTIVE_INTERVAL_MS * 1000ULL;

    ESP_LOGD(TAG, "Starting status sync timer with interval: %llu ms", interval_us / 1000);
    esp_timer_stop(status_sync_timer);
    esp_timer_start_periodic(status_sync_timer, interval_us);
}

static void stop_status_sync_timer() {
    if (status_sync_timer) {
        esp_timer_stop(status_sync_timer);
    }
}

static void trigger_immediate_status_sync() {
    badge_event_t event = {.type = BADGE_EVENT_STATUS_SYNC};
    xQueueSend(badge_event_queue, &event, 0);
}

static void screen_wake_handler() {
    int64_t now                  = esp_timer_get_time();
    int64_t time_since_last_sync = now - last_status_sync_time;
    int64_t active_interval_us   = STATUS_SYNC_ACTIVE_INTERVAL_MS * 1000ULL;

    ESP_LOGD(TAG, "Screen woke up, time since last sync: %lld ms", time_since_last_sync / 1000);

    // If it's been more than the active interval since last sync, trigger immediate sync
    if (time_since_last_sync >= active_interval_us) {
        ESP_LOGI(TAG, "Screen woke up and sync overdue, triggering immediate sync");
        trigger_immediate_status_sync();
    } else {
        // Otherwise just restart the timer with active interval
        ESP_LOGD(TAG, "Screen woke up, restarting timer with active interval");
        start_status_sync_timer();
    }
}

static void ota_splay_timer_callback(void *arg) {
    ESP_LOGI(TAG, "OTA splay timer expired, triggering OTA check");
    ota_check();
}

static void handle_status_sync_response(api_status_response_t *status) {
    if (!status) {
        return;
    }

    bool config_changed = save_config_from_api(status);

    ESP_LOGD(TAG, "Config changed: %s", config_changed ? "YES" : "NO");

    if (config_changed) {
        ESP_LOGI(TAG, "Badge config updated from periodic sync");

        // Update faction LED pattern based on current config
        if (badge_config.faction_leds && badge_config.team_id <= NUM_FACTIONS && badge_config.sorting_hat) {
            ESP_LOGD(TAG, "Restarting faction LED pattern due to config change");
            led_pattern_faction_stop();
            led_pattern_faction_start();
        } else {
            // Faction LEDs disabled or requirements not met - ensure pattern is stopped
            ESP_LOGD(TAG, "Stopping faction LED pattern (disabled or requirements not met)");
            led_pattern_faction_stop();
        }

        // Notify UI to update
        if (ui_ready()) {
            notify_badge_config_updated();
        }
    }

    // Handle firmware update availability with random delay splay (0-15 minutes)
    if (status->firmware_info.update_available) {
        uint32_t splay_delay_ms = esp_random() % (15 * 60 * 1000); // 0-15 minutes
        ESP_LOGI(TAG, "Firmware update available, will trigger OTA check in %lu ms (%.1f minutes)", splay_delay_ms,
                 splay_delay_ms / 60000.0f);

        // Create one-shot timer to trigger OTA check after splay delay
        if (ota_splay_timer) {
            esp_timer_stop(ota_splay_timer);
            esp_timer_delete(ota_splay_timer);
            ota_splay_timer = NULL;
        }

        esp_timer_create_args_t timer_args = {
            .callback = ota_splay_timer_callback,
            .arg      = NULL,
            .name     = "ota_splay",
        };
        if (esp_timer_create(&timer_args, &ota_splay_timer) == ESP_OK) {
            esp_timer_start_once(ota_splay_timer, splay_delay_ms * 1000ULL);
        } else {
            ESP_LOGE(TAG, "Failed to create OTA splay timer");
        }
    }

    // TODO: Handle messages when message UI is implemented
    if (status->has_messages) {
        ESP_LOGI(TAG, "Badge has unread messages (message UI not yet implemented)");
    }
}

/**
 * Save badge configuration from API status response.
 */
static bool save_config_from_api(api_status_response_t *status) {
    bool changed = false;

    ESP_LOGD(TAG, "Player status:");
    ESP_LOGD(TAG, "  - Player Name: %s", status->player_name ? status->player_name : "(null)");
    ESP_LOGD(TAG, "  - Team ID: %d", status->team_id);
    ESP_LOGD(TAG, "  - Team Name: %s", status->team_name ? status->team_name : "(none)");
    ESP_LOGD(TAG, "  - Credits: %d", status->credits);
    ESP_LOGD(TAG, "  - Level: %d", status->level);
    ESP_LOGD(TAG, "  - Registered At: %s", status->registered_at ? status->registered_at : "(null)");
    ESP_LOGD(TAG, "  - Sorting Hat: %s", status->sorting_hat ? "Yes" : "No");
    ESP_LOGD(TAG, "  - Has Messages: %s", status->has_messages ? "Yes" : "No");
    ESP_LOGD(TAG, "  - Firmware:");
    if (status->firmware_info.current_version && status->firmware_info.latest_version) {
        ESP_LOGD(TAG, "      Current Version: %s", status->firmware_info.current_version);
        ESP_LOGD(TAG, "      Latest Version: %s", status->firmware_info.latest_version);
        ESP_LOGD(TAG, "      Update Available: %d", status->firmware_info.update_available);
    } else {
        ESP_LOGD(TAG, "      Firmware info not available");
    }

    if (!status->player_name) {
        ESP_LOGW(TAG, "No player name in status response... skipping config update");
        return false;
    }

    if (strlen(status->player_name) == 0) {
        ESP_LOGW(TAG, "Empty player name in status response");
        badge_config.registered = false;
    } else if (status->player_name != NULL && strncmp(badge_config.player_name, status->player_name, PLAYER_NAME_LENGTH) != 0) {
        snprintf(badge_config.player_name, sizeof(badge_config.player_name), "%s", status->player_name);
        badge_config.registered = true;
        changed                 = true;
    }
    if (status->team_id != badge_config.team_id) {
        badge_config.team_id = status->team_id;
        changed              = true;
    }
    if (status->team_name != NULL && strncmp(badge_config.team_name, status->team_name, TEAM_NAME_LENGTH) != 0) {
        snprintf(badge_config.team_name, sizeof(badge_config.team_name), "%s", status->team_name);
        changed = true;
    }
    if (status->credits != badge_config.credits) {
        badge_config.credits = status->credits;
        changed              = true;
    }
    if (status->level != badge_config.level) {
        badge_config.level = status->level;
        changed            = true;
    }
    if (status->sorting_hat != badge_config.sorting_hat) {
        badge_config.sorting_hat = status->sorting_hat;
        changed                  = true;
    }

    if (changed) {
        ESP_LOGI(TAG,
                 "Saving updated badge config from API: player_name=%s, team_id=%d, team_name=%s, credits=%d, level=%d, "
                 "sorting_hat=%s",
                 badge_config.player_name, badge_config.team_id, badge_config.team_name, badge_config.credits, badge_config.level,
                 badge_config.sorting_hat ? "Yes" : "No");
        save_badge_config();
        return true;
    }

    return false;
}

bool try_status_sync() {
    if (!badge_state.ready) {
        ESP_LOGD(TAG, "Badge component not initialized, skipping status sync");
        return false;
    }
    if (badge_state.wifi_status != WIFI_STATUS_CONNECTED) {
        ESP_LOGD(TAG, "WiFi not connected (status=%d), cannot perform API requests", badge_state.wifi_status);
        return false;
    }
    if (!badge_state.provision_state.api_secrets) {
        ESP_LOGD(TAG, "API secrets not provisioned, cannot do API requests");
        return false;
    }

    // Attempt to get status from the server
    //   For a registered badge this should work and should be harmless otherwise but will return an error
    api_result_t *result = api_get_status();
    if (result == NULL) {
        ESP_LOGE(TAG, "Failed to get badge status");
        return false;
    } else if (!result->ok) {
        ESP_LOGE(TAG, "Failed to get badge status: %s", result->detail != NULL ? result->detail : "Unknown error");
        api_free_result(result, true);
        return false;
    } else {
        ESP_LOGD(TAG, "Successfully got badge status from server");
        api_status_response_t *status = (api_status_response_t *)result->data;
        if (status != NULL) {
            save_config_from_api(status);
            api_free_result(result, true);
            return true;
        }
    }
    api_free_result(result, true);

    return false;
}

// -------------------------------------------------------------------------------------------------
// Dawn Accord Violation Handler
// -------------------------------------------------------------------------------------------------

/**
 * Handles Dawn Accord violation state changes.
 * Manages LED patterns, UI modals, and API reporting based on violation state.
 * Works offline and queues reports for when WiFi is available.
 */
static void dawn_accord_violation_handler(dawn_accord_violation_t violation, dawn_accord_state_t state) {
    bool inhibitor_enabled = (state.ai_inhibitor != AI_INHIBIT_NONE);
    bool strip_enabled     = state.security_strip;

    ESP_LOGI(TAG, "Dawn Accord violation handler: violation=%d, inhibitor=%d, strip=%d", violation, inhibitor_enabled,
             strip_enabled);

    // Check if we should skip showing modals (OTA or hwtest screen active)
    ui_state_t ui_state = get_ui_state();
    bool skip_modal     = (ui_state.screen == SCREEN_UPDATE || ui_state.screen == SCREEN_HWTEST);

    // Handle violation → no violation transition (cleared)
    if (current_violation != DAWN_ACCORD_VIOLATION_NONE && violation == DAWN_ACCORD_VIOLATION_NONE) {
        ESP_LOGI(TAG, "Dawn Accord violation cleared");

        // Stop LED pattern
        led_pattern_dawn_accord_violation_stop();

        // Close violation modal if open
        if (violation_modal) {
            if (lvgl_lock(portMAX_DELAY, __FILE__, __LINE__)) {
                dawn_accord_modal_close(violation_modal);
                violation_modal = NULL;
                lvgl_unlock(__FILE__, __LINE__);
            }
        }

        // Show "cleared" success modal unless on OTA/hwtest screen
        if (!skip_modal && ui_ready()) {
            if (lvgl_lock(portMAX_DELAY, __FILE__, __LINE__)) {
                cleared_modal = dawn_accord_violation_cleared_show();
                lvgl_unlock(__FILE__, __LINE__);
            }
        }

        // Queue repair report if WiFi not connected, otherwise send immediately
        if (badge_state.wifi_status == WIFI_STATUS_CONNECTED && badge_state.provision_state.api_secrets) {
            api_result_t *result = api_report_dawn_accord(inhibitor_enabled, strip_enabled);
            if (result) {
                if (!result->ok) {
                    ESP_LOGW(TAG, "Failed to report Dawn Accord cleared state: %s",
                             result->detail ? result->detail : "Unknown error");
                    // Queue for retry
                    pending_dawn_accord.has_pending_repair = true;
                    pending_dawn_accord.inhibitor_enabled  = inhibitor_enabled;
                    pending_dawn_accord.strip_enabled      = strip_enabled;
                } else {
                    pending_dawn_accord.has_pending_repair = false;
                }
                api_free_result(result, true);
            }
        } else {
            ESP_LOGI(TAG, "Queueing Dawn Accord repair report (WiFi not connected)");
            pending_dawn_accord.has_pending_repair = true;
            pending_dawn_accord.inhibitor_enabled  = inhibitor_enabled;
            pending_dawn_accord.strip_enabled      = strip_enabled;
        }

        current_violation = violation;
        return;
    }

    // Handle no violation → violation transition (new violation)
    if (current_violation == DAWN_ACCORD_VIOLATION_NONE && violation != DAWN_ACCORD_VIOLATION_NONE) {
        ESP_LOGI(TAG, "New Dawn Accord violation detected: %d", violation);

        // Start LED pattern
        led_pattern_dawn_accord_violation_start();

        // Close any existing cleared modal before showing violation modal
        if (cleared_modal) {
            if (lvgl_lock(portMAX_DELAY, __FILE__, __LINE__)) {
                dawn_accord_modal_close(cleared_modal);
                cleared_modal = NULL;
                lvgl_unlock(__FILE__, __LINE__);
            }
        }

        // Show violation modal unless on OTA/hwtest screen
        if (!skip_modal && ui_ready()) {
            if (lvgl_lock(portMAX_DELAY, __FILE__, __LINE__)) {
                violation_modal = dawn_accord_violation_show(violation);
                lvgl_unlock(__FILE__, __LINE__);
            }
        }

        // Queue violation report if WiFi not connected, otherwise send immediately
        if (badge_state.wifi_status == WIFI_STATUS_CONNECTED && badge_state.provision_state.api_secrets) {
            api_result_t *result = api_report_dawn_accord(inhibitor_enabled, strip_enabled);
            if (result) {
                if (!result->ok) {
                    ESP_LOGW(TAG, "Failed to report Dawn Accord violation: %s",
                             result->detail ? result->detail : "Unknown error");
                    // Queue for retry
                    pending_dawn_accord.has_pending_violation = true;
                    pending_dawn_accord.inhibitor_enabled     = inhibitor_enabled;
                    pending_dawn_accord.strip_enabled         = strip_enabled;
                } else {
                    pending_dawn_accord.has_pending_violation = false;
                }
                api_free_result(result, true);
            }
        } else {
            ESP_LOGI(TAG, "Queueing Dawn Accord violation report (WiFi not connected)");
            pending_dawn_accord.has_pending_violation = true;
            pending_dawn_accord.inhibitor_enabled     = inhibitor_enabled;
            pending_dawn_accord.strip_enabled         = strip_enabled;
        }

        current_violation = violation;
        return;
    }

    // Handle both protections disabled (bypass success)
    if (!inhibitor_enabled && !strip_enabled && !bypass_shown) {
        ESP_LOGI(TAG, "Dawn Accord bypass detected (both protections disabled)");

        // Show one-time bypass modal unless on OTA/hwtest screen
        if (!skip_modal && ui_ready()) {
            if (lvgl_lock(portMAX_DELAY, __FILE__, __LINE__)) {
                dawn_accord_bypass_show();
                lvgl_unlock(__FILE__, __LINE__);
            }
        }

        // Queue bypass report if WiFi not connected, otherwise send immediately
        if (badge_state.wifi_status == WIFI_STATUS_CONNECTED && badge_state.provision_state.api_secrets) {
            api_result_t *result = api_report_dawn_accord(inhibitor_enabled, strip_enabled);
            if (result) {
                if (!result->ok) {
                    ESP_LOGW(TAG, "Failed to report Dawn Accord bypass state: %s",
                             result->detail ? result->detail : "Unknown error");
                } else {
                    bypass_shown = true;
                }
                api_free_result(result, true);
            }
        } else {
            ESP_LOGI(TAG, "Dawn Accord bypass detected but WiFi not connected (will report when online)");
            bypass_shown = true; // Show modal but remember to report later
        }
    }

    // Reset bypass flag if either protection is re-enabled
    if ((inhibitor_enabled || strip_enabled) && bypass_shown) {
        bypass_shown = false;
    }

    current_violation = violation;
}

/**
 * Process any pending Dawn Accord reports that were queued while offline.
 * Called when WiFi connects and API secrets are available.
 */
static void dawn_accord_process_pending_reports() {
    if (!badge_state.provision_state.api_secrets) {
        ESP_LOGD(TAG, "API secrets not available, cannot process pending Dawn Accord reports");
        return;
    }

    // Process pending violation report
    if (pending_dawn_accord.has_pending_violation) {
        ESP_LOGI(TAG, "Processing queued Dawn Accord violation report");
        api_result_t *result = api_report_dawn_accord(pending_dawn_accord.inhibitor_enabled, pending_dawn_accord.strip_enabled);
        if (result) {
            if (result->ok) {
                ESP_LOGI(TAG, "Successfully reported queued violation");
                pending_dawn_accord.has_pending_violation = false;
            } else {
                ESP_LOGW(TAG, "Failed to report queued violation: %s", result->detail ? result->detail : "Unknown error");
            }
            api_free_result(result, true);
        }
    }

    // Process pending repair report
    if (pending_dawn_accord.has_pending_repair) {
        ESP_LOGI(TAG, "Processing queued Dawn Accord repair report");
        api_result_t *result = api_report_dawn_accord(pending_dawn_accord.inhibitor_enabled, pending_dawn_accord.strip_enabled);
        if (result) {
            if (result->ok) {
                ESP_LOGI(TAG, "Successfully reported queued repair");
                pending_dawn_accord.has_pending_repair = false;
            } else {
                ESP_LOGW(TAG, "Failed to report queued repair: %s", result->detail ? result->detail : "Unknown error");
            }
            api_free_result(result, true);
        }
    }
}

// -------------------------------------------------------------------------------------------------
// Badge Ready Tasks
// -------------------------------------------------------------------------------------------------

void badge_check_ready_tasks() {
    // Check if we've already completed
    if (badge_state.ready_tasks_complete) {
        ESP_LOGD(TAG, "Badge ready tasks already completed, skipping");
        return;
    }

    // Check required conditions to do API calls for everything
    if (!badge_state.ready) {
        ESP_LOGD(TAG, "Badge component not initialized, skipping ready tasks");
        return;
    }
    if (badge_state.wifi_status != WIFI_STATUS_CONNECTED) {
        ESP_LOGD(TAG, "WiFi not connected (status=%d), cannot perform API requests", badge_state.wifi_status);
        return;
    }
    if (!badge_state.provision_state.api_secrets) {
        ESP_LOGD(TAG, "API secrets not provisioned, cannot do API requests");
        return;
    }
    if (get_ui_state().screen != SCREEN_MAIN) {
        ESP_LOGD(TAG, "Main screen not loaded (screen=%d), skipping ready tasks", get_ui_state().screen);
        return;
    }

    ESP_LOGI(TAG, "Running ready state tasks...");

    // Check if CTF password is provisioned
    if (badge_state.provision_state.ctf_password) {
        ESP_LOGD(TAG, "CTF password is provisioned");
        if (esp_log_level_get(TAG) >= ESP_LOG_DEBUG) {
            uint8_t slot_data[64] = {0};
            if (se_read_slot(SE_KEY_CTF_PASSWORD, slot_data, sizeof(slot_data)) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to read CTF password slot");
            } else {
                ESP_LOGD(TAG, "CTF Password:");
                ESP_LOG_BUFFER_HEXDUMP(TAG, slot_data, sizeof(slot_data), ESP_LOG_DEBUG);
            }
        }
    } else {
        ESP_LOGD(TAG, "CTF password not provisioned, attempting to provision...");
        api_result_t *result = api_get_ctf_hints_password();
        if (result == NULL) {
            ESP_LOGE(TAG, "Failed to get CTF hints password");
        } else if (!result->ok) {
            ESP_LOGE(TAG, "Failed to get CTF hints password: %s", result->detail != NULL ? result->detail : "Unknown error");
        } else {
            ESP_LOGD(TAG, "Successfully got CTF hints password");
            api_ctf_hints_password_response_t *ctf_hints_password_data = (api_ctf_hints_password_response_t *)result->data;
            if (ctf_hints_password_data != NULL) {
                ESP_LOGD(TAG, "CTF Hints Password: %s", ctf_hints_password_data->password);
                uint8_t slot_data[SLOT_CAP(SE_KEY_CTF_PASSWORD)] = {0};
                snprintf((char *)slot_data, sizeof(slot_data), "%s", ctf_hints_password_data->password);
                if (se_write_slot(SE_KEY_CTF_PASSWORD, (const uint8_t *)slot_data, sizeof(slot_data)) != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to write CTF hints password to secure element");
                } else {
                    badge_state.provision_state.ctf_password = true;
                    ESP_LOGI(TAG, "Successfully saved CTF hints password to the slot");
                }
            }
        }
        api_free_result(result, true);
    }

    // -----------------------------------------------------------------
    // Sync badge data from the server
    // -----------------------------------------------------------------
    if (try_status_sync()) {
        ESP_LOGI(TAG, "Badge status synced from server");
#if LED_COLOR_TEST_MODE
        start_led_color_test();
#else
        // In order to show the faction LED pattern, the badge must be registered, have faction LEDs enabled,
        // have a valid team ID, and have been assigned their faction (e.g. visited the sorting hat)
        if (badge_config.faction_leds && badge_config.team_id <= NUM_FACTIONS && badge_config.sorting_hat) {
            ESP_LOGD(TAG, "Starting faction LED pattern for faction %s", get_faction(badge_config.team_id).name);
            led_pattern_faction_start();
        } else {
            ESP_LOGD(TAG, "Faction LEDs disabled in config, skipping LED pattern start");
        }
#endif
        // Update UI to reflect synced config (especially faction label)
        if (ui_ready()) {
            notify_badge_config_updated();
        }
    }

    // -----------------------------------------------------------------
    // Register the badge if not already done
    // -----------------------------------------------------------------
    else {
        ESP_LOGD(TAG, "Badge not registered, attempting registration...");
        if (strlen(badge_config.player_name) > 0) {
            ESP_LOGD(TAG, "Player name is set, registering with name: %s", badge_config.player_name);
            api_result_t *result = api_register(badge_config.player_name);
            if (result == NULL) {
                ESP_LOGE(TAG, "Failed to register badge");
            } else if (!result->ok) {
                ESP_LOGE(TAG, "Failed to register badge: %s", result->detail != NULL ? result->detail : "Unknown error");
            } else {
                ESP_LOGD(TAG, "Successfully registered badge");
                api_status_response_t *status = (api_status_response_t *)result->data;
                if (status != NULL) {
                    save_config_from_api(status);
                }
            }
            api_free_result(result, true);
        } else {
            ESP_LOGD(TAG, "Player name not set, skipping registration");
        }
    }

    // -----------------------------------------------------------------
    // Check for OTA updates
    // -----------------------------------------------------------------
    ESP_LOGD(TAG, "Running OTA check...");
    ota_check();

    // Mark as completed
    badge_state.ready_tasks_complete = true;
    ESP_LOGI(TAG, "Initial badge sync and OTA check completed");

    // Process any pending Dawn Accord reports that were queued while offline
    dawn_accord_process_pending_reports();
}

void badge_event_task(void *_args) {
    badge_event_t event;
    while (1) {
        if (xQueueReceive(badge_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event.type) {
                case BADGE_EVENT_READY: //
                    badge_state.ready = true;

                    // Start Dawn Accord monitoring as soon as badge is ready
                    // This allows violation detection to work even without WiFi
                    if (!dawn_accord_monitoring_started) {
                        ESP_LOGI(TAG, "Starting Dawn Accord monitoring (UI ready)...");
                        esp_err_t da_err = dawn_accord_start_monitoring(dawn_accord_violation_handler);
                        if (da_err == ESP_ERR_INVALID_STATE) {
                            ESP_LOGD(TAG, "Dawn Accord not initialized yet, will retry when WiFi connects");
                        } else if (da_err != ESP_OK) {
                            ESP_LOGE(TAG, "Failed to start Dawn Accord monitoring: %s", esp_err_to_name(da_err));
                        } else {
                            ESP_LOGI(TAG, "Dawn Accord monitoring started successfully");
                            dawn_accord_monitoring_started = true;
                        }
                    }
                    break;
                case BADGE_EVENT_WIFI_STATUS:
                    // Update the wifi status in our state
                    badge_state.wifi_status = event.data.wifi_status;

                    // Update the UI with the current wifi status
                    if (ui_ready()) {
                        set_ui_state_wifi(badge_state.wifi_status);
                    }

                    ESP_LOGD(TAG, "Wifi status changed: %d", badge_state.wifi_status);
                    ESP_LOGD(TAG, "Badge state: ready=%d, wifi_status=%d, provision_state.api_secrets=%d", badge_state.ready,
                             badge_state.wifi_status, badge_state.provision_state.api_secrets);

                    // Things to do when wifi is connected
                    if (badge_state.wifi_status == WIFI_STATUS_CONNECTED) {
                        // Retry starting Dawn Accord monitoring if it failed during BADGE_EVENT_READY
                        // (dawn_accord_init may not have completed yet during first boot)
                        if (!dawn_accord_monitoring_started) {
                            ESP_LOGI(TAG, "Retrying Dawn Accord monitoring start (WiFi connected)...");
                            esp_err_t da_err = dawn_accord_start_monitoring(dawn_accord_violation_handler);
                            if (da_err == ESP_OK) {
                                ESP_LOGI(TAG, "Dawn Accord monitoring started successfully");
                                dawn_accord_monitoring_started = true;
                            } else if (da_err != ESP_ERR_INVALID_STATE) {
                                ESP_LOGE(TAG, "Failed to start Dawn Accord monitoring: %s", esp_err_to_name(da_err));
                            }
                        }

                        // Determine if we are connected to the provisioning SSID or the real conference one
                        bool is_provisioning = true;
                        wifi_ap_record_t current_ap_info;
                        if (esp_wifi_sta_get_ap_info(&current_ap_info) == ESP_OK) {
                            if (strcmp((char *)current_ap_info.ssid, CONFIG_CONFERENCE_WIFI_SSID) == 0) {
                                ESP_LOGD(TAG, "Connected to provisioning SSID (%s)", CONFIG_CONFERENCE_WIFI_SSID);
                                is_provisioning = true;
                            } else {
                                ESP_LOGD(TAG, "Connected to real SSID (%s), proceeding with ready tasks", current_ap_info.ssid);
                                is_provisioning = false;
                            }
                        } else {
                            ESP_LOGE(TAG, "Failed to get current WiFi config");
                        }

                        ESP_LOGD(TAG, "WiFi is connected, processing badge tasks...");
                        if (badge_state.ready) {
                            ESP_LOGD(TAG, "Badge is ready, checking API secrets...");
                            bool wifi_before = badge_state.provision_state.wifi_creds;
                            bool provisioned = provision_api() && provision_wifi();
                            bool wifi_now    = badge_state.provision_state.wifi_creds;
                            if (provisioned) {
                                ESP_LOGD(TAG, "Badge successfully provisioned");
                            } else if (is_provisioning) {
                                ESP_LOGD(TAG, "Badge not yet provisioned, still connected to provisioning SSID");
                            }

                            // If WiFi creds have just been provisioned this iteration, force reconnect and skip further tasks
                            if (!wifi_before && wifi_now) {
                                ESP_LOGI(TAG, "New WiFi credentials provisioned... forcing reconnection to conference network");
                                wifi_force_reconnect();
                                break;
                            }

                            // We can't make API calls until we have the API secrets provisioned
                            if (badge_state.provision_state.api_secrets) {
                                ESP_LOGD(TAG, "API secrets are provisioned, attempting badge sync and OTA check...");
                                badge_check_ready_tasks();

                                // Start periodic status sync after initial ready tasks
                                if (badge_state.ready_tasks_complete) {
                                    start_status_sync_timer();
                                }

                                // Process any pending Dawn Accord reports
                                dawn_accord_process_pending_reports();
                            } else {
                                ESP_LOGD(TAG, "API secrets not provisioned, skipping badge sync and OTA check");
                            }
                        } else {
                            ESP_LOGW(TAG, "Badge not ready, skipping badge registration and update check");
                        }
                    } else {
                        ESP_LOGD(TAG, "WiFi not connected (status=%d), skipping badge tasks", badge_state.wifi_status);
                        // Stop periodic sync when WiFi disconnects
                        stop_status_sync_timer();
                    }
                    break;
                case BADGE_EVENT_OTA: //
                    ESP_LOGD(TAG, "OTA event message: %s", event.data.ota.message);
                    break;
                case BADGE_EVENT_STATUS_SYNC:
                    ESP_LOGD(TAG, "Periodic status sync triggered");
                    if (badge_state.wifi_status == WIFI_STATUS_CONNECTED && badge_state.provision_state.api_secrets) {
                        api_result_t *result = api_get_status();
                        if (result && result->ok) {
                            api_status_response_t *status = (api_status_response_t *)result->data;
                            handle_status_sync_response(status);
                            api_free_result(result, true);

                            // Record sync time
                            last_status_sync_time = esp_timer_get_time();
                        } else {
                            ESP_LOGW(TAG, "Periodic status sync failed: %s",
                                     result && result->detail ? result->detail : "Unknown error");
                            if (result) {
                                api_free_result(result, true);
                            }
                        }

                        // Restart timer with appropriate interval based on screen state
                        start_status_sync_timer();
                    } else {
                        ESP_LOGD(TAG, "Skipping status sync - WiFi not connected or not provisioned");
                        stop_status_sync_timer();
                    }
                    break;
                default: //
                    ESP_LOGW(TAG, "Unknown event type: %d", event.type);
                    break;
            }
        }
    }
}
