#include <dirent.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "api.h"
#include "nut.h"
#include "nut/code_manager.h"
#include "led.h"
#include "secure_element.h"
#include "nvs.h"
#include "i2c_manager.h"
#include "wifi_manager.h"

static const char *TAG = "nut";

// ------------------------------------------------------------------------------------------------
// Nut Configuration
// ------------------------------------------------------------------------------------------------

// For fun let's obscure the "Llama" and "Nut" strings which we'll use to decode the "real" SSID
static const uint8_t ssid_key[] = {
    'L' ^ 'B' ^ 0xA5, 'l' ^ 'a' ^ 0xA5, 'a' ^ 'd' ^ 0xA5, 'm' ^ 'g' ^ 0xA5, 'a' ^ 'e' ^ 0xA5,
};

// Nut state struct
nut_state_t nut_state = {
    .ready          = false,
    .network_status = NETWORK_STATUS_DISCONNECTED,
    .network_type   = NETWORK_TYPE_NONE,
    .provision_state =
        {
            .api_secrets = false,
            .wifi_creds  = false,
        },
    .ready_tasks_complete = false,
};

// Nut events to be handled by the main event nut task
#define NUT_EVENT_QUEUE_SIZE      10
#define NUT_EVENT_TASK_STACK_SIZE 8 * 1024

typedef enum {
    NUT_EVENT_NONE,
    NUT_EVENT_READY,
    NUT_EVENT_NETWORK_STATUS,
    NUT_EVENT_OTA,
    NUT_EVENT_OTA_CHECK,
    NUT_EVENT_STATUS_SYNC,
    NUT_EVENT_HEARTBEAT,
} nut_event_type_t;

// Nut event data
typedef struct {
    nut_event_type_t type;
    union {
        struct {
            network_status_t status;
            network_type_t type;
        } network;
        ota_state_t ota;
    } data;
} nut_event_t;

// Nut event queue
QueueHandle_t nut_event_queue = NULL;

// Periodic status sync and heartbeat
#define STATUS_SYNC_INTERVAL_MS (10 * 60 * 1000) // 10 minutes
#define HEARTBEAT_INTERVAL_MS   (60 * 1000)      // 1 minute
static esp_timer_handle_t status_sync_timer = NULL;
static esp_timer_handle_t heartbeat_timer   = NULL;
static int64_t last_status_sync_time        = 0; // Timestamp of last sync (microseconds)
static int64_t last_heartbeat_time          = 0; // Timestamp of last heartbeat (microseconds)

// Nut event handling task
void nut_event_task(void *_args);

// Network status callback function
void network_status_callback(network_status_t status, network_type_t type) {
    nut_event_t event = {.type = NUT_EVENT_NETWORK_STATUS, .data.network = {.status = status, .type = type}};
    xQueueSend(nut_event_queue, &event, 0);
}

void ota_state_callback(ota_state_t ota_state) {
    ESP_LOGD(TAG, "OTA state callback: %d", ota_state.status);
    nut_event_t event = {.type = NUT_EVENT_OTA, .data.ota = ota_state};
    xQueueSend(nut_event_queue, &event, 0);
}

// Timer callbacks
static void status_sync_timer_callback(void *arg) {
    nut_event_t event = {.type = NUT_EVENT_STATUS_SYNC};
    xQueueSend(nut_event_queue, &event, 0);
}

static void heartbeat_timer_callback(void *arg) {
    nut_event_t event = {.type = NUT_EVENT_HEARTBEAT};
    xQueueSend(nut_event_queue, &event, 0);
}

// Timer management functions
static void start_status_sync_timer() {
    if (status_sync_timer == NULL) {
        esp_timer_create_args_t timer_args = {
            .callback = status_sync_timer_callback,
            .arg      = NULL,
            .name     = "status_sync",
        };
        esp_timer_create(&timer_args, &status_sync_timer);
    }

    uint64_t interval_us = STATUS_SYNC_INTERVAL_MS * 1000ULL;
    esp_timer_stop(status_sync_timer);
    esp_timer_start_periodic(status_sync_timer, interval_us);
    ESP_LOGI(TAG, "Status sync timer started (interval: %d minutes)", STATUS_SYNC_INTERVAL_MS / 60000);
}

static void stop_status_sync_timer() {
    if (status_sync_timer) {
        esp_timer_stop(status_sync_timer);
        ESP_LOGD(TAG, "Status sync timer stopped");
    }
}

static void start_heartbeat_timer() {
    if (heartbeat_timer == NULL) {
        esp_timer_create_args_t timer_args = {
            .callback = heartbeat_timer_callback,
            .arg      = NULL,
            .name     = "heartbeat",
        };
        esp_timer_create(&timer_args, &heartbeat_timer);
    }

    uint64_t interval_us = HEARTBEAT_INTERVAL_MS * 1000ULL;
    esp_timer_stop(heartbeat_timer);
    esp_timer_start_periodic(heartbeat_timer, interval_us);
    ESP_LOGI(TAG, "Heartbeat timer started (interval: %d seconds)", HEARTBEAT_INTERVAL_MS / 1000);
}

static void stop_heartbeat_timer() {
    if (heartbeat_timer) {
        esp_timer_stop(heartbeat_timer);
        ESP_LOGD(TAG, "Heartbeat timer stopped");
    }
}

esp_err_t nut_detect_hardware(bool has_w5500) {
    ESP_LOGI(TAG, "Detecting hardware configuration...");

    // Scan I2C bus for MCP23017 (typically at 0x20-0x27)
    bool has_mcp23017 = false;

    // Scan I2C bus 0 for devices
    uint8_t found_devices[16] = {0};
    uint8_t device_count      = 0;

    esp_err_t err = i2c_manager_scan(0, found_devices, &device_count);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Found %d devices on I2C bus 0", device_count);

        // Check if any found devices are in the MCP23017 address range (0x20-0x27)
        for (uint8_t i = 0; i < device_count; i++) {
            ESP_LOGD(TAG, "I2C device found at address 0x%02X", found_devices[i]);
            if (found_devices[i] >= 0x20 && found_devices[i] <= 0x27) {
                ESP_LOGI(TAG, "Found I2C device at address 0x%02X - potential MCP23017", found_devices[i]);
                has_mcp23017 = true;
            }
        }
    } else {
        ESP_LOGW(TAG, "Failed to scan I2C bus 0: %s", esp_err_to_name(err));
    }

    // Determine hardware type based on components found
    nut_hw_type_t detected_hw_type = NUT_HW_TYPE_UNKNOWN;
    nut_type_t detected_type       = NUT_TYPE_UNKNOWN;

    if (has_mcp23017) {
        // MCP23017 present - assume this is TELECOM hardware with W5500 ethernet
        // (avoids SPI conflicts by not probing W5500 directly)
        detected_hw_type = NUT_HW_TYPE_TELECOM;
        detected_type    = NUT_TYPE_TELECOM;
        if (has_w5500) {
            ESP_LOGI(TAG, "Detected TELECOM hardware: MCP23017 + W5500 confirmed present");
        } else {
            ESP_LOGI(TAG, "Detected TELECOM hardware: MCP23017 present (assuming W5500 also present)");
        }
    } else {
        // No MCP23017 - this is wireless-only hardware
        detected_hw_type = NUT_HW_TYPE_WIRELESS;
        ESP_LOGI(TAG, "Detected WIRELESS hardware: no I/O expander detected");
    }

    // Update configuration if hardware type changed
    bool config_changed = false;

    if (nut_config.hw_type != detected_hw_type) {
        ESP_LOGI(TAG, "Hardware type changed from %s to %s", nut_config.hw_type == NUT_HW_TYPE_TELECOM ? "TELECOM" : "WIRELESS",
                 detected_hw_type == NUT_HW_TYPE_TELECOM ? "TELECOM" : "WIRELESS");
        nut_config.hw_type = detected_hw_type;
        config_changed     = true;
    }

    // Only set type if it's currently UNKNOWN - otherwise preserve API-assigned type
    if (nut_config.type == NUT_TYPE_UNKNOWN) {
        ESP_LOGI(TAG, "Setting initial nut type to %s based on hardware detection", get_nut_type_str(detected_type));
        nut_config.type = detected_type;
        config_changed  = true;
    }

    if (config_changed) {
        // Save updated configuration
        esp_err_t err = save_nut_config();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save updated hardware configuration: %s", esp_err_to_name(err));
            return err;
        }
    }

    return ESP_OK;
}

esp_err_t nut_init() {
    // Config already loaded by nut_detect_hardware(), just log it
    ESP_LOGI(TAG, "Nut config: version=%lu, hw_type=%s, type=%s, nut_id=%s, enabled=%s", nut_config.version,
             nut_config.hw_type == NUT_HW_TYPE_TELECOM ? "TELECOM" : "WIRELESS", get_nut_type_str(nut_config.type),
             nut_config.nut_id, nut_config.enabled ? "true" : "false");

    esp_err_t err;

    // Create the nut event queue
    nut_event_queue = xQueueCreate(NUT_EVENT_QUEUE_SIZE, sizeof(nut_event_t));

    // Print the firmware version
    const esp_app_desc_t *app_desc = esp_app_get_description();
    ESP_LOGI(TAG,
             "Nut firmware information:\n"
             "  - Version: %s\n"
             "  - Project name: %s\n"
             "  - Compile time: %s\n"
             "  - Compile date: %s\n"
             "  - IDF version: %s\n"
             "  - SHA256: %s",
             app_desc->version, app_desc->project_name, app_desc->time, app_desc->date, app_desc->idf_ver,
             esp_app_get_elf_sha256_str());

    // Check to see if we already have provisioned API secrets
    uint8_t slot_data[32] = {0};
    if ((err = se_read_slot(SE_KEY_HASH_PREFIX, slot_data, sizeof(slot_data))) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read hash prefix slot");
    }
    // If it was read and has been marked as touched then consider it provisioned
    else if (se_slot_touched(SE_KEY_HASH_PREFIX)) {
        nut_state.provision_state.api_secrets = true;
        ESP_LOGD(TAG, "HMAC secret and hash prefix are already provisioned");
        ESP_LOG_BUFFER_HEXDUMP(TAG, slot_data, sizeof(slot_data), ESP_LOG_DEBUG);
    }

    // See if we have WiFi credentials provisioned
    memset(slot_data, 0, sizeof(slot_data));
    if ((err = se_read_slot(SE_KEY_WIFI_SSID, slot_data, sizeof(slot_data))) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WiFi SSID slot");
    }
    // If it was read and has been marked as touched then we might consider it provisioned but in this case
    // we need to do a sanity check on the data since some nuts have garbage in this slot from some unknown previous
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
            nut_state.provision_state.wifi_creds = false;
        } else {
            nut_state.provision_state.wifi_creds = true;
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
        nut_state.provision_state.ctf_password = true;
        ESP_LOGD(TAG, "CTF password is already provisioned");
        ESP_LOG_BUFFER_HEXDUMP(TAG, big_slot_data, sizeof(big_slot_data), ESP_LOG_DEBUG);
    }

    // Add network status callback
    err = network_manager_add_status_callback(network_status_callback);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register network status callback: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Network status callback registered successfully");

    // Initialize OTA and add a state callback
    ota_init();
    ota_add_state_callback(ota_state_callback);

    // Initialize code manager
    code_mgr_init();

    // Create the nut event handling task
    xTaskCreate(nut_event_task, "nut_event_task", NUT_EVENT_TASK_STACK_SIZE, NULL, 5, NULL);

    // Send a ready event
    nut_event_t event = {.type = NUT_EVENT_READY};
    xQueueSend(nut_event_queue, &event, 0);

    return err;
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
        nut_state.provision_state.api_secrets = true;
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
            nut_state.provision_state.api_secrets = true;
            return true;
        } else {
            ESP_LOGW(TAG, "Still have unlocked API secret slots... will retry on next attempt");
            return false;
        }
    }
    if (!any_locked) {
        ESP_LOGI(TAG, "No API secret slots locked, starting fresh provision");
        nut_state.provision_state.api_secrets = false;
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
        {SE_KEY_HMAC,        "HMAC Secret", data->hmac_secret, sizeof(data->hmac_secret)},
        {SE_KEY_HASH_PREFIX, "Hash Prefix", data->hash_prefix, sizeof(data->hash_prefix)},
        // clang-format on
    };
    static const uint8_t num_secrets = sizeof(secrets) / sizeof(secrets[0]);
    ESP_LOGD(TAG, "Nut provision response:");
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
            ESP_LOGE(TAG, "Failed to verify nut: %s", verify_result->detail != NULL ? verify_result->detail : "Unknown error");
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

    return (nut_state.provision_state.api_secrets = verified && all_locked);
}

/**
 * Provision WiFi credentials from the server and store them in the secure element
 */
static bool provision_wifi() {
    if (nut_state.provision_state.wifi_creds) {
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
        if (result->detail && strstr(result->detail, "PSK already retrieved") != NULL) {
            ESP_LOGD(TAG, "WiFi PSK already retrieved from server, credentials should be in secure element");
            api_free_result(result, true);
            return (nut_state.provision_state.wifi_creds = true);
        }
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

    return (nut_state.provision_state.wifi_creds = true);
}

/**
 * Save nut configuration from API status response.
 */
static bool save_config_from_api(api_status_response_t *status) {
    bool changed = false;

    // Debug print the status response
    ESP_LOGD(TAG, "Nut Status:");
    ESP_LOGD(TAG, "  - Nut ID: %s", status->nut_id ? status->nut_id : "(null)");
    ESP_LOGD(TAG, "  - Nut Type: %s", get_nut_type_str(status->nut_type));
    ESP_LOGD(TAG, "  - Enabled: %s", status->enabled ? "Yes" : "No");
    ESP_LOGD(TAG, "  - Provisioned: %s", status->provisioned ? "Yes" : "No");
    ESP_LOGD(TAG, "  - Vendor ID: %d", status->vendor_id);
    ESP_LOGD(TAG, "  - Community ID: %d", status->community_id);
    ESP_LOGD(TAG, "  - Contest ID: %d", status->contest_id);
    ESP_LOGD(TAG, "  - Event ID: %d", status->event_id);
    ESP_LOGD(TAG, "  - Garbage ID: %d", status->garbage_id);
    ESP_LOGD(TAG, "  - System ID: %d", status->system_id);
    ESP_LOGD(TAG, "  - Node ID: %d", status->node_id);
    if (status->provisioned_at != API_TIMESTAMP_NULL) {
        char provisioned_str[32];
        struct tm timeinfo;
        localtime_r(&status->provisioned_at, &timeinfo);
        strftime(provisioned_str, sizeof(provisioned_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
        ESP_LOGD(TAG, "  - Provisioned At: %s", provisioned_str);
    } else {
        ESP_LOGD(TAG, "  - Provisioned At: (null)");
    }
    if (status->last_seen != API_TIMESTAMP_NULL) {
        char last_seen_str[32];
        struct tm timeinfo;
        localtime_r(&status->last_seen, &timeinfo);
        strftime(last_seen_str, sizeof(last_seen_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
        ESP_LOGD(TAG, "  - Last Seen: %s", last_seen_str);
    } else {
        ESP_LOGD(TAG, "  - Last Seen: (null)");
    }

    ESP_LOGD(TAG, "  - Firmware:");
    if (status->firmware_info.current_version && status->firmware_info.latest_version) {
        ESP_LOGD(TAG, "      Current Version: %s", status->firmware_info.current_version);
        ESP_LOGD(TAG, "      Latest Version: %s", status->firmware_info.latest_version);
        ESP_LOGD(TAG, "      Update Available: %d", status->firmware_info.update_available);
    } else {
        ESP_LOGD(TAG, "      Firmware info not available");
    }

    if (!status->nut_id) {
        ESP_LOGW(TAG, "No nut ID in status response... skipping config update");
        return false;
    }
    if (strncmp(nut_config.nut_id, status->nut_id, NUT_ID_LENGTH) != 0) {
        snprintf(nut_config.nut_id, sizeof(nut_config.nut_id), "%s", status->nut_id);
        changed = true;
    }
    if (nut_config.enabled != status->enabled) {
        nut_config.enabled = status->enabled;
        changed            = true;
    }
    if (status->nut_type != nut_config.type) {
        nut_config.type = status->nut_type;
        changed         = true;
    }
    if (nut_config.node_id != status->node_id) {
        nut_config.node_id = status->node_id;
        changed            = true;
    }

    if (changed) {
        ESP_LOGI(TAG, "Saving updated nut config from API: nut_id=%s, type=%s, enabled=%s", nut_config.nut_id,
                 get_nut_type_str(nut_config.type), nut_config.enabled ? "Yes" : "No");
        esp_err_t err = save_nut_config();
        if (err == ESP_OK) {
            // Update the nut type record in code manager if the type changed
            code_mgr_update_nut_type_record();
        }
        return err == ESP_OK;
    }

    return changed;
}

bool try_status_sync() {
    if (!nut_state.ready) {
        ESP_LOGD(TAG, "Nut component not initialized, skipping status sync");
        return false;
    }
    if (nut_state.network_status != NETWORK_STATUS_CONNECTED) {
        ESP_LOGD(TAG, "Network not connected (status=%d), cannot perform API requests", nut_state.network_status);
        return false;
    }
    if (!nut_state.provision_state.api_secrets) {
        ESP_LOGD(TAG, "API secrets not provisioned, cannot do API requests");
        return false;
    }

    // Attempt to get status from the server
    //   For a registered nut this should work and should be harmless otherwise but will return an error
    api_result_t *result = api_get_status();
    if (result == NULL) {
        ESP_LOGE(TAG, "Failed to get nut status");
        return false;
    } else if (!result->ok) {
        ESP_LOGE(TAG, "Failed to get nut status: %s", result->detail != NULL ? result->detail : "Unknown error");
        api_free_result(result, true);
        return false;
    } else {
        ESP_LOGD(TAG, "Successfully got nut status from server");
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

void nut_check_ready_tasks() {
    // Check if we've already completed
    if (nut_state.ready_tasks_complete) {
        ESP_LOGD(TAG, "Nut ready tasks already completed, skipping");
        return;
    }

    // Check required conditions to do API calls for everything
    if (!nut_state.ready) {
        ESP_LOGD(TAG, "Nut component not initialized, skipping ready tasks");
        return;
    }
    if (nut_state.network_status != NETWORK_STATUS_CONNECTED) {
        ESP_LOGD(TAG, "Network not connected (status=%d), cannot perform API requests", nut_state.network_status);
        return;
    }
    if (!nut_state.provision_state.api_secrets) {
        ESP_LOGD(TAG, "API secrets not provisioned, cannot do API requests");
        return;
    }

    ESP_LOGI(TAG, "Running ready state tasks...");

    // -----------------------------------------------------------------
    // Sync nut data from the server
    // -----------------------------------------------------------------
    if (try_status_sync()) {
        ESP_LOGI(TAG, "Nut status synced from server");
#if LED_COLOR_TEST_MODE
        // start_led_color_test();
#else
        // start_faction_led_pattern();
#endif
    }

    // -----------------------------------------------------------------
    // Check for OTA updates
    // -----------------------------------------------------------------
    ESP_LOGD(TAG, "Running OTA check...");
    ota_check();

    // Mark as completed
    nut_state.ready_tasks_complete = true;
    ESP_LOGI(TAG, "Initial nut sync and OTA check completed");

    code_mgr_on_nut_ready();
}

void nut_event_task(void *_args) {
    nut_event_t event;
    while (1) {
        if (xQueueReceive(nut_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event.type) {
                case NUT_EVENT_READY: //
                    nut_state.ready = true;
                    break;
                case NUT_EVENT_NETWORK_STATUS:
                    // Update the network status in our state
                    nut_state.network_status = event.data.network.status;
                    nut_state.network_type   = event.data.network.type;

                    ESP_LOGD(TAG, "Network status changed: %d (type=%d)", nut_state.network_status, nut_state.network_type);
                    ESP_LOGD(TAG, "Nut state: ready=%d, network_status=%d, provision_state.api_secrets=%d", nut_state.ready,
                             nut_state.network_status, nut_state.provision_state.api_secrets);

                    // Things to do when network is connected
                    if (nut_state.network_status == NETWORK_STATUS_CONNECTED) {
                        // Determine if we are connected to the provisioning SSID or the real conference one
                        // Only check this for WiFi connections
                        bool is_provisioning = true;
                        if (nut_state.network_type == NETWORK_TYPE_WIFI) {
                            wifi_ap_record_t current_ap_info;
                            if (esp_wifi_sta_get_ap_info(&current_ap_info) == ESP_OK) {
                                if (strcmp((char *)current_ap_info.ssid, CONFIG_CONFERENCE_WIFI_SSID) == 0) {
                                    ESP_LOGD(TAG, "Connected to provisioning SSID (%s)", CONFIG_CONFERENCE_WIFI_SSID);
                                    is_provisioning = true;
                                } else {
                                    ESP_LOGD(TAG, "Connected to real SSID (%s), proceeding with ready tasks",
                                             current_ap_info.ssid);
                                    is_provisioning = false;
                                }
                            } else {
                                ESP_LOGE(TAG, "Failed to get current WiFi config");
                            }
                        } else if (nut_state.network_type == NETWORK_TYPE_ETHERNET) {
                            ESP_LOGD(TAG, "Connected via Ethernet, proceeding with ready tasks");
                            is_provisioning = false;
                        }

                        ESP_LOGD(TAG, "Network is connected, processing nut tasks...");
                        if (nut_state.ready) {
                            ESP_LOGD(TAG, "Nut is ready, checking API secrets...");
                            bool wifi_before = nut_state.provision_state.wifi_creds;
                            bool provisioned = provision_api() && provision_wifi();
                            bool wifi_now    = nut_state.provision_state.wifi_creds;
                            if (provisioned) {
                                ESP_LOGD(TAG, "Nut successfully provisioned");
                            } else if (is_provisioning) {
                                ESP_LOGD(TAG, "Nut not yet provisioned, still connected to provisioning SSID");
                            }

                            // If WiFi creds have just been provisioned this iteration, force reconnect and skip further tasks
                            if (network_manager_is_connected() && network_manager_get_primary_type() != NETWORK_TYPE_ETHERNET &&
                                !wifi_before && wifi_now) {
                                ESP_LOGI(TAG, "New WiFi credentials provisioned... forcing reconnection to conference network");
                                network_manager_force_reconnect();
                                break;
                            }

                            // We can't make API calls until we have the API secrets provisioned
                            if (nut_state.provision_state.api_secrets) {
                                ESP_LOGD(TAG, "API secrets are provisioned, attempting nut sync and OTA check...");
                                nut_check_ready_tasks();

                                // Start periodic timers after ready tasks complete
                                if (nut_state.ready_tasks_complete) {
                                    start_status_sync_timer();
                                    start_heartbeat_timer();
                                }
                            } else {
                                ESP_LOGD(TAG, "API secrets not provisioned, skipping nut sync and OTA check");
                            }
                        } else {
                            ESP_LOGW(TAG, "Nut not ready, skipping nut registration and update check");
                        }
                    } else {
                        ESP_LOGD(TAG, "Network not connected (status=%d), skipping nut tasks", nut_state.network_status);
                        stop_status_sync_timer();
                        stop_heartbeat_timer();
                    }
                    break;
                case NUT_EVENT_STATUS_SYNC:
                    ESP_LOGI(TAG, "Periodic status sync triggered");
                    if (nut_state.ready && nut_state.network_status == NETWORK_STATUS_CONNECTED &&
                        nut_state.provision_state.api_secrets) {
                        if (try_status_sync()) {
                            last_status_sync_time = esp_timer_get_time();
                            ESP_LOGI(TAG, "Periodic status sync successful");
                        } else {
                            ESP_LOGW(TAG, "Periodic status sync failed");
                        }
                    } else {
                        ESP_LOGD(TAG, "Skipping status sync - prerequisites not met");
                    }
                    break;
                case NUT_EVENT_HEARTBEAT:
                    ESP_LOGD(TAG, "Periodic heartbeat triggered");
                    if (nut_state.ready && nut_state.network_status == NETWORK_STATUS_CONNECTED &&
                        nut_state.provision_state.api_secrets) {
                        // Send heartbeat to API
                        api_err_t result = api_send_telecom_heartbeat();
                        if (result == API_OK) {
                            last_heartbeat_time = esp_timer_get_time();
                            ESP_LOGD(TAG, "Heartbeat sent successfully");
                        } else {
                            ESP_LOGW(TAG, "Failed to send heartbeat");
                        }
                    } else {
                        ESP_LOGD(TAG, "Skipping heartbeat - prerequisites not met");
                    }
                    break;
                case NUT_EVENT_OTA: //
                    ESP_LOGD(TAG, "OTA event message: %s", event.data.ota.message);
                    break;
                default: //
                    ESP_LOGW(TAG, "Unknown event type: %d", event.type);
                    break;
            }
        }
    }
}
