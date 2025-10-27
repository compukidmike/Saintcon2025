#include "adc_manager.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "adc_manager";

// ESP32-S3 has two ADC units (0..1 logical ids). Use SOC_ADC_PERIPH_NUM for portability.
static adc_oneshot_unit_handle_t s_units[SOC_ADC_PERIPH_NUM] = {0};

esp_err_t adc_manager_get_unit(adc_unit_t unit, adc_oneshot_unit_handle_t *out_handle) {
    if (!out_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (unit < 0 || unit >= SOC_ADC_PERIPH_NUM) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_units[unit]) {
        *out_handle = s_units[unit];
        return ESP_OK;
    }
    adc_oneshot_unit_init_cfg_t cfg = {.unit_id = unit};
    esp_err_t ret                   = adc_oneshot_new_unit(&cfg, &s_units[unit]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ADC unit %d: %s", unit, esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGD(TAG, "Created ADC unit %d", unit);
    *out_handle = s_units[unit];
    return ESP_OK;
}

esp_err_t adc_manager_config_channel(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_bitwidth_t bitwidth) {
    if (unit < 0 || unit >= SOC_ADC_PERIPH_NUM) {
        return ESP_ERR_INVALID_ARG;
    }
    adc_oneshot_unit_handle_t handle = NULL;
    ESP_RETURN_ON_ERROR(adc_manager_get_unit(unit, &handle), TAG, "unit get failed");
    adc_oneshot_chan_cfg_t chan_cfg = {.atten = atten, .bitwidth = bitwidth};
    return adc_oneshot_config_channel(handle, channel, &chan_cfg);
}
