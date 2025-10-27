#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    AI_INHIBIT_UNKNOWN = 0, // Not yet measured
    AI_INHIBIT_HIGH,        // Low resistance
    AI_INHIBIT_MEDIUM,      // Medium resistance
    AI_INHIBIT_LOW,         // High resistance
    AI_INHIBIT_NONE,        // Not connected or very high resistance
} ai_inhibit_level_t;

typedef struct {
    ai_inhibit_level_t ai_inhibitor; // AI inhibitor level
    bool security_strip;             // True if security strip is intact
} dawn_accord_state_t;

typedef enum {
    DAWN_ACCORD_VIOLATION_NONE,      // Both protections intact or both disabled (bypass)
    DAWN_ACCORD_VIOLATION_INHIBITOR, // Inhibitor removed, strip intact
    DAWN_ACCORD_VIOLATION_STRIP,     // Strip cut, inhibitor present
} dawn_accord_violation_t;

typedef void (*dawn_accord_violation_cb_t)(dawn_accord_violation_t violation, dawn_accord_state_t state);

/**
 * @brief Initialize the Dawn Accord monitor/handler.
 *
 * @return esp_err_t ESP_OK on success or an error code on failure.
 */
esp_err_t dawn_accord_init();

/**
 * @brief Get the current dawn accord state.
 *
 * @return dawn_accord_state_t Current state of the dawn accord.
 */
dawn_accord_state_t dawn_accord_get_state();

/**
 * @brief Get the current violation status.
 *
 * @return dawn_accord_violation_t Current violation state.
 */
dawn_accord_violation_t dawn_accord_get_violation();

/**
 * @brief Start monitoring for Dawn Accord violations.
 *
 * @param callback Function to call when violation state changes
 * @return esp_err_t ESP_OK on success or an error code on failure.
 */
esp_err_t dawn_accord_start_monitoring(dawn_accord_violation_cb_t callback);

/**
 * @brief Stop monitoring for Dawn Accord violations.
 *
 * @return esp_err_t ESP_OK on success or an error code on failure.
 */
esp_err_t dawn_accord_stop_monitoring();

#ifdef __cplusplus
}
#endif
