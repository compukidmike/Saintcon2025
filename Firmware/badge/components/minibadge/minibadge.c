#include <memory.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "badge.h"
#include "i2c_manager.h"
#include "minibadge.h"

static const char *TAG = "minibadge";

#define I2C_BUS_MINIBADGE I2C_BUS_B

// Events
ESP_EVENT_DEFINE_BASE(MINIBADGE_DPAD_EVENT);

#define MINIBADGE_MAX_CALLBACKS      5
#define MINIBADGE_I2C_SCAN_PERIOD_MS 2000 // How often to scan for minibadges
#define MINIBADGE_CLK_PERIOD_MS      1000 // How often to toggle the minibadge CLK line
// #define MINIBADGE_CLK_PERIOD_MS       694                             // How often to toggle the minibadge CLK line
#define MINIBADGE_CLK_PIN_LOW         CONFIG_MINIBADGE_CLOCK_LOW_PIN  // The GPIO pin to toggle the minibadge CLK line - low
#define MINIBADGE_CLK_PIN_HIGH        CONFIG_MINIBADGE_CLOCK_HIGH_PIN // The GPIO pin to toggle the minibadge CLK line - high
#define MINIBADGE_DPAD_POLL_PERIOD_MS 100                             // How often to poll the D-pad minibadge

// Callbacks for minibadge events
static minibadge_event_cb_t minibadge_callbacks[MINIBADGE_MAX_CALLBACKS] = {0};

// Scan timer
static esp_timer_handle_t minibadge_scan_timer;

// Timer for toggling the minibadge CLK line every second
static esp_timer_handle_t minibadge_clk_timer;
static bool minibadge_clk_high = false;
static esp_timer_handle_t minibadge_dpad_detect_timer;

// Minibadge devices
minibadge_device_t minibadge_devices[MINIBADGE_SLOT_COUNT] = {0};

// The number of minibadges from the last scan
static uint8_t minibadge_count = 0;

// D-pad polling task
static TaskHandle_t minibadge_dpad_task_handle                  = NULL;
static minibadge_device_t *minibadge_dpad[MINIBADGE_SLOT_COUNT] = {0};
esp_event_loop_handle_t minibadge_event_loop_handle             = NULL;
static portMUX_TYPE minibadge_dpad_lock                         = portMUX_INITIALIZER_UNLOCKED;

// -------------------------------------------------------------------------------------------------
// D-pad handler deferred registration
// -------------------------------------------------------------------------------------------------
typedef struct {
    esp_event_handler_t handler;
    void *arg;
    bool registered;
} dpad_handler_entry_t;

#define MINIBADGE_DPAD_HANDLER_MAX 3
static dpad_handler_entry_t dpad_handlers[MINIBADGE_DPAD_HANDLER_MAX] = {0};

static void register_pending_dpad_handlers() {
    if (minibadge_event_loop_handle == NULL) {
        return;
    }
    for (int i = 0; i < MINIBADGE_DPAD_HANDLER_MAX; i++) {
        if (dpad_handlers[i].handler && !dpad_handlers[i].registered) {
            esp_err_t r = esp_event_handler_register_with(minibadge_event_loop_handle, MINIBADGE_DPAD_EVENT, ESP_EVENT_ANY_ID,
                                                          dpad_handlers[i].handler, dpad_handlers[i].arg);
            if (r == ESP_OK) {
                dpad_handlers[i].registered = true;
            } else {
                ESP_LOGE(TAG, "Failed to register D-pad handler (%d): %s", i, esp_err_to_name(r));
            }
        }
    }
}

esp_err_t minibadge_register_dpad_handler(esp_event_handler_t handler, void *arg) {
    for (int i = 0; i < MINIBADGE_DPAD_HANDLER_MAX; i++) {
        if (dpad_handlers[i].handler == handler) {
            // Already present
            return ESP_OK;
        }
        if (dpad_handlers[i].handler == NULL) {
            dpad_handlers[i].handler    = handler;
            dpad_handlers[i].arg        = arg;
            dpad_handlers[i].registered = false;
            register_pending_dpad_handlers();
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

// Function prototypes
static void check_minibadges_periodic(void *arg);
static void minibadge_clk_toggle(void *arg);
static void minibadge_dpad_task(void *arg);

// -------------------------------------------------------------------------------------------------
// D-pad Auto-Detection (handles re-plug without EEPROM presence)
// -------------------------------------------------------------------------------------------------
static void minibadge_dpad_detect_cb(void *arg) {
    (void)arg;
    if (minibadge_dpad_task_handle != NULL) {
        return; // already polling
    }
    for (uint8_t i = 0; i < MINIBADGE_SLOT_COUNT; i++) {
        i2c_manager_device_config_t probe_cfg = {
            .bus_index = I2C_BUS_MINIBADGE,
#ifdef CONFIG_I2C_SWITCH_ENABLED
            .channel = I2C_SWITCH_CHANNEL_0 + i,
#endif
            .config =
                {
                    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                    .device_address  = MINIBADGE_I2C_ADDR_DPAD,
                    .scl_speed_hz    = 100000,
                },
        };
        if (i2c_manager_ping(&probe_cfg) == ESP_OK) {
            minibadge_dpad[i] = &minibadge_devices[i];
            minibadge_dpad_poll(true, i);
            minibadge_event_t inserted = {
                .type = MINIBADGE_EVENT_INSERTED,
                .slot = i,
            };
            for (uint8_t cb = 0; cb < MINIBADGE_MAX_CALLBACKS; cb++) {
                if (minibadge_callbacks[cb]) {
                    minibadge_callbacks[cb](inserted);
                }
            }
            ESP_LOGI(TAG, "Auto-detected D-pad minibadge on slot %d", i + 1);
            break;
        }
    }
}

esp_err_t minibadge_init() {
    for (uint8_t i = 0; i < MINIBADGE_SLOT_COUNT; i++) {
        minibadge_devices[i].slot = i;
    }
    // Set up a timer to scan for minibadges and update the minibadge_devices array
    esp_timer_create_args_t timer_args = {
        .callback = &check_minibadges_periodic,
        .name     = "minibadge_monitor",
    };
    esp_err_t err = esp_timer_create(&timer_args, &minibadge_scan_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create minibadge scan timer: %s", esp_err_to_name(err));
        return err;
    }

    // Start the minibadge scan timer
    err = esp_timer_start_periodic(minibadge_scan_timer, MINIBADGE_I2C_SCAN_PERIOD_MS * 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start minibadge scan timer: %s", esp_err_to_name(err));
        return err;
    }

    // Initialize the minibadge CLK pins
    gpio_config_t clk_pin_config = {
        .pin_bit_mask = (1ULL << MINIBADGE_CLK_PIN_LOW) | (1ULL << MINIBADGE_CLK_PIN_HIGH),
        .mode         = GPIO_MODE_OUTPUT,
    };
    gpio_config(&clk_pin_config);

    // Set the minibadge CLK pins low to start
    gpio_set_level(MINIBADGE_CLK_PIN_LOW, 0);
    gpio_set_level(MINIBADGE_CLK_PIN_HIGH, 0);

    // Create a timer to toggle the minibadge CLK line every second
    esp_timer_create_args_t clk_timer_args = {
        .callback = &minibadge_clk_toggle,
        .name     = "minibadge_clk",
    };
    err = esp_timer_create(&clk_timer_args, &minibadge_clk_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create minibadge CLK timer: %s", esp_err_to_name(err));
        return err;
    }

    // Start the minibadge CLK timer
    err = esp_timer_start_periodic(minibadge_clk_timer, MINIBADGE_CLK_PERIOD_MS * 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start minibadge CLK timer: %s", esp_err_to_name(err));
        return err;
    }

    // Create an event loop for the D-pad events
    esp_event_loop_args_t event_loop_args = {
        .queue_size      = 10,
        .task_name       = "minibadge_dpad_event_loop",
        .task_stack_size = 4096,
        .task_priority   = 5,
    };
    esp_event_loop_create(&event_loop_args, &minibadge_event_loop_handle);

    // Register any handlers that were requested before init
    register_pending_dpad_handlers();

    // Create & start periodic D-pad auto-detect timer (every 1s)
    esp_timer_create_args_t dpad_detect_args = {
        .callback = &minibadge_dpad_detect_cb,
        .name     = "minibadge_dpad_detect",
    };
    esp_err_t terr = esp_timer_create(&dpad_detect_args, &minibadge_dpad_detect_timer);
    if (terr == ESP_OK) {
        esp_timer_start_periodic(minibadge_dpad_detect_timer, 1000 * 1000);
    } else {
        ESP_LOGW(TAG, "Failed to create D-pad detect timer: %s", esp_err_to_name(terr));
    }

    return ESP_OK;
}

esp_err_t minibadge_add_event_callback(minibadge_event_cb_t callback) {
    for (uint8_t i = 0; i < MINIBADGE_MAX_CALLBACKS; i++) {
        if (minibadge_callbacks[i] == NULL) {
            minibadge_callbacks[i] = callback;
            return ESP_OK;
        }
    }
    ESP_LOGW(TAG, "No space for additional minibadge event callbacks");
    return ESP_ERR_NO_MEM;
}

esp_err_t minibadge_remove_event_callback(minibadge_event_cb_t callback) {
    for (uint8_t i = 0; i < MINIBADGE_MAX_CALLBACKS; i++) {
        if (minibadge_callbacks[i] == callback) {
            minibadge_callbacks[i] = NULL;
            return ESP_OK;
        }
    }
    ESP_LOGW(TAG, "Minibadge event callback not found");
    return ESP_ERR_NOT_FOUND;
}

// Function to compare two minibadge devices
static bool minibadge_device_equals(minibadge_device_t *a, minibadge_device_t *b) {
    return a->type == b->type && a->address == b->address && a->slot == b->slot &&
           memcmp(a->data, b->data, sizeof(a->data)) == 0 && memcmp(a->serial, b->serial, sizeof(a->serial)) == 0;
}

static bool serial_is_valid(const uint8_t *serial) {
    for (int i = 0; i < 16; i++) {
        // If the byte is not 0x00 or 0xFF, it's probably a valid serial number
        if (serial[i] != 0x00 && serial[i] != 0xFF) {
            return true;
        }
    }
    return false;
}

static void check_minibadges_periodic(void *arg) {
    // Copy the minibadge devices array to compare later
    minibadge_device_t prev_minibadge_devices[MINIBADGE_SLOT_COUNT];
    memcpy(prev_minibadge_devices, minibadge_devices, sizeof(minibadge_devices));

    // Scan for minibadge devices
    check_minibadges();
    for (uint8_t i = 0; i < MINIBADGE_SLOT_COUNT; i++) {
        if (!minibadge_device_equals(&minibadge_devices[i], &prev_minibadge_devices[i])) {
            bool swapped =
                minibadge_devices[i].type != MINIBADGE_TYPE_NONE && prev_minibadge_devices[i].type != MINIBADGE_TYPE_NONE;

            // Minibadge event data
            minibadge_event_t event_data = {
                .type = minibadge_devices[i].type == MINIBADGE_TYPE_NONE ? MINIBADGE_EVENT_REMOVED
                        : swapped                                        ? MINIBADGE_EVENT_REPLACED
                                                                         : MINIBADGE_EVENT_INSERTED,
                .slot = minibadge_devices[i].slot,
            };

            // Call the minibadge event callbacks
            for (uint8_t j = 0; j < MINIBADGE_MAX_CALLBACKS; j++) {
                if (minibadge_callbacks[j] != NULL) {
                    minibadge_callbacks[j](event_data);
                }
            }
        }
    }
}

uint8_t minibadge_get_count() {
    return minibadge_count;
}

// Scan for minibadge devices and update the minibadge_devices array
int8_t check_minibadges() {
    // Ensure I2C manager is initialized
    if (!i2c_manager_initialized) {
        ESP_LOGW(TAG, "I2C manager not initialized, skipping minibadge scan");
        return 0;
    }

    // Clear the minibadge devices array
    memset(minibadge_devices, 0, sizeof(minibadge_devices));

    // Scan for minibadge devices
    esp_err_t err;
    int8_t device_count = 0;
    for (uint8_t i = 0; i < MINIBADGE_SLOT_COUNT; i++) {
        // Set the slot number
        minibadge_devices[i].slot = i;

        // Construct a device configuration assuming it's an EEPROM
        i2c_manager_device_config_t device = {
            .bus_index = I2C_BUS_MINIBADGE,
#ifdef CONFIG_I2C_SWITCH_ENABLED
            .channel = I2C_SWITCH_CHANNEL_0 + i,
#endif
            .config = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
                       .device_address  = MINIBADGE_I2C_ADDR_EEPROM,
                       .scl_speed_hz    = 100000},
        };

        // Try to ping the device first to see if it actually exists and responds
        err = i2c_manager_ping(&device);
        if (err != ESP_OK) {
            continue;
        }

        // We have a device, try to read the name from the EEPROM
        uint8_t mb_name[8] = {0};
        err                = i2c_manager_read_eeprom(&device, MINIBADGE_EEPROM_ADDR_DATA, mb_name, sizeof(mb_name));
        if (err == ESP_OK) {
            // It's at least a basic type
            minibadge_devices[i].type    = MINIBADGE_TYPE_EEPROM_BASIC;
            minibadge_devices[i].address = MINIBADGE_I2C_ADDR_EEPROM;
            memcpy(minibadge_devices[i].data, mb_name, sizeof(mb_name));

            // Try to read a serial number from the EEPROM
            uint8_t mb_serial[16]        = {0};
            device.config.device_address = MINIBADGE_I2C_ADDR_EEPROM + 0x08; // EEPROM serial is at 0x58
            err = i2c_manager_read_eeprom(&device, MINIBADGE_EEPROM_ADDR_SERIAL, mb_serial, sizeof(mb_serial));
            if (err == ESP_OK && serial_is_valid(mb_serial)) {
                minibadge_devices[i].type = MINIBADGE_TYPE_EEPROM_SERIALIZED;
                memcpy(minibadge_devices[i].serial, mb_serial, sizeof(mb_serial));
            }

            device_count++;
        }
    }

    minibadge_count = device_count;
    return device_count;
}

static void minibadge_clk_toggle(void *arg) {
    gpio_set_level(MINIBADGE_CLK_PIN_LOW, minibadge_clk_high);
    gpio_set_level(MINIBADGE_CLK_PIN_HIGH, !minibadge_clk_high);
    minibadge_clk_high = !minibadge_clk_high;
}

void minibadge_dpad_poll(bool enable, minibadge_slot_t slot) {
    minibadge_dpad[slot] = enable ? &minibadge_devices[slot] : NULL;
    int slots_enabled    = 0;
    for (uint8_t i = 0; i < MINIBADGE_SLOT_COUNT; i++) {
        if (minibadge_dpad[i] != NULL) {
            slots_enabled++;
        }
    }

    if (slots_enabled > 0) {
        if (minibadge_dpad_task_handle != NULL) {
            return;
        }
        xTaskCreate(minibadge_dpad_task, "minibadge_dpad", 4096, NULL, 5, &minibadge_dpad_task_handle);
    } else {
        taskENTER_CRITICAL(&minibadge_dpad_lock);
        if (minibadge_dpad_task_handle != NULL) {
            if (xTaskGetCurrentTaskHandle() == minibadge_dpad_task_handle) {
                vTaskDelete(NULL);
            } else {
                vTaskDelete(minibadge_dpad_task_handle);
            }
            minibadge_dpad_task_handle = NULL;
        }
        taskEXIT_CRITICAL(&minibadge_dpad_lock);
    }
}

bool minibadge_dpad_polling_active() {
    return minibadge_dpad_task_handle != NULL;
}

static void minibadge_dpad_task(void *arg) {
    i2c_manager_device_config_t devices[MINIBADGE_SLOT_COUNT] = {0};
    uint8_t prev_state[MINIBADGE_SLOT_COUNT]                  = {0};
    uint8_t error_count[MINIBADGE_SLOT_COUNT]                 = {0};
    const uint8_t error_threshold                             = 5; // consecutive failures before disabling slot

    for (uint8_t i = 0; i < MINIBADGE_SLOT_COUNT; i++) {
        devices[i] = (i2c_manager_device_config_t){
            .bus_index = I2C_BUS_MINIBADGE,
#ifdef CONFIG_I2C_SWITCH_ENABLED
            .channel = I2C_SWITCH_CHANNEL_0 + i,
#endif
            .config =
                {
                    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                    .device_address  = MINIBADGE_I2C_ADDR_DPAD,
                    .scl_speed_hz    = 100000,
                },
        };
    }
    esp_err_t err;

    // Continually read the D-pad minibadge until the task is deleted
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(MINIBADGE_DPAD_POLL_PERIOD_MS));

        // Read the D-pad minibadge
        for (uint8_t i = 0; i < MINIBADGE_SLOT_COUNT; i++) {
            if (devices[i].config.device_address != 0 && minibadge_dpad[i] != NULL) {
                uint8_t dpad_state = 0;
                err                = i2c_manager_receive(&devices[i], &dpad_state, sizeof(dpad_state), 100);
                if (err != ESP_OK) {
                    // Limit log spam: first failure is WARN, subsequent are DEBUG until threshold hit
                    if (error_count[i] == 0) {
                        ESP_LOGW(TAG, "D-pad read failed (slot %d): %s", i + 1, esp_err_to_name(err));
                    } else if (error_count[i] < error_threshold) {
                        ESP_LOGD(TAG, "D-pad read retry %d (slot %d): %s", error_count[i], i + 1, esp_err_to_name(err));
                    }
                    if (++error_count[i] >= error_threshold) {
                        // Treat as unplugged: emit a NONE state (release) if last known state not NONE
                        if (prev_state[i] != MINIBADGE_DPAD_NONE) {
                            minibadge_dpad_event_t event_data = {
                                .slot  = i,
                                .state = MINIBADGE_DPAD_NONE,
                            };
                            esp_event_post_to(minibadge_event_loop_handle, MINIBADGE_DPAD_EVENT, MINIBADGE_DPAD_EVENT_PRESS,
                                              &event_data, sizeof(event_data), portMAX_DELAY);
                        }
                        ESP_LOGI(TAG, "Stopping D-pad polling for slot %d after %d consecutive failures", i + 1, error_count[i]);
                        minibadge_dpad[i] = NULL; // disable this slot

                        // Notify higher-level callbacks of removal (synthetic since no EEPROM change)
                        minibadge_event_t removal = {
                            .type = MINIBADGE_EVENT_REMOVED,
                            .slot = i,
                        };
                        for (uint8_t cb = 0; cb < MINIBADGE_MAX_CALLBACKS; cb++) {
                            if (minibadge_callbacks[cb]) {
                                minibadge_callbacks[cb](removal);
                            }
                        }

                        // If no more slots active, end the task
                        int active = 0;
                        for (uint8_t j = 0; j < MINIBADGE_SLOT_COUNT; j++) {
                            if (minibadge_dpad[j] != NULL) {
                                active++;
                            }
                        }
                        if (active == 0) {
                            taskENTER_CRITICAL(&minibadge_dpad_lock);
                            minibadge_dpad_task_handle = NULL;
                            taskEXIT_CRITICAL(&minibadge_dpad_lock);
                            vTaskDelete(NULL);
                        }
                    }
                    continue; // skip state-change processing on failure
                }

                // Successful read resets error counter
                if (error_count[i] > 0) {
                    if (error_count[i] >= error_threshold) {
                        ESP_LOGI(TAG, "D-pad recovered (slot %d)", i + 1);
                    }
                    error_count[i] = 0;
                }

                // If the D-pad state has changed, post an event
                if (dpad_state != prev_state[i]) {
                    minibadge_dpad_event_t event_data = {
                        .slot  = minibadge_dpad[i]->slot,
                        .state = dpad_state,
                    };
                    esp_event_post_to(minibadge_event_loop_handle, MINIBADGE_DPAD_EVENT, MINIBADGE_DPAD_EVENT_PRESS, &event_data,
                                      sizeof(event_data), portMAX_DELAY);
                    prev_state[i] = dpad_state;

                    // Reset the screen timeout like we do for the touch events
                    screen_timeout_reset();

                    ESP_LOGD(TAG, "D-pad state[slot %d]: %02X", i + 1, dpad_state);
                }
            }
        }
    }
}
