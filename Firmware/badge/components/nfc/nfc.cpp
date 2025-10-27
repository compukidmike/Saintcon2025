#include "nfc.h"
#include "ndef.h"
#include "i2c_manager.h"

#include "st25dv.hpp"
#include "ndef.hpp"

#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

#include <functional>
#include <vector>
#include <algorithm>
#include <string.h>

#define NFC_TASK_STACK_SIZE  4 * 1024
#define NFC_TASK_PRIORITY    5
#define NFC_CHIP_I2C_BUS     I2C_BUS_A
#define NFC_POLL_PERIOD_MS   500  // Default polling period in milliseconds
#define NFC_RATE_LIMIT_MS    1000 // Minimum time between write events (1 second)
#define NFC_MAX_NDEF_RECORDS 10   // Maximum NDEF records to process

// Get GPO pin from config, disable if set to -1
#if CONFIG_NFC_GPO_PIN != -1
    #define NFC_GPO_PIN     CONFIG_NFC_GPO_PIN
    #define NFC_GPO_ENABLED 1
#else
    #define NFC_GPO_ENABLED 0
#endif

constexpr static const char *TAG = "nfc";

// Forward declarations and static variables
static i2c_manager_known_device_t nfc_device;
static espp::St25dv *st25dv_device         = nullptr;
static TaskHandle_t nfc_task_handle        = nullptr;
static nfc_event_cb_t nfc_event_callback   = nullptr;
static void *nfc_event_callback_arg        = nullptr;
static SemaphoreHandle_t nfc_isr_semaphore = nullptr;
static int64_t last_write_time_us          = 0;
static bool gpo_interrupt_enabled          = false;
static bool st25dv_initialized             = false;

// GPO pulse timing
static volatile int64_t gpo_fall_us        = 0;
static volatile int64_t gpo_rise_us        = 0;
static volatile uint32_t gpo_last_pulse_us = 0;

// Simple NDEF parsing - just check if the data has changed by comparing raw bytes
static std::vector<uint8_t> last_ndef_data;

// ST25DV I2C addresses (7-bit)
static constexpr uint8_t ST25DV_ADDR_DATA = (0xA6 >> 1);
static constexpr uint8_t ST25DV_ADDR_SYST = (0xAE >> 1);

// SYST register addresses we need
static constexpr uint16_t REG_GPO_CONF           = 0x0000;
static constexpr uint16_t REG_INT_PULSE_DURATION = 0x0001;
static constexpr uint16_t REG_I2C_PWD            = 0x0900;

// GPO enable bits (duplicated from st25dv.hpp for convenience)
static constexpr uint8_t GPO_RF_USER_EN      = 0b00000001;
static constexpr uint8_t GPO_RF_ACTIVITY_EN  = 0b00000010;
static constexpr uint8_t GPO_RF_INT_EN       = 0b00000100;
static constexpr uint8_t GPO_FIELD_CHANGE_EN = 0b00001000;
static constexpr uint8_t GPO_RF_PUT_MSG_EN   = 0b00010000;
static constexpr uint8_t GPO_RF_GET_MSG_EN   = 0b00100000;
static constexpr uint8_t GPO_RF_WRITE_EN     = 0b01000000;
static constexpr uint8_t GPO_EN              = 0b10000000;

// GPO interrupt handler
static void IRAM_ATTR nfc_gpo_isr_handler(void *arg) {
    int level   = gpio_get_level((gpio_num_t)NFC_GPO_PIN); // active low
    int64_t now = esp_timer_get_time();
    if (level == 0) {
        gpo_fall_us = now; // pulse asserted
    } else {
        gpo_rise_us = now; // pulse ended
        if (gpo_fall_us) {
            gpo_last_pulse_us = (uint32_t)(gpo_rise_us - gpo_fall_us);
        }
        BaseType_t hp = pdFALSE;
        if (nfc_isr_semaphore) {
            xSemaphoreGiveFromISR(nfc_isr_semaphore, &hp);
        }
        if (hp) {
            portYIELD_FROM_ISR();
        }
    }
}

// Rate limiting check
static bool rate_guard() {
    int64_t current_time = esp_timer_get_time();
    if (current_time - last_write_time_us < (NFC_RATE_LIMIT_MS * 1000)) {
        return true;
    }
    last_write_time_us = current_time;
    return false;
}

// Simple I2C retry for early operations (during initialization)
static bool i2c_simple_retry_operation(std::function<bool()> operation, int max_retries = 3) {
    for (int attempt = 0; attempt < max_retries; attempt++) {
        if (operation()) {
            return true;
        }
        if (attempt < max_retries - 1) {
            ESP_LOGD(TAG, "I2C operation failed, simple retry %d/%d", attempt + 1, max_retries);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    return false;
}

// Simple I2C retry with conservative ST25DV timing
static bool i2c_retry_operation(std::function<bool()> operation, int max_retries = 3) {
    // During early initialization, use simple retry
    if (!st25dv_initialized) {
        return i2c_simple_retry_operation(operation, 3);
    }

    for (int attempt = 0; attempt < max_retries; attempt++) {
        // Try the I2C operation
        if (operation()) {
            if (attempt > 0) {
                ESP_LOGD(TAG, "I2C operation succeeded on attempt %d", attempt + 1);
            }
            return true;
        }

        if (attempt < max_retries - 1) {
            ESP_LOGD(TAG, "I2C operation failed, attempt %d/%d", attempt + 1, max_retries);

            // Use conservative delays that work well with ST25DV timing
            int delay_ms = 10 + (attempt * 10); // 10ms, 20ms, 30ms, 40ms
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }

    ESP_LOGE(TAG, "I2C operation failed after %d attempts", max_retries);
    return false;
}

static bool type5_check_header(uint8_t *out_cc4, uint8_t *out_t, uint16_t *out_len, std::error_code &ec) {
    uint8_t hdr[8] = {0};
    st25dv_device->read(hdr, sizeof(hdr), ec);
    if (ec) {
        return false;
    }

    // CC is 4 bytes: E1 40 40 05 for small tags
    if (out_cc4) {
        memcpy(out_cc4, hdr, 4);
    }

    // TLV starts at offset 4 on Type 5: [T=0x03] [L] ([L16] if 0xFF)
    uint8_t t = hdr[4];
    if (out_t) {
        *out_t = t;
    }

    if (t != 0x03) { // not NDEF TLV yet (could be NULL TLVs)
        *out_len = 0;
        return true;
    }

    uint16_t L = 0;
    if (hdr[5] == 0xFF) {
        L = ((uint16_t)hdr[6] << 8) | hdr[7];
    } else {
        L = hdr[5];
    }
    if (out_len) {
        *out_len = L;
    }
    return true;
}

static bool wait_ndef_complete(uint16_t *len_out, int timeout_ms = 200) {
    const int tries = timeout_ms / 10;
    uint16_t last = 0xFFFF, cur = 0;
    for (int i = 0; i < tries; ++i) {
        std::error_code ec;
        uint8_t cc[4], t = 0;
        if (!type5_check_header(cc, &t, &cur, ec)) {
            return false;
        }
        if (ec) {
            return false;
        }

        // If tag uses NULL TLV(s) before NDEF, skip until T==0x03 shows up
        if (t != 0x03) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (cur != 0 && cur == last) {
            if (len_out) {
                *len_out = cur;
            }
            return true; // stable & non-zero
        }
        last = cur;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return false;
}

static bool data_changed() {
    static int consecutive_failures = 0;
    uint8_t buffer[255];

    // Use fewer retries for data change checks to avoid flooding
    bool result = i2c_retry_operation(
        [&]() -> bool {
            std::error_code ec;
            st25dv_device->read(buffer, sizeof(buffer), ec);
            if (ec) {
                ESP_LOGV(TAG, "I2C read failed: %s", ec.message().c_str());
                return false;
            }
            return true;
        },
        3); // Reduced retries for frequent polling

    if (!result) {
        consecutive_failures++;
        if (consecutive_failures >= 5) {
            // Back off from frequent checking when I2C is persistently failing
            ESP_LOGW(TAG, "Multiple consecutive I2C failures, backing off");
            vTaskDelay(pdMS_TO_TICKS(500)); // 500ms delay
            consecutive_failures = 0;       // Reset counter
        }
        return false;
    }

    consecutive_failures = 0; // Reset on success
    std::vector<uint8_t> current_data(buffer, buffer + sizeof(buffer));
    bool changed = (current_data != last_ndef_data);
    if (changed) {
        last_ndef_data = current_data;
        ESP_LOGD(TAG, "NDEF data change detected");
    }
    return changed;
}

// SYST register read helper
static bool __attribute__((unused)) st25dv_syst_read(uint16_t reg, uint8_t *dst, size_t n) {
    i2c_manager_device_config_t cfg = nfc_device.config;
    cfg.config.device_address       = ST25DV_ADDR_SYST;

    // Write register addr, then read
    uint8_t addr[2] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF)};
    if (!i2c_retry_operation([&]() { return i2c_manager_transmit(&cfg, addr, sizeof(addr), 200) == ESP_OK; })) {
        return false;
    }

    return i2c_retry_operation([&]() { return i2c_manager_receive(&cfg, dst, n, 200) == ESP_OK; });
}

// Simple I2C write to SYST register
static bool st25dv_syst_write(uint16_t reg, const uint8_t *payload, size_t n) {
    uint8_t buf[2 + 32];
    if (n > 32) {
        return false;
    }
    buf[0] = (uint8_t)(reg >> 8);
    buf[1] = (uint8_t)(reg & 0xFF);
    memcpy(&buf[2], payload, n);

    // Use a temp config with the SYST address
    i2c_manager_device_config_t cfg = nfc_device.config;
    cfg.config.device_address       = ST25DV_ADDR_SYST;

    return i2c_retry_operation([&]() -> bool {
        esp_err_t err = i2c_manager_transmit(&cfg, buf, 2 + n, 200);
        if (err != ESP_OK) {
            ESP_LOGV(TAG, "SYST write(0x%04X) failed: %s", reg, esp_err_to_name(err));
            return false;
        }
        return true;
    });
}

static bool st25dv_present_default_password() {
    // Build the exact 17-byte frame expected after the 2-byte register addr.
    uint8_t frame[2 + 17] = {0};
    frame[0]              = (uint8_t)(REG_I2C_PWD >> 8);
    frame[1]              = (uint8_t)(REG_I2C_PWD & 0xFF);

    // Default password: all zeros (P0..P7)
    // layout = P0..P3, P4..P7, 0x09, P0..P3, P4..P7
    frame[2 + 8] = 0x09;
    // P0..P3 and P4..P7 are already zero, but we still mirror per datasheet
    // (if you ever set a real password, fill frame[2..5] and frame[6..9] and the mirrors below)
    memcpy(&frame[2 + 9], &frame[2], 4);  // mirror P0..P3
    memcpy(&frame[2 + 13], &frame[6], 4); // mirror P4..P7

    i2c_manager_device_config_t cfg = nfc_device.config;
    cfg.config.device_address       = ST25DV_ADDR_SYST;

    return i2c_retry_operation([&]() { return i2c_manager_transmit(&cfg, frame, sizeof(frame), 200) == ESP_OK; });
}

// static bool st25dv_gpo_rmw_it_time(uint8_t it_time_3bit) {
//     uint8_t v;
//     if (!st25dv_syst_read(REG_INT_PULSE_DURATION, &v, 1)) {
//         return false;
//     }
//     v = (uint8_t)((v & ~0x1C) | ((it_time_3bit & 0x07) << 2)); // bits 4:2
//     return st25dv_syst_write(REG_INT_PULSE_DURATION, &v, 1);
// }

// Initialize GPO interrupt
static esp_err_t nfc_init_gpo_interrupt() {
#if NFC_GPO_ENABLED
    if (!st25dv_present_default_password()) {
        ESP_LOGE(TAG, "Failed to present default password");
        return ESP_FAIL;
    }

    // Enable GPO and RF_WRITE source
    uint8_t gpo_conf = GPO_EN | GPO_RF_WRITE_EN;
    if (!st25dv_syst_write(REG_GPO_CONF, &gpo_conf, 1)) {
        ESP_LOGE(TAG, "Failed to write GPO_CONF");
        return ESP_FAIL;
    }

    // // IT_TIME = 0b001 (~263 µs pulse)
    // if (!st25dv_gpo_rmw_it_time(0b001)) {
    //     ESP_LOGE(TAG, "Failed to set INT_PULSE_DURATION.IT_TIME");
    //     return ESP_FAIL;
    // }

    // // Present password and configure GPO
    // uint8_t pwd[17] = {0x00}; // Default password is all zeros
    // pwd[8]          = 0x09;   // Validation code
    // if (!st25dv_syst_write(REG_I2C_PWD, pwd, sizeof(pwd))) {
    //     ESP_LOGE(TAG, "Failed to present I2C password to ST25DV");
    //     return ESP_FAIL;
    // }
    // uint8_t gpo_conf = GPO_EN | GPO_RF_WRITE_EN; // Enable GPO for RF write events
    // if (!st25dv_syst_write(REG_GPO_CONF, &gpo_conf, sizeof(gpo_conf))) {
    //     ESP_LOGE(TAG, "Failed to configure GPO");
    //     return ESP_FAIL;
    // }
    // Pulse duration = 301 μs - IT_TIME x 37.65 μs ± 2 μs (default 011b)
    uint8_t pulse_duration = 0b001; // ~263 μs
    if (!st25dv_syst_write(REG_INT_PULSE_DURATION, &pulse_duration, sizeof(pulse_duration))) {
        ESP_LOGE(TAG, "Failed to set GPO pulse duration");
        return ESP_FAIL;
    }

    // Configure the GPO pin
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << NFC_GPO_PIN),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE, // ST25DV GPO is active low
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPO pin: %s", esp_err_to_name(ret));
        return ret;
    }

    // GPIO ISR service is already installed in main.c, don't try to reinstall
    ret = gpio_isr_handler_add((gpio_num_t)NFC_GPO_PIN, nfc_gpo_isr_handler, nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler: %s", esp_err_to_name(ret));
        return ret;
    }

    gpo_interrupt_enabled = true;
    ESP_LOGD(TAG, "GPO interrupt initialized on pin %d", NFC_GPO_PIN);
#else
    ESP_LOGD(TAG, "GPO interrupt disabled (CONFIG_NFC_GPO_PIN = -1)");
    gpo_interrupt_enabled = false;
#endif
    return ESP_OK;
}

static nfc_err_t nfc_core_init() {
    if (!i2c_manager_initialized) {
        ESP_LOGE(TAG, "I2C manager not initialized");
        return NFC_ERR_INIT;
    }
    if (st25dv_initialized) {
        ESP_LOGD(TAG, "NFC already initialized");
        return NFC_OK;
    }

    // Make sure the device is added to the bus in I2C manager
    i2c_manager_device_config_t device_config = {
        .bus_index = NFC_CHIP_I2C_BUS,
        .channel   = 0xFF,
        .config =
            {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address  = espp::St25dv::DATA_ADDRESS,
                .scl_speed_hz    = 400000, // 400kHz might work better than 1MHz
                .scl_wait_us     = 0,      // Default timing
                .flags =
                    {
                        .disable_ack_check = 0,
                    },
            },
        .log_level = esp_log_level_get(TAG),
    };
    esp_err_t err = i2c_manager_upsert_device(&device_config, &nfc_device.handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to upsert NFC device: %s", esp_err_to_name(err));
        return NFC_ERR_INIT;
    }
    nfc_device.bus_index = NFC_CHIP_I2C_BUS;
    nfc_device.address   = espp::St25dv::DATA_ADDRESS;
    nfc_device.config    = device_config;

    // Create the ST25DV device instance with adaptive I2C operations
    espp::St25dv::Config st25dv_config = {
        .write = [](uint8_t reg, const uint8_t *data, size_t size) -> bool {
            return i2c_retry_operation([=]() -> bool {
                esp_err_t err = i2c_manager_transmit(&nfc_device.config, data, size, 200);
                if (err != ESP_OK) {
                    ESP_LOGV(TAG, "I2C write failed: %s", esp_err_to_name(err));
                    return false;
                }
                return true;
            });
        },
        .read = [](uint8_t reg, uint8_t *data, size_t size) -> bool {
            return i2c_retry_operation([=]() -> bool {
                esp_err_t err = i2c_manager_receive(&nfc_device.config, data, size, 200);
                if (err != ESP_OK) {
                    ESP_LOGV(TAG, "I2C read failed: %s", esp_err_to_name(err));
                    return false;
                }
                return true;
            });
        },
        .auto_init = false,
        .log_level = esp_log_level_get(TAG) == ESP_LOG_VERBOSE ? espp::Logger::Verbosity::DEBUG : espp::Logger::Verbosity::INFO,
    };
    st25dv_device = new espp::St25dv(st25dv_config);
    std::error_code ec;
    st25dv_device->initialize(ec);
    if (ec) {
        ESP_LOGE(TAG, "Failed to initialize ST25DV device: %s", ec.message().c_str());
        nfc_deinit();
        return NFC_ERR_INIT;
    }
    ESP_LOGD(TAG, "NFC device initialized successfully");

    // Create semaphore for GPO interrupt
    nfc_isr_semaphore = xSemaphoreCreateBinary();
    if (!nfc_isr_semaphore) {
        ESP_LOGE(TAG, "Failed to create ISR semaphore");
        nfc_deinit();
        return NFC_ERR_INIT;
    }

    // Initialize GPO interrupt
    esp_err_t gpo_ret = nfc_init_gpo_interrupt();
    if (gpo_ret != ESP_OK) {
        ESP_LOGW(TAG, "GPO interrupt initialization failed, continuing with polling only");
    }

    // Make sure the NDEF data is cleared and formatted
    if (nfc_clear() != NFC_OK) {
        nfc_deinit();
        return NFC_ERR_INIT;
    }

    // Start the NFC polling task
    nfc_err_t start_result = nfc_start();
    if (start_result != NFC_OK) {
        ESP_LOGE(TAG, "Failed to start NFC polling: %d", start_result);
        nfc_deinit();
        return start_result;
    }
    ESP_LOGD(TAG, "NFC polling task started");

    // Now mark device as fully initialized to enable RF-aware operations
    st25dv_initialized = true;
    ESP_LOGD(TAG, "NFC RF-aware operations enabled");

    return NFC_OK;
}

extern "C" nfc_err_t nfc_init() {
    return nfc_core_init();
}

extern "C" nfc_err_t nfc_init_with_default(const nfc_default_record_t *def) {
    nfc_err_t res = nfc_core_init();
    if (res != NFC_OK) {
        return res;
    }
    if (!def || def->type == NFC_DEFAULT_NONE) {
        return NFC_OK;
    }
    std::vector<espp::Ndef> records;
    switch (def->type) {
        case NFC_DEFAULT_TEXT: {
            if (!def->text) {
                ESP_LOGW(TAG, "Default TEXT type missing text pointer");
                return NFC_OK;
            }
            records.emplace_back(espp::Ndef::make_text(def->text));
            break;
        }
        case NFC_DEFAULT_URL: {
            if (!def->url) {
                ESP_LOGW(TAG, "Default URL type missing url pointer");
                return NFC_OK;
            }
            // Attempt to classify prefix for URI compression if possible
            espp::Ndef::Uic uic = espp::Ndef::Uic::NONE;
            if (strncmp(def->url, "https://", 8) == 0) {
                uic = espp::Ndef::Uic::HTTPS;
                records.emplace_back(espp::Ndef::make_uri(def->url + 8, uic));
            } else if (strncmp(def->url, "http://", 7) == 0) {
                uic = espp::Ndef::Uic::HTTP;
                records.emplace_back(espp::Ndef::make_uri(def->url + 7, uic));
            } else {
                records.emplace_back(espp::Ndef::make_uri(def->url, uic));
            }
            break;
        }
        case NFC_DEFAULT_WIFI: {
            if (!def->wifi_ssid || !def->wifi_key) {
                ESP_LOGW(TAG, "Default WIFI type missing ssid or key");
                return NFC_OK;
            }
            records.emplace_back(espp::Ndef::make_wifi_config({.ssid = def->wifi_ssid, .key = def->wifi_key}));
            break;
        }
        default: return NFC_OK;
    }
    if (!records.empty()) {
        std::error_code ec;
        st25dv_device->set_records(records, ec);
        if (ec) {
            ESP_LOGE(TAG, "Failed to write default NDEF record: %s", ec.message().c_str());
            return NFC_ERR_WRITE;
        }
        ESP_LOGI(TAG, "Default NDEF record provisioned (type=%d)", (int)def->type);
    }
    return NFC_OK;
}

extern "C" void nfc_deinit() {
    nfc_stop();

    // Reset initialization flag
    st25dv_initialized = false;

    // Clean up GPO interrupt
    if (gpo_interrupt_enabled) {
        gpio_isr_handler_remove((gpio_num_t)NFC_GPO_PIN);
        gpo_interrupt_enabled = false;
    }

    if (nfc_isr_semaphore) {
        vSemaphoreDelete(nfc_isr_semaphore);
        nfc_isr_semaphore = nullptr;
    }

    if (st25dv_device) {
        delete st25dv_device;
        st25dv_device = nullptr;
    }
    if (nfc_device.handle) {
        i2c_manager_remove_device(&nfc_device);
        nfc_device.handle = nullptr;
    }
}

extern "C" bool nfc_ready() {
    return st25dv_initialized;
}

extern "C" const char *nfc_err_to_string(nfc_err_t err) {
    switch (err) {
        case NFC_OK: return "No error";
        case NFC_ERR_INIT: return "Initialization error";
        case NFC_ERR_READ: return "Read error";
        case NFC_ERR_WRITE: return "Write error";
        case NFC_ERR_TIMEOUT: return "Timeout error";
        default: return "Unknown error";
    }
}

extern "C" void nfc_set_event_callback(nfc_event_cb_t callback, void *arg) {
    nfc_event_callback     = callback;
    nfc_event_callback_arg = arg;
}

static void nfc_task(void *) {
    espp::St25dv::IT_STS last_status;
    bool was_field_present             = false;
    int64_t last_interrupt_time        = 0;
    int64_t last_field_change_time     = 0;
    int64_t last_rf_write_time         = 0;
    const int64_t debounce_time_us     = 50000;  // 50ms debounce for GPO
    const int64_t field_settle_time_us = 200000; // 200ms settling time after field changes
    const int64_t write_settle_time_us = 200000; // 200ms settle time after last RF_WRITE before processing

    // // For now just create a basic set of records for testing
    // std::vector<espp::Ndef> ndef_records;
    // ndef_records.emplace_back(espp::Ndef::make_text("Hello from the badge!"));
    // std::error_code ec;
    // st25dv_device->set_records(ndef_records, ec);
    // if (ec) {
    //     ESP_LOGE(TAG, "Failed to set NDEF records: %s", ec.message().c_str());
    //     return;
    // }
    // vTaskDelay(pdMS_TO_TICKS(50));
    // ESP_LOGD(TAG, "NDEF records set successfully");

    while (true) {
        bool interrupt_triggered = false;
        int64_t current_time     = esp_timer_get_time();

        // Wait for either GPO interrupt or timeout
        if (gpo_interrupt_enabled && nfc_isr_semaphore) {
            if (xSemaphoreTake(nfc_isr_semaphore, pdMS_TO_TICKS(NFC_POLL_PERIOD_MS)) == pdTRUE) {
                // Debounce rapid interrupts
                if (current_time - last_interrupt_time > debounce_time_us) {
                    interrupt_triggered = true;
                    last_interrupt_time = current_time;
                    ESP_LOGD(TAG, "GPO interrupt triggered");

                    // Drain any additional semaphore gives from rapid interrupts
                    while (xSemaphoreTake(nfc_isr_semaphore, 0) == pdTRUE) {
                        ESP_LOGD(TAG, "Draining extra interrupt");
                    }
                } else {
                    ESP_LOGD(TAG, "GPO interrupt debounced");
                }
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(NFC_POLL_PERIOD_MS));
        }

        if (interrupt_triggered) {
            ESP_LOGD(TAG, "GPO pulse width ~%u us", (unsigned)gpo_last_pulse_us);
        }

        // Read interrupt status with retry logic
        espp::St25dv::IT_STS status{};
        nfc_device.config.config.flags.disable_ack_check = 1; // Disable ACK check for status reads
        bool status_read_ok                              = i2c_retry_operation([&]() -> bool {
            std::error_code ec;
            status = st25dv_device->get_interrupt_status(ec);
            if (!ec) {
                return true;
            }
            ESP_LOGD(TAG, "Status read retry needed: %s", ec.message().c_str());
            vTaskDelay(pdMS_TO_TICKS(50));
            return false;
        });
        nfc_device.config.config.flags.disable_ack_check = 0; // Restore ACK check

        if (!status_read_ok) {
            ESP_LOGE(TAG, "NFC chip unresponsive, waiting for recovery...");
            // Give the chip more time to recover, then continue polling
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        // Check for field changes with additional settling time
        bool field_present = status.FIELD_RISING || (!status.FIELD_FALLING && was_field_present);

        if (field_present != was_field_present && nfc_event_callback) {
            // Only report field changes if enough time has passed since last change
            if (current_time - last_field_change_time > field_settle_time_us) {
                last_field_change_time = current_time;

                if (field_present) {
                    ESP_LOGD(TAG, "NFC field detected");
                    nfc_event_callback(NFC_EVENT_FIELD_DETECTED, nullptr, 0, nfc_event_callback_arg);
                } else {
                    ESP_LOGD(TAG, "NFC field lost");
                    nfc_event_callback(NFC_EVENT_FIELD_LOST, nullptr, 0, nfc_event_callback_arg);
                }
                was_field_present = field_present;
            } else {
                ESP_LOGD(TAG, "Field change debounced (too soon after last change)");
            }
        }

        // Track any RF_WRITE activity whenever status changes or interrupt fires
        if (interrupt_triggered || status != last_status) {
            if (status.RF_WRITE) {
                last_rf_write_time = current_time;
                ESP_LOGD(TAG, "NFC tag write flag set");
            }
        }

        // Check for pending write completion on EVERY loop iteration (not just status changes)
        // This ensures we process writes even if interrupts were debounced
        if (last_rf_write_time > 0) {
            int64_t time_since_rf_write = current_time - last_rf_write_time;

            // Only process write data if enough settling time has passed
            if (time_since_rf_write >= write_settle_time_us) {
                // Settling time has passed - now verify completion and check for changes
                uint16_t ndef_len = 0;
                if (!wait_ndef_complete(&ndef_len, 500)) {
                    ESP_LOGW(TAG, "NDEF write not completed after settling period");
                    last_rf_write_time = 0;
                    continue;
                }
                ESP_LOGD(TAG, "NDEF write completed, length=%u bytes", ndef_len);

                // Now check if data actually changed
                if (data_changed()) {
                    if (rate_guard()) {
                        ESP_LOGW(TAG, "NDEF write blocked due to rate limiting");
                        if (nfc_event_callback) {
                            nfc_event_callback(NFC_EVENT_WRITE_BLOCKED, nullptr, 0, nfc_event_callback_arg);
                        }
                    } else {
                        ESP_LOGD(TAG, "New NDEF data detected");
                        if (nfc_event_callback && !last_ndef_data.empty()) {
                            nfc_event_callback(NFC_EVENT_TAG_WRITE, last_ndef_data.data(), last_ndef_data.size(),
                                               nfc_event_callback_arg);
                        }
                    }
                } else {
                    ESP_LOGD(TAG, "NDEF data unchanged after write");
                }

                // Clear the write timestamp after processing
                last_rf_write_time = 0;
            }
        }

        last_status = status;
    }
}

extern "C" nfc_err_t nfc_start() {
    if (!st25dv_device) {
        ESP_LOGE(TAG, "NFC device not initialized");
        return NFC_ERR_INIT;
    }
    if (nfc_task_handle) {
        ESP_LOGW(TAG, "NFC task already running");
        return NFC_OK;
    }

    // Create the NFC task
    BaseType_t result = xTaskCreate(nfc_task, "nfc_task", NFC_TASK_STACK_SIZE, nullptr, NFC_TASK_PRIORITY, &nfc_task_handle);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create NFC task");
        return NFC_ERR_INIT;
    }

    ESP_LOGD(TAG, "NFC polling started with period: %u ms", NFC_POLL_PERIOD_MS);
    return NFC_OK;
}

extern "C" void nfc_stop() {
    if (nfc_task_handle) {
        vTaskDelete(nfc_task_handle);
        nfc_task_handle = nullptr;
        ESP_LOGD(TAG, "NFC polling stopped");
    } else {
        ESP_LOGW(TAG, "NFC task not running");
    }
}

extern "C" nfc_err_t nfc_read(uint8_t *buffer, size_t len) {
    if (!st25dv_device) {
        ESP_LOGE(TAG, "NFC device not initialized");
        return NFC_ERR_INIT;
    }
    if (!buffer || len == 0) {
        ESP_LOGE(TAG, "Invalid data buffer or length");
        return NFC_ERR_READ;
    }

    std::error_code ec;
    st25dv_device->read(buffer, len, ec);
    if (ec) {
        ESP_LOGE(TAG, "Failed to read NFC tag: %s", ec.message().c_str());
        return NFC_ERR_READ;
    }

    return NFC_OK;
}

extern "C" nfc_err_t nfc_write(const uint8_t *buffer, size_t len) {
    if (!st25dv_device) {
        ESP_LOGE(TAG, "NFC device not initialized");
        return NFC_ERR_INIT;
    }
    if (!buffer || len == 0) {
        ESP_LOGE(TAG, "Invalid data buffer or length");
        return NFC_ERR_WRITE;
    }

    std::error_code ec;
    std::string_view data(reinterpret_cast<const char *>(buffer), len);
    st25dv_device->write(data, ec);
    if (ec) {
        ESP_LOGE(TAG, "Failed to write to NFC tag: %s", ec.message().c_str());
        return NFC_ERR_WRITE;
    }

    return NFC_OK;
}

extern "C" nfc_err_t nfc_get_records(nfc_ndef_record_t **records, size_t *count) {
    if (!st25dv_device || !records || !count) {
        ESP_LOGE(TAG, "Invalid parameters for NDEF record reading");
        return NFC_ERR_READ;
    }

    *records = nullptr;
    *count   = 0;

    // Read raw data from the device
    std::error_code ec;
    uint8_t raw_data[255]; // Read up to 255 bytes of raw data
    st25dv_device->read(raw_data, sizeof(raw_data), ec);
    if (ec) {
        ESP_LOGE(TAG, "Failed to read raw data: %s", ec.message().c_str());
        return NFC_ERR_READ;
    }

    ESP_LOGD(TAG, "Raw data read (%zu bytes)", sizeof(raw_data));
    ESP_LOG_BUFFER_HEXDUMP(TAG, raw_data, sizeof(raw_data), ESP_LOG_DEBUG);

    // Locate the NDEF message TLV inside the Type-5 area
    const uint8_t *ndef_ptr = nullptr;
    size_t ndef_len         = 0;
    ndef_status_t s         = ndef_type5_find_ndef(raw_data, sizeof(raw_data), &ndef_ptr, &ndef_len);
    if (s != NDEF_OK) {
        ESP_LOGW(TAG, "No NDEF message found (status=%d)", (int)s);
        // Debug: Show TLV structure for first few bytes
        ESP_LOGD(TAG, "TLV analysis: [0-7] %02x %02x %02x %02x %02x %02x %02x %02x", raw_data[0], raw_data[1], raw_data[2],
                 raw_data[3], raw_data[4], raw_data[5], raw_data[6], raw_data[7]);
        return NFC_OK;
    }

    ESP_LOGD(TAG, "Found NDEF TLV: length=%zu bytes", ndef_len);

    // Parse the NDEF message
    ndef_message_t msg = {nullptr, 0};
    s                  = ndef_parse_message(ndef_ptr, ndef_len, &msg);
    if (s != NDEF_OK) {
        ESP_LOGE(TAG, "Malformed NDEF message (status=%d)", (int)s);
        return NFC_ERR_READ;
    }

    // Cap records to NFC_MAX_NDEF_RECORDS just in case
    size_t out_count = msg.count;
    if (out_count > NFC_MAX_NDEF_RECORDS) {
        out_count = NFC_MAX_NDEF_RECORDS;
    }

    nfc_ndef_record_t *out = (nfc_ndef_record_t *)calloc(out_count, sizeof(nfc_ndef_record_t));
    if (!out) {
        ndef_free_message(&msg);
        ESP_LOGE(TAG, "OOM allocating records");
        return NFC_ERR_READ;
    }

    // Map decoder records to your API
    for (size_t i = 0; i < out_count; ++i) {
        const ndef_record_t *r = &msg.records[i];
        nfc_ndef_record_t *dst = &out[i];

        // Decide a friendly type string when possible
        const char *friendly = nullptr;
        if (r->tnf == NDEF_TNF_WELL_KNOWN) {
            if (r->type_len == 1 && r->type && r->type[0] == 'T') {
                friendly = "text";
            } else if (r->type_len == 1 && r->type && r->type[0] == 'U') {
                friendly = "uri";
            }
        }

        if (!friendly) {
            // fallback: copy the raw Type field as a C string (best effort; types are usually ASCII)
            dst->type = (char *)malloc(r->type_len + 1);
            if (dst->type) {
                memcpy(dst->type, r->type, r->type_len);
                dst->type[r->type_len] = '\0';
                dst->type_len          = r->type_len;
            }
        } else {
            dst->type_len = strlen(friendly);
            dst->type     = (char *)malloc(dst->type_len + 1);
            if (dst->type) {
                memcpy(dst->type, friendly, dst->type_len + 1);
            }
        }

        // Copy payload bytes verbatim
        dst->payload_len = r->payload_len;
        if (dst->payload_len) {
            dst->payload = (uint8_t *)malloc(dst->payload_len);
            if (dst->payload) {
                memcpy(dst->payload, r->payload, dst->payload_len);
            } else {
                // leave payload_len set; free path will handle null pointer safely
            }
        }
    }

    // Done with the decoder structures
    ndef_free_message(&msg);

    *records = out;
    *count   = out_count;
    ESP_LOGD(TAG, "Parsed %u NDEF record(s)", (unsigned)out_count);
    return NFC_OK;
}

extern "C" nfc_err_t nfc_decode_text_record(const nfc_ndef_record_t *record, char **out_text, char **out_lang) {
    if (!out_text || !out_lang) {
        return NFC_ERR_READ;
    }
    *out_text = nullptr;
    *out_lang = nullptr;

    if (!record || !record->type || record->type_len == 0) {
        return NFC_ERR_READ;
    }

    // Accept either the friendly expanded type "text" or the raw well-known type 'T'
    bool is_text = false;
    if ((record->type_len == 1 && record->type[0] == 'T') || (strcmp(record->type, "text") == 0)) {
        is_text = true;
    }
    if (!is_text) {
        return NFC_ERR_READ;
    }

    if (record->payload_len < 1 || !record->payload) {
        ESP_LOGW(TAG, "Invalid text record payload");
        return NFC_ERR_READ;
    }

    uint8_t status   = record->payload[0];
    bool is_utf16    = (status & 0x80) != 0;
    uint8_t lang_len = status & 0x3F; // bits 5..0 store language code length

    if ((size_t)lang_len + 1 > record->payload_len) {
        ESP_LOGW(TAG, "Malformed text record (lang_len too long)");
        return NFC_ERR_READ;
    }

    // Extract language code (if present)
    if (lang_len > 0) {
        *out_lang = (char *)malloc(lang_len + 1);
        if (!*out_lang) {
            ESP_LOGE(TAG, "Failed to allocate memory for language code");
            return NFC_ERR_READ;
        }
        memcpy(*out_lang, &record->payload[1], lang_len);
        (*out_lang)[lang_len] = '\0';
    }

    size_t text_len = record->payload_len - 1 - lang_len;
    if (text_len > 0) {
        *out_text = (char *)malloc(text_len + 1);
        if (!*out_text) {
            if (*out_lang) {
                free(*out_lang);
                *out_lang = nullptr;
            }
            ESP_LOGE(TAG, "Failed to allocate memory for text");
            return NFC_ERR_READ;
        }
        memcpy(*out_text, &record->payload[1 + lang_len], text_len);
        (*out_text)[text_len] = '\0';

        if (is_utf16) {
            // We currently don't implement UTF-16 decoding; payload is left as raw bytes.
            ESP_LOGW(TAG, "UTF-16 text decoding not implemented; returning raw bytes");
        }
    }

    return NFC_OK;
}

extern "C" void nfc_free_records(nfc_ndef_record_t *records, size_t count) {
    if (!records) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        if (records[i].payload) {
            free(records[i].payload);
        }
        if (records[i].type) {
            free(records[i].type);
        }
    }
    free(records);
}

extern "C" nfc_err_t nfc_clear() {
    if (!st25dv_device) {
        ESP_LOGE(TAG, "NFC device not initialized");
        return NFC_ERR_INIT;
    }

    // Write an empty NDEF Type-5 layout: CC (E1 40 40 05), NDEF TLV (03 00), Terminator (FE)
    uint8_t empty[8]; // 7 bytes needed
    size_t need = ndef_type5_build_empty(empty, sizeof(empty));
    std::error_code ec;
    st25dv_device->write(std::string_view((const char *)empty, need), ec);
    if (ec) {
        ESP_LOGE(TAG, "Failed to clear data: %s", ec.message().c_str());
        return NFC_ERR_WRITE;
    }

    ESP_LOGD(TAG, "Data cleared");
    return NFC_OK;
}

extern "C" nfc_err_t nfc_write_text(const char *text) {
    if (!st25dv_device) {
        ESP_LOGE(TAG, "NFC device not initialized");
        return NFC_ERR_INIT;
    }
    if (!text) {
        ESP_LOGE(TAG, "Invalid text parameter");
        return NFC_ERR_WRITE;
    }

    // Create a text NDEF record using the espp library
    std::vector<espp::Ndef> ndef_records;
    ndef_records.emplace_back(espp::Ndef::make_text(text));

    // Write the records to the device
    std::error_code ec;
    st25dv_device->set_records(ndef_records, ec);
    if (ec) {
        ESP_LOGE(TAG, "Failed to write text record: %s", ec.message().c_str());
        return NFC_ERR_WRITE;
    }

    ESP_LOGD(TAG, "Wrote text record: '%s'", text);
    return NFC_OK;
}

extern "C" nfc_err_t nfc_write_wifi_config(const char *ssid, const char *password) {
    if (!st25dv_device) {
        ESP_LOGE(TAG, "NFC device not initialized");
        return NFC_ERR_INIT;
    }
    if (!ssid || !password) {
        ESP_LOGE(TAG, "Invalid WiFi parameters");
        return NFC_ERR_WRITE;
    }
    std::vector<espp::Ndef> records;
    records.emplace_back(espp::Ndef::make_wifi_config({.ssid = ssid, .key = password}));
    std::error_code ec;
    st25dv_device->set_records(records, ec);
    if (ec) {
        ESP_LOGE(TAG, "Failed to write WiFi record: %s", ec.message().c_str());
        return NFC_ERR_WRITE;
    }
    ESP_LOGD(TAG, "Wrote WiFi record: ssid='%s'", ssid);
    return NFC_OK;
}

extern "C" nfc_err_t nfc_write_url(const char *url) {
    if (!st25dv_device) {
        ESP_LOGE(TAG, "NFC device not initialized");
        return NFC_ERR_INIT;
    }
    if (!url) {
        ESP_LOGE(TAG, "Invalid URL parameter");
        return NFC_ERR_WRITE;
    }
    espp::Ndef::Uic uic = espp::Ndef::Uic::NONE;
    const char *suffix  = url;
    if (strncmp(url, "https://", 8) == 0) {
        uic    = espp::Ndef::Uic::HTTPS;
        suffix = url + 8;
    } else if (strncmp(url, "http://", 7) == 0) {
        uic    = espp::Ndef::Uic::HTTP;
        suffix = url + 7;
    }
    std::vector<espp::Ndef> records;
    records.emplace_back(espp::Ndef::make_uri(suffix, uic));
    std::error_code ec;
    st25dv_device->set_records(records, ec);
    if (ec) {
        ESP_LOGE(TAG, "Failed to write URL record: %s", ec.message().c_str());
        return NFC_ERR_WRITE;
    }
    ESP_LOGD(TAG, "Wrote URL record: '%s'", url);
    return NFC_OK;
}