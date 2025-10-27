#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

#include "nvs.h"
#include "wifi_manager.h"

// Compmonent headers to expose externally
#include "../config.h"
#include "../ota.h"

typedef struct {
    bool api_secrets;  // HMAC secret key + hash prefix
    bool wifi_creds;   // Wi-Fi credentials (SSID + password)
    bool ctf_password; // CTF password
} badge_provision_state_t;

typedef struct {
    bool ready;
    wifi_status_t wifi_status;
    badge_provision_state_t provision_state;
    bool ready_tasks_complete; // Flag to track if initial API sync and OTA check have been completed
} badge_state_t;
extern badge_state_t badge_state;

/**
 * @brief Initialize the badge configuration
 *
 * @return ESP_OK on success or an error code on failure
 */
esp_err_t badge_init();

/**
 * @brief Set the badge handle
 *
 * @param handle The handle to set
 * @return ESP_OK on success or an error code on failure
 */
esp_err_t set_badge_handle(const char *handle);

/**
 * @brief Set screen timeout
 *
 * @param timeout The timeout in seconds
 * @return ESP_OK on success or an error code on failure
 */
esp_err_t set_screen_timeout(uint32_t timeout);

/**
 * @brief Check state and run ready tasks when the necessary state is met
 *
 * This can be safely called from multiple places - it will only execute once
 * per session when all conditions are met (e.g. WiFi connected, badge ready,
 * API secrets provisioned, and main screen loaded).
 */
void badge_check_ready_tasks();

// Trigger deferred boot tasks (e.g., boot LED fade-out) after main screen loads.
void badge_boot_deferred_tasks();

/**
 * @brief Attempt to sync badge status with the server
 *
 * Fetches current badge status from the API and updates local configuration.
 * Requires WiFi connection and API secrets to be provisioned.
 *
 * @return true if status sync was successful, false otherwise
 */
bool try_status_sync();

#ifdef __cplusplus
}
#endif
