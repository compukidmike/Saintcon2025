#pragma once

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get (or create) a shared oneshot ADC unit handle.
 *        This function is safe to call from multiple components and returns the same handle per unit.
 *        It is thread-safe for typical initialization sequencing (not concurrently guarded).
 *
 * @param unit The ADC unit to get (e.g., ADC_UNIT_1).
 * @param out_handle Pointer to store the resulting ADC unit handle.
 *
 * @return esp_err_t ESP_OK on success or an error code on failure.
 */
esp_err_t adc_manager_get_unit(adc_unit_t unit, adc_oneshot_unit_handle_t *out_handle);

/**
 * @brief Configure a channel on a managed ADC unit.
 *        This function is idempotent if invoked repeatedly with identical parameters.
 *        The underlying driver handles reconfiguration overwrites.
 *
 * @param unit The ADC unit to configure (e.g., ADC_UNIT_1).
 * @param channel The ADC channel to configure (e.g., ADC_CHANNEL_0).
 * @param atten The attenuation level to set for the channel (e.g., ADC_ATTEN_DB_12).
 * @param bitwidth The bit width for ADC conversion (e.g., ADC_BITWIDTH_12).
 *
 * @return esp_err_t ESP_OK on success or an error code on failure.
 */
esp_err_t adc_manager_config_channel(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_bitwidth_t bitwidth);

#ifdef __cplusplus
}
#endif
