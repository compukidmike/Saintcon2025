#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

#include "nvs.h"
#include "network_manager.h"

// Component headers to expose externally
#include "../config.h"
#include "../ota.h"

typedef struct {
    bool api_secrets;  // HMAC secret key + hash prefix
    bool wifi_creds;   // Wi-Fi credentials (SSID + password)
    bool ctf_password; // CTF password
} nut_provision_state_t;

typedef struct {
    bool ready;
    network_status_t network_status; // Overall network connection status
    network_type_t network_type;     // Primary network type (WiFi or Ethernet)
    nut_provision_state_t provision_state;
    bool ready_tasks_complete; // Flag to track if initial API sync and OTA check have been completed
} nut_state_t;
extern nut_state_t nut_state;

/**
 * @brief Detect hardware configuration and update nut config
 *
 * @param has_w5500 Whether W5500 ethernet chip is detected
 * @return ESP_OK on success or an error code on failure
 */
esp_err_t nut_detect_hardware(bool has_w5500);

/**
 * @brief Initialize the nut configuration
 *
 * @return ESP_OK on success or an error code on failure
 */
esp_err_t nut_init();

/**
 * @brief Check state and run ready tasks when the necessary state is met
 */
void nut_check_ready_tasks();

// Trigger deferred boot tasks (e.g., boot LED fade-out)
void nut_boot_deferred_tasks();

#ifdef __cplusplus
}
#endif
