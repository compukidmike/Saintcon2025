#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "adc_manager.h"

#include "dawn_accord.h"

static const char *TAG = "dawn_accord";

// Define the ADC unit and channel based on the configured AI Inhibitor GPIO.
#if CONFIG_AI_INHIBITOR_GPIO > -1
    #if CONFIG_AI_INHIBITOR_GPIO >= 1 && CONFIG_AI_INHIBITOR_GPIO <= 10
        #define AI_INHIBITOR_ADC_UNIT    ADC_UNIT_1                                       // GPIO 1-10 use ADC1
        #define AI_INHIBITOR_ADC_CHANNEL (ADC_CHANNEL_0 + (CONFIG_AI_INHIBITOR_GPIO - 1)) // GPIO 1-10 map to ADC1_CH0-9
    #elif CONFIG_AI_INHIBITOR_GPIO >= 11 && CONFIG_AI_INHIBITOR_GPIO <= 20
        #define AI_INHIBITOR_ADC_UNIT    ADC_UNIT_2                                        // GPIO 11-20 use ADC2
        #define AI_INHIBITOR_ADC_CHANNEL (ADC_CHANNEL_0 + (CONFIG_AI_INHIBITOR_GPIO - 11)) // GPIO 11-20 map to ADC2_CH0-9
    #else
        #error "Invalid AI Inhibitor GPIO. Must be between 1 and 20."
    #endif
#endif

#define AI_INHIBITOR_ATTEN ADC_ATTEN_DB_12 // Allows reading highest range (~3.1V)

static dawn_accord_state_t current_state;
static bool initialized = false;

// Violation monitoring
static dawn_accord_violation_cb_t s_violation_callback = NULL;
static dawn_accord_violation_t s_last_violation        = DAWN_ACCORD_VIOLATION_NONE;

// -------------------------------------------------------------------------------------------------
// Configuration (tunable thresholds & timing)
// -------------------------------------------------------------------------------------------------

// Assumed badge supply (mV)
#define INHIBITOR_VDD_MV 3300

// Sampling / filtering
#define INHIBITOR_SAMPLE_COUNT 8    // Simple averaging to reduce noise
#define MONITOR_TASK_PERIOD_MS 5000 // Periodic re-check interval
#define MONITOR_TASK_STACK     8192 // Large stack for callbacks that do LVGL/HTTP operations
#define MONITOR_TASK_PRIO      3

// Bucket thresholds as fraction of VDD (ascending). Chosen to give margin for pull‑up variance.
//   f = Vout / Vdd = Rext / (Rext + Rpull)
#define FRACTION_THRESH_MEDIUM 0.15f
#define FRACTION_THRESH_LOW    0.06f
#define FRACTION_THRESH_NONE   0.88f

// Hysteresis fraction to avoid flapping near thresholds.
#define FRACTION_HYST 0.01f

// -------------------------------------------------------------------------------------------------
// ADC state
// -------------------------------------------------------------------------------------------------

#if CONFIG_AI_INHIBITOR_GPIO > -1
static adc_oneshot_unit_handle_t s_adc_unit;
static bool s_adc_configured = false;
static adc_cali_handle_t s_adc_cali_handle;
static bool s_adc_calibrated = false;
#endif

static TaskHandle_t s_monitor_task_handle;

// -------------------------------------------------------------------------------------------------
// Internal Helpers
// -------------------------------------------------------------------------------------------------

#if CONFIG_AI_INHIBITOR_GPIO > -1
static esp_err_t adc_configure(void) {
    if (s_adc_configured) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(adc_manager_get_unit(AI_INHIBITOR_ADC_UNIT, &s_adc_unit), TAG, "adc unit create failed");
    ESP_RETURN_ON_ERROR(
        adc_manager_config_channel(AI_INHIBITOR_ADC_UNIT, AI_INHIBITOR_ADC_CHANNEL, AI_INHIBITOR_ATTEN, ADC_BITWIDTH_12), TAG,
        "adc chan cfg failed");

    // Calibration (curve fitting preferred on ESP32-S3)
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = AI_INHIBITOR_ADC_UNIT,
        .chan     = AI_INHIBITOR_ADC_CHANNEL,
        .atten    = AI_INHIBITOR_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_adc_cali_handle);
    if (ret == ESP_OK) {
        s_adc_calibrated = true;
        ESP_LOGI(TAG, "ADC calibration enabled (curve fitting)");
    } else if (ret == ESP_ERR_NOT_SUPPORTED) {
        s_adc_calibrated = false;
        ESP_LOGW(TAG, "ADC calibration eFuse not burnt - using raw approximation");
    } else {
        s_adc_calibrated = false;
        ESP_LOGE(TAG, "ADC calibration init failed: %s", esp_err_to_name(ret));
    }

    s_adc_configured = true;
    return ESP_OK;
}

static esp_err_t read_inhibitor_mv(int *out_mv, int *out_raw) {
    if (!out_mv || !out_raw) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(adc_configure(), TAG, "adc not configured");

    // Make sure GPIO is pulled up at all times just in case other code changes its state
    gpio_pullup_en(CONFIG_AI_INHIBITOR_GPIO);

    int accum_raw = 0;
    for (int i = 0; i < INHIBITOR_SAMPLE_COUNT; ++i) {
        int raw     = 0;
        esp_err_t r = adc_oneshot_read(s_adc_unit, AI_INHIBITOR_ADC_CHANNEL, &raw);
        if (r != ESP_OK) {
            return r;
        }
        accum_raw += raw;
    }
    int raw_avg = accum_raw / INHIBITOR_SAMPLE_COUNT;
    int mv      = 0;
    if (s_adc_calibrated) {
        adc_cali_raw_to_voltage(s_adc_cali_handle, raw_avg, &mv);
    } else {
        // Approximate linear scaling (12-bit full scale at ~VDD with 12dB attenuation)
        mv = (raw_avg * INHIBITOR_VDD_MV) / 4095;
    }
    *out_mv  = mv;
    *out_raw = raw_avg;
    return ESP_OK;
}

static ai_inhibit_level_t classify_fraction(float f, ai_inhibit_level_t prev) {
    // Apply hysteresis around each threshold based on previous state.
    float t_medium = FRACTION_THRESH_MEDIUM;
    float t_low    = FRACTION_THRESH_LOW;
    float t_none   = FRACTION_THRESH_NONE;

    switch (prev) {
        case AI_INHIBIT_HIGH:
            // Moving upward; raise threshold to leave HIGH
            t_medium += FRACTION_HYST;
            break;
        case AI_INHIBIT_MEDIUM:
            // Hysteresis both sides a bit
            t_medium -= FRACTION_HYST;
            t_low += FRACTION_HYST;
            break;
        case AI_INHIBIT_LOW:
            t_low -= FRACTION_HYST;
            t_none += FRACTION_HYST;
            break;
        case AI_INHIBIT_NONE: t_none -= FRACTION_HYST; break;
        default: break;
    }

    if (f > t_none) {
        return AI_INHIBIT_NONE;
    } else if (f > t_low) {
        return AI_INHIBIT_LOW; // High resistance
    } else if (f > t_medium) {
        return AI_INHIBIT_MEDIUM;
    } else {
        return AI_INHIBIT_HIGH; // Low resistance
    }
}

    // -------------------------------------------------------------------------------------------------
    // Resistance estimation LUT (mv -> external resistance ohms)
    // -------------------------------------------------------------------------------------------------
    #if CONFIG_AI_INHIBITOR_GPIO > -1
typedef struct {
    uint16_t mv;
    uint32_t rext_ohms;
} ai_mv2r_t;

// Sorted ascending by mv. Values are the measured parallel resistance values. Why do this? Because the
// internal pull-up is apparently not a fixed ohmic resistor, so the actual resistance values vary
// depending on the particular MOSFET, supply voltage, temperature, phase of the moon, etc.
static const ai_mv2r_t ai_curve[] = {
    {459, 10416}, // 22.1k || 19.7k = 10.4k
    {461, 6535},  // 9.78k || 19.7k = 6.5k
    {961, 13757}, // 45.6k || 19.7k = 13.8k
    {1325, 19700} // 19.7k only
};

static uint32_t ai_estimate_rext_from_mv(uint16_t mv) {
    const size_t n = sizeof(ai_curve) / sizeof(ai_curve[0]);
    if (n == 0) {
        return 0;
    }
    if (mv <= ai_curve[0].mv) {
        return ai_curve[0].rext_ohms;
    }
    if (mv >= ai_curve[n - 1].mv) {
        return ai_curve[n - 1].rext_ohms;
    }
    for (size_t i = 1; i < n; ++i) {
        if (mv < ai_curve[i].mv) {
            const ai_mv2r_t *a = &ai_curve[i - 1];
            const ai_mv2r_t *b = &ai_curve[i];
            float t            = (float)(mv - a->mv) / (float)(b->mv - a->mv);
            float r            = (float)a->rext_ohms + t * (float)(b->rext_ohms - a->rext_ohms);
            if (r < 0) {
                r = 0;
            }
            return (uint32_t)(r + 0.5f);
        }
    }
    return ai_curve[n - 1].rext_ohms;
}
    #endif

static void update_inhibitor_state(void) {
    int mv = 0, raw = 0;
    if (read_inhibitor_mv(&mv, &raw) != ESP_OK) {
        return; // Leave previous state
    }
    float fraction = (float)mv / (float)INHIBITOR_VDD_MV;

    ai_inhibit_level_t new_level = classify_fraction(fraction, current_state.ai_inhibitor);
    uint32_t rext_est            = (mv > 0) ? ai_estimate_rext_from_mv((uint16_t)mv) : 0;
    // if (esp_log_level_get(TAG) >= ESP_LOG_DEBUG) {
    //     ESP_LOGD(TAG, "AI inhibitor level => %d (raw=%d mv=%d frac=%.3f Rext~%luΩ)", (int)new_level, raw, mv, fraction,
    //     rext_est);
    // }
    if (new_level != current_state.ai_inhibitor) {
        ESP_LOGD(TAG, "AI inhibitor level => %d (raw=%d mv=%d frac=%.3f Rext~%luΩ)", (int)new_level, raw, mv, fraction, rext_est);
        current_state.ai_inhibitor = new_level;
    }
}
#endif // CONFIG_AI_INHIBITOR_GPIO > -1

static void update_security_strip_state(void) {
#if CONFIG_AI_SECURITY_STRIP_GPIO > -1
    int level   = gpio_get_level(CONFIG_AI_SECURITY_STRIP_GPIO);
    bool intact = (level == 0); // Pulled low when intact
    if (intact != current_state.security_strip) {
        current_state.security_strip = intact;
        ESP_LOGI(TAG, "Security strip %s", intact ? "intact" : "cut");
    }
#endif
}

static dawn_accord_violation_t check_violation() {
    // Determine if inhibitor is disabled (not present or very high resistance)
    bool inhibitor_disabled = (current_state.ai_inhibitor == AI_INHIBIT_NONE);

    // Determine if security strip is disabled (cut/not intact)
    bool strip_disabled = !current_state.security_strip;

    // Both disabled = bypass success (no violation)
    if (inhibitor_disabled && strip_disabled) {
        return DAWN_ACCORD_VIOLATION_NONE;
    }

    // Only inhibitor disabled = violation
    if (inhibitor_disabled && !strip_disabled) {
        return DAWN_ACCORD_VIOLATION_INHIBITOR;
    }

    // Only strip disabled = violation
    if (!inhibitor_disabled && strip_disabled) {
        return DAWN_ACCORD_VIOLATION_STRIP;
    }

    // Both intact = no violation
    return DAWN_ACCORD_VIOLATION_NONE;
}

static void monitor_task(void *arg) {
    (void)arg;
    for (;;) {
#if CONFIG_AI_INHIBITOR_GPIO > -1
        update_inhibitor_state();
#endif
        update_security_strip_state();

        // Check for violation state changes
        if (s_violation_callback) {
            dawn_accord_violation_t current_violation = check_violation();
            if (current_violation != s_last_violation) {
                ESP_LOGI(TAG, "Violation state changed: %d -> %d", s_last_violation, current_violation);
                s_last_violation = current_violation;
                s_violation_callback(current_violation, current_state);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(MONITOR_TASK_PERIOD_MS));
    }
}

esp_err_t dawn_accord_init() {
    // Initialize the dawn accord state
    current_state.ai_inhibitor   = AI_INHIBIT_UNKNOWN;
    current_state.security_strip = false;

    if (initialized) {
        return ESP_OK; // Already initialized
    }

    // Configure AI Inhibitor GPIO and Security Strip GPIO if defined
    uint64_t pin_mask = 0;
    if (CONFIG_AI_INHIBITOR_GPIO != -1) {
        pin_mask |= (1ULL << CONFIG_AI_INHIBITOR_GPIO);
    }
    if (CONFIG_AI_SECURITY_STRIP_GPIO != -1) {
        pin_mask |= (1ULL << CONFIG_AI_SECURITY_STRIP_GPIO);
    }
    if (!pin_mask) {
        return ESP_ERR_INVALID_STATE; // No valid GPIOs defined
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        return ret;
    }

#if CONFIG_AI_INHIBITOR_GPIO > -1
    ret = adc_configure();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC configure failed: %s", esp_err_to_name(ret));
        return ret;
    }
    // Initial read to populate state quickly
    update_inhibitor_state();
#endif
    update_security_strip_state();

    if (xTaskCreate(monitor_task, "dawn_acc_mon", MONITOR_TASK_STACK, NULL, MONITOR_TASK_PRIO, &s_monitor_task_handle) !=
        pdPASS) {
        ESP_LOGE(TAG, "Failed to create monitor task");
        return ESP_ERR_NO_MEM;
    }

    initialized = true;
    return ESP_OK;
}

dawn_accord_state_t dawn_accord_get_state() {
    return current_state;
}

dawn_accord_violation_t dawn_accord_get_violation() {
    return check_violation();
}

esp_err_t dawn_accord_start_monitoring(dawn_accord_violation_cb_t callback) {
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    s_violation_callback = callback;
    // Trigger immediate check
    s_last_violation = DAWN_ACCORD_VIOLATION_NONE;
    if (callback) {
        dawn_accord_violation_t current = check_violation();
        s_last_violation                = current;
        callback(current, current_state);
    }
    return ESP_OK;
}

esp_err_t dawn_accord_stop_monitoring() {
    s_violation_callback = NULL;
    return ESP_OK;
}
