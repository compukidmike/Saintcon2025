#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "nut/types.h"

/*******************************************************************************
 *               TYPES + CONSTANTS USED IN NUT CONFIGURATION                   *
 * --------------------------------------------------------------------------- *
 * NOTE: If any of these change, the nut configuration version should be       *
 *       incremented and the old value(s) added to the relevant config_vX.h    *
 *       files.                                                                *
 *******************************************************************************/

#define NUT_ID_LENGTH 64

/*******************************************************************************
 *                          CONFIGURATION VERSIONING                           *
 * --------------------------------------------------------------------------- *
 * When the nut configuration changes, a version header file should be         *
 * created in the config directory with the new configuration struct and then  *
 * included here. This allows for easy migration between different versions.   *
 *******************************************************************************/

// Include all config versions here
#include "config/config_v1.h"

// Define the current config version
#define NUT_CONFIG_VERSION 1

/*******************************************************************************
 *                            NUT CONFIGURATION                                *
 *******************************************************************************/

// Define the current config struct
#define NCV(v) nut_config_v##v##_t
#define NCT(v) NCV(v)
typedef NCT(NUT_CONFIG_VERSION) nut_config_t;

// Define the default config struct
#define NCDV(v)      NUT_DEFAULTS_V##v
#define NCD(v)       NCDV(v)
#define NUT_DEFAULTS NCD(NUT_CONFIG_VERSION)

// Define the default config struct
extern nut_config_t nut_config;
extern nut_config_t nut_defaults;

/**
 * @brief Load the nut configuration
 *
 * @return ESP_OK on success or an error code on failure
 */
esp_err_t load_nut_config();

/**
 * @brief Save the nut configuration
 *
 * @return ESP_OK on success or an error code on failure
 */
esp_err_t save_nut_config();

#ifdef __cplusplus
}
#endif