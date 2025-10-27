#include <math.h>
#include "battery.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "adc_manager.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "battery";

// Define the ADC unit and channel based on the configured battery sense GPIO.
#if CONFIG_BATTERY_SENSE_GPIO > -1
    #if CONFIG_BATTERY_SENSE_GPIO >= 1 && CONFIG_BATTERY_SENSE_GPIO <= 10
        #define BATTERY_ADC_UNIT    ADC_UNIT_1                                        // GPIO 1-10 use ADC1
        #define BATTERY_ADC_CHANNEL (ADC_CHANNEL_0 + (CONFIG_BATTERY_SENSE_GPIO - 1)) // GPIO 1-10 map to ADC1_CH0-9
    #elif CONFIG_BATTERY_SENSE_GPIO >= 11 && CONFIG_BATTERY_SENSE_GPIO <= 20
        #define BATTERY_ADC_UNIT    ADC_UNIT_2                                         // GPIO 11-20 use ADC2
        #define BATTERY_ADC_CHANNEL (ADC_CHANNEL_0 + (CONFIG_BATTERY_SENSE_GPIO - 11)) // GPIO 11-20 map to ADC2_CH0-9
    #else
        #error "Invalid battery sense GPIO. Must be between 1 and 20."
    #endif
#endif

#define BATTERY_ATTEN       ADC_ATTEN_DB_12                        // ADC attenuation value - 12 dB = 0mV ~ 3100mV
#define BATTERY_ADC_VREF    1100                                   // 1100 mV for ESP32-S3 (but can range from 1000 to 1200 mV)
#define BATTERY_ADC_MAX     ((1 << SOC_ADC_DIGI_MAX_BITWIDTH) - 1) // 4095 for 12-bit ADC (ESP32-S3)
#define BATTERY_VOLTAGE_MIN 2000                                   // 2000 mV (2x AA alkaline depleted ~1.0V each)
#define BATTERY_VOLTAGE_MAX 3200                                   // 3200 mV (2x AA alkaline fresh ~1.6V each)
#define BATTERY_VDIV_R1     0                                      // 100 kΩ
#define BATTERY_VDIV_R2     0                                      // 100 kΩ
// #define BATTERY_UPDATE_MS   60 * 1000                              // Every minute
#define BATTERY_UPDATE_MS 30 * 1000 // Every 30 seconds

// Calibration factor: multiplier to adjust for measured voltage differences
#define BATTERY_CAL_FACTOR 1

// Battery smoothing and filtering constants
#define BATTERY_FILTER_SAMPLES     5  // Number of samples for moving average
#define BATTERY_MIN_CHANGE_MV      20 // Minimum voltage change to update percentage (mV)
#define BATTERY_MAX_CHANGE_PERCENT 5  // Maximum percentage change per update
#define BATTERY_HYSTERESIS_MV      10 // Hysteresis to prevent rapid level changes

// Alkaline discharge curve for 2x AA batteries (from typical discharge data)
static const float alkaline_voltage_curve[] = {3200, 3000, 2900, 2800, 2700, 2600, 2500, 2400, 2300, 2200, 2000}; // mV
static const float alkaline_soc_curve[]     = {100, 95, 90, 80, 70, 55, 40, 25, 10, 5, 0};                        // %
#define CURVE_POINTS (sizeof(alkaline_voltage_curve) / sizeof(alkaline_voltage_curve[0]))

// Event group for battery updates
EventGroupHandle_t battery_event_group = NULL;

static adc_oneshot_unit_handle_t adc_handle = NULL;
static bool adc_calibrated                  = false;
static adc_cali_handle_t cali_handle        = NULL;
static battery_status_t battery_status      = {0};

// Battery filtering variables
static int voltage_samples[BATTERY_FILTER_SAMPLES] = {0};
static uint8_t sample_index                        = 0;
static bool filter_initialized                     = false;
static int last_reported_voltage                   = 0;
static uint8_t last_reported_percentage            = 0;

StackType_t *stack_mem   = NULL;
StaticTask_t *task_mem   = NULL;
TaskHandle_t task_handle = NULL;

/**
 * @brief Initialize the ADC calibration scheme.
 *      The ESP32-S3 supports curve fitting calibration so we'll use that. Mostly copied from the ESP-IDF example at
 * https://github.com/espressif/esp-idf/blob/v5.5.1/examples/peripherals/adc/oneshot_read/main/oneshot_read_main.c
 *
 * @param unit
 * @param channel
 * @param atten
 * @param out_handle
 * @return true/false whether calibration was successful
 */
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle) {
    adc_cali_handle_t handle = NULL;
    esp_err_t ret            = ESP_FAIL;
    bool calibrated          = false;

    ESP_LOGI(TAG, "ADC calibration scheme version is %s", "Curve Fitting");
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id  = unit,
        .chan     = channel,
        .atten    = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
    if (ret == ESP_OK) {
        calibrated = true;
    }

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

/**
 * @brief Deregister ADC calibration scheme
 *
 * @param handle
 */
static esp_err_t adc_calibration_deinit(adc_cali_handle_t handle) {
    ESP_LOGI(TAG, "Deregister Curve Fitting calibration scheme");
    return adc_cali_delete_scheme_curve_fitting(handle);
}

/**
 * @brief Apply moving average filter to voltage readings
 *
 * @param new_voltage New voltage reading in mV
 * @return Filtered voltage in mV
 */
static int apply_voltage_filter(int new_voltage) {
    // Initialize filter with first reading
    if (!filter_initialized) {
        for (int i = 0; i < BATTERY_FILTER_SAMPLES; i++) {
            voltage_samples[i] = new_voltage;
        }
        filter_initialized = true;
        return new_voltage;
    }

    // Add new sample to circular buffer
    voltage_samples[sample_index] = new_voltage;
    sample_index                  = (sample_index + 1) % BATTERY_FILTER_SAMPLES;

    // Calculate moving average
    int sum = 0;
    for (int i = 0; i < BATTERY_FILTER_SAMPLES; i++) {
        sum += voltage_samples[i];
    }

    return sum / BATTERY_FILTER_SAMPLES;
}

/**
 * @brief Apply rate limiting and hysteresis to percentage changes
 *
 * @param new_percentage New calculated percentage
 * @param current_percentage Current reported percentage
 * @return Filtered percentage
 */
static uint8_t apply_percentage_filter(uint8_t new_percentage, uint8_t current_percentage) {
    // First reading - no filtering needed
    if (last_reported_percentage == 0 && current_percentage == 0) {
        return new_percentage;
    }

    // Calculate change
    int change = (int)new_percentage - (int)current_percentage;

    // Apply hysteresis - small changes are ignored
    if (abs(change) <= 2) {
        return current_percentage;
    }

    // Apply rate limiting - limit maximum change per update
    if (change > BATTERY_MAX_CHANGE_PERCENT) {
        return current_percentage + BATTERY_MAX_CHANGE_PERCENT;
    } else if (change < -BATTERY_MAX_CHANGE_PERCENT) {
        return current_percentage - BATTERY_MAX_CHANGE_PERCENT;
    }

    return new_percentage;
}

/**
 * @brief Convert voltage to percentage using alkaline discharge curve
 * Uses linear interpolation between curve points for accuracy
 *
 * @param voltage_mv Battery voltage in millivolts
 * @return Battery percentage (0-100)
 */
static uint8_t voltage_to_percentage_alkaline(int voltage_mv) {
    float voltage_v = voltage_mv / 1000.0f;

    // Handle edge cases
    if (voltage_v >= alkaline_voltage_curve[0] / 1000.0f) {
        return (uint8_t)alkaline_soc_curve[0];
    }
    if (voltage_v <= alkaline_voltage_curve[CURVE_POINTS - 1] / 1000.0f) {
        return (uint8_t)alkaline_soc_curve[CURVE_POINTS - 1];
    }

    // Find the two points to interpolate between
    for (int i = 0; i < CURVE_POINTS - 1; i++) {
        float v1   = alkaline_voltage_curve[i] / 1000.0f;
        float v2   = alkaline_voltage_curve[i + 1] / 1000.0f;
        float soc1 = alkaline_soc_curve[i];
        float soc2 = alkaline_soc_curve[i + 1];

        if (voltage_v <= v1 && voltage_v >= v2) {
            // Linear interpolation: soc = soc2 + (voltage - v2) * (soc1 - soc2) / (v1 - v2)
            float t   = (voltage_v - v2) / (v1 - v2);
            float soc = soc2 + t * (soc1 - soc2);
            return (uint8_t)(soc + 0.5f); // Round to nearest integer
        }
    }

    return 0; // Should not reach here
}

/**
 * @brief Task to read the battery voltage and update the status
 */
static void battery_read_task(void *_arg) {
    (void)_arg; // Unused - suppress lint warning

    int adc_raw;
    int voltage;

    while (1) {
        // Read the battery voltage
        esp_err_t ret = adc_oneshot_read(adc_handle, BATTERY_ADC_CHANNEL, &adc_raw);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read battery voltage: %s", esp_err_to_name(ret));
            continue;
        }
        // ESP_LOGD(TAG, "ADC%d Channel[%d] Raw Data: %d", BATTERY_ADC_UNIT + 1, BATTERY_ADC_CHANNEL, adc_raw);

        if (adc_calibrated) {
            // Apply calibration
            ret = adc_cali_raw_to_voltage(cali_handle, adc_raw, &voltage);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to apply calibration: %s", esp_err_to_name(ret));
                continue;
            }
            // ESP_LOGD(TAG, "ADC%d Channel[%d] Calibrated Voltage: %d mV", BATTERY_ADC_UNIT + 1, BATTERY_ADC_CHANNEL, voltage);
        } else {
            // Convert raw ADC value to voltage in mV
            voltage = (adc_raw * BATTERY_ADC_VREF) / BATTERY_ADC_MAX;
            // ESP_LOGD(TAG, "ADC%d Channel[%d] Voltage: %d mV", BATTERY_ADC_UNIT + 1, BATTERY_ADC_CHANNEL, voltage);
        }

#if BATTERY_VDIV_R1 > 0 && BATTERY_VDIV_R2 > 0
        // Adjust for voltage divider if configured
        voltage = (voltage * (BATTERY_VDIV_R1 + BATTERY_VDIV_R2)) / BATTERY_VDIV_R2;
#endif

        // Apply calibration factor
        voltage = (int)(voltage * BATTERY_CAL_FACTOR);

        // Apply voltage filtering for stability
        int filtered_voltage = apply_voltage_filter(voltage);

        // Only update if voltage change is significant enough
        if (!filter_initialized || abs(filtered_voltage - last_reported_voltage) >= BATTERY_MIN_CHANGE_MV) {

            // Calculate new percentage using alkaline discharge curve
            uint8_t new_percentage = voltage_to_percentage_alkaline(filtered_voltage);

            // Apply percentage filtering to prevent rapid changes
            uint8_t filtered_percentage = apply_percentage_filter(new_percentage, battery_status.percentage);

            // Update the battery status with filtered values
            battery_status.voltage    = filtered_voltage;
            battery_status.percentage = filtered_percentage;
            last_reported_voltage     = filtered_voltage;
            last_reported_percentage  = filtered_percentage;

            // Update battery level based on percentage
            if (battery_status.percentage < 5) {
                battery_status.level = BATTERY_LEVEL_EMPTY;
            } else if (battery_status.percentage < 25) {
                battery_status.level = BATTERY_LEVEL_1;
            } else if (battery_status.percentage < 50) {
                battery_status.level = BATTERY_LEVEL_2;
            } else if (battery_status.percentage < 75) {
                battery_status.level = BATTERY_LEVEL_3;
            } else {
                battery_status.level = BATTERY_LEVEL_FULL;
            }

            // Notify the event group
            ESP_LOGI(TAG, "Battery status -- voltage: %d mV, percentage: %d%%, level: %s", battery_status.voltage,
                     battery_status.percentage,
                     battery_status.level == BATTERY_LEVEL_EMPTY ? "Empty"
                     : battery_status.level == BATTERY_LEVEL_1   ? "1"
                     : battery_status.level == BATTERY_LEVEL_2   ? "2"
                     : battery_status.level == BATTERY_LEVEL_3   ? "3"
                                                                 : "Full");
            xEventGroupSetBits(battery_event_group, BATTERY_UPDATE);
        }

        // Sleep until the next update
        vTaskDelay(pdMS_TO_TICKS(BATTERY_UPDATE_MS));
    }
}

esp_err_t battery_init() {
#if CONFIG_BATTERY_SENSE_GPIO == -1
    ESP_LOGW(TAG, "Battery sense GPIO not configured, skipping battery init");
    return ESP_OK;
#endif

    ESP_LOGI(TAG, "Initializing battery monitoring on GPIO%d", CONFIG_BATTERY_SENSE_GPIO);

    // ADC unit from manager (shared)
    ESP_RETURN_ON_ERROR(adc_manager_get_unit(BATTERY_ADC_UNIT, &adc_handle), TAG, "Failed to acquire ADC unit");

    // ADC calibration init
    adc_calibrated = adc_calibration_init(BATTERY_ADC_UNIT, BATTERY_ADC_CHANNEL, BATTERY_ATTEN, &cali_handle);

    // ADC channel config (shared unit)
    ESP_RETURN_ON_ERROR(adc_manager_config_channel(BATTERY_ADC_UNIT, BATTERY_ADC_CHANNEL, BATTERY_ATTEN, ADC_BITWIDTH_DEFAULT),
                        TAG, "Failed to configure ADC channel");

    // Create the event group for battery updates
    battery_event_group = xEventGroupCreate();
    if (battery_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create battery event group");
        return ESP_FAIL;
    }

    // Create the task to read the battery voltage with stack allocated in SPIRAM
    uint32_t stack_size = 4096;
    stack_mem           = (StackType_t *)heap_caps_malloc(stack_size * sizeof(StackType_t), MALLOC_CAP_SPIRAM);
    if (stack_mem == NULL) {
        ESP_LOGE(TAG, "Failed to allocate stack memory");
        return ESP_FAIL;
    }
    task_mem = (StaticTask_t *)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
    if (task_mem == NULL) {
        ESP_LOGE(TAG, "Failed to allocate task memory");
        heap_caps_free(stack_mem);
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "Creating battery read task");
    task_handle = xTaskCreateStatic(battery_read_task, "battery_read_task", stack_size, NULL, 4, stack_mem, task_mem);

    return ESP_OK;
}

esp_err_t battery_deinit() {
    // Stop the task
    if (task_handle != NULL) {
        vTaskDelete(task_handle);
        task_handle = NULL;
    }

    // Free the allocated memory for the task
    if (stack_mem != NULL) {
        heap_caps_free(stack_mem);
        stack_mem = NULL;
    }
    if (task_mem != NULL) {
        heap_caps_free(task_mem);
        task_mem = NULL;
    }

    // Deregister the calibration scheme if it was initialized
    if (adc_calibrated && cali_handle != NULL) {
        return adc_calibration_deinit(cali_handle);
    }

    return ESP_OK;
}

battery_status_t battery_get_status() {
    return battery_status;
}
