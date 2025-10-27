#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "si_format.h"
#include "i2c_manager.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"

// Private include from esp_driver_i2c so we can override attributes
#include "i2c_private.h"

static const char *TAG = "i2c_manager";
esp_err_t check_bus(uint8_t bus_index);

bool i2c_manager_initialized                            = false;
i2c_manager_bus_t i2c_bus[SOC_I2C_NUM]                  = {0};
i2c_manager_known_device_t i2c_devices[MAX_I2C_DEVICES] = {0};
uint8_t i2c_device_count                                = 0;
bool i2c_allow_reserved                                 = false;
// Protects i2c_devices[] and i2c_device_count
static SemaphoreHandle_t device_list_lock = NULL;

// Use 100 kHz as the default I2C bus speed
#define I2C_DEFAULT_BUS_SPEED 100000

// -------------------------------------------------------------------------------------------------
// Mux capability matrix
// -------------------------------------------------------------------------------------------------
typedef struct {
    i2c_mux_type_t type;
    uint8_t channels;
    bool supports_irq;          // aggregated interrupt bits readable
    bool channel_write_oneshot; // selection uses one-hot bit pattern
    bool status_reflects_ctrl;  // read returns control+status bits (TCA9544A)
} mux_cap_t;

#define MAX_MUX_CHANNELS 8 // Make sure to update this if adding a mux with more channels

static const mux_cap_t mux_capabilities[] = {
    {I2C_MUX_NONE, 0, false, false, false},
    {I2C_MUX_TCA9544A, 4, true, false, true},
    {I2C_MUX_PCA9548A, 8, false, true, false},
};

static const mux_cap_t *mux_get_caps(i2c_mux_type_t type) {
    for (size_t i = 0; i < sizeof(mux_capabilities) / sizeof(mux_capabilities[0]); i++) {
        if (mux_capabilities[i].type == type) {
            return &mux_capabilities[i];
        }
    }
    return NULL;
}

// -------------------------------------------------------------------------------------------------
// Mux helpers
// -------------------------------------------------------------------------------------------------

#if (defined(CONFIG_I2C_MANAGER_BUS_A_MUX_INT_ENABLE) || defined(CONFIG_I2C_MANAGER_BUS_B_MUX_INT_ENABLE))
    #define I2C_MANAGER_MUX_INT_ENABLED
#endif

// Interrupt callback record
typedef struct {
    i2c_manager_mux_int_cb_t cb;
    void *ctx;
} mux_irq_slot_t;

typedef struct {
    uint8_t bus_index;
} mux_irq_event_t;

static mux_irq_slot_t mux_irq_callbacks[SOC_I2C_NUM][MAX_MUX_CHANNELS];
#ifdef I2C_MANAGER_MUX_INT_ENABLED
static QueueHandle_t mux_irq_queue = NULL; // queue of bus indexes needing service

static void IRAM_ATTR mux_int_isr(void *arg) {
    uint32_t bus_index  = (uint32_t)arg;
    mux_irq_event_t evt = {.bus_index = (uint8_t)bus_index};
    BaseType_t hpw      = pdFALSE;
    if (mux_irq_queue) {
        xQueueSendFromISR(mux_irq_queue, &evt, &hpw);
    }
    if (hpw) {
        portYIELD_FROM_ISR();
    }
}

static void mux_irq_task(void *arg) {
    mux_irq_event_t evt;
    while (1) {
        if (xQueueReceive(mux_irq_queue, &evt, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        uint8_t bus_index = evt.bus_index;
        if (check_bus(bus_index) != ESP_OK) {
            continue;
        }
        i2c_manager_bus_t *bus = &i2c_bus[bus_index];
        const mux_cap_t *caps  = mux_get_caps(bus->mux.type);
        if (!caps || !caps->supports_irq || !bus->mux.present) {
            continue;
        }
        // Read status/control (same register we write) to see channel + interrupt bits
        uint32_t raw = 0;
        if (i2c_manager_mux_status(bus_index, &raw) != ESP_OK) {
            continue;
        }
        // Bits 4..7 are interrupt flags per datasheet, channel enable bits 0..1.
        uint8_t int_flags = (raw >> 4) & 0x0F;
        if (!int_flags) {
            continue;
        }
        for (uint8_t ch = 0; ch < bus->mux.channels && ch < 8; ch++) {
            if (int_flags & (1U << ch)) {
                mux_irq_slot_t *slot = &mux_irq_callbacks[bus_index][ch];
                if (slot->cb) {
                    slot->cb(bus_index, ch, raw, slot->ctx);
                }
            }
        }
    }
}
#endif

static esp_err_t mux_write(uint8_t bus_index, uint8_t address, uint8_t byte) {
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = address,
        .scl_speed_hz    = I2C_DEFAULT_BUS_SPEED,
    };
    i2c_master_dev_handle_t handle;
    if (i2c_master_bus_add_device(i2c_bus[bus_index].handle, &cfg, &handle) != ESP_OK) {
        return ESP_FAIL;
    }
    esp_err_t r = i2c_master_transmit(handle, &byte, 1, 50);
    i2c_master_bus_rm_device(handle);
    return r;
}

static esp_err_t mux_read(uint8_t bus_index, uint8_t address, uint8_t *byte) {
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = address,
        .scl_speed_hz    = I2C_DEFAULT_BUS_SPEED,
    };
    i2c_master_dev_handle_t handle;
    if (i2c_master_bus_add_device(i2c_bus[bus_index].handle, &cfg, &handle) != ESP_OK) {
        return ESP_FAIL;
    }
    esp_err_t r = i2c_master_receive(handle, byte, 1, 50);
    i2c_master_bus_rm_device(handle);
    return r;
}

esp_err_t i2c_manager_mux_register(uint8_t bus_index, i2c_mux_type_t type, uint8_t address) {
    if (bus_index >= SOC_I2C_NUM) {
        return ESP_ERR_INVALID_ARG;
    }
    i2c_manager_bus_t *bus = &i2c_bus[bus_index];
    if (!bus->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    bus->mux.type            = type;
    bus->mux.address         = address;
    bus->mux.present         = false;
    bus->mux.current_channel = 0xFF;
    bus->mux.int_gpio        = -1;
    const mux_cap_t *caps    = mux_get_caps(type);
    if (!caps) {
        return ESP_ERR_INVALID_ARG;
    }
    if (type == I2C_MUX_NONE) {
        return ESP_OK;
    }
    bus->mux.channels = caps->channels;
    uint8_t check     = 0xFF;
    if (mux_read(bus_index, address, &check) == ESP_OK) {
        bus->mux.present = true;
    } else {
        ESP_LOGW(TAG, "Mux 0x%02X probe failed on bus %d", address, bus_index);
    }
    return ESP_OK;
}

esp_err_t i2c_manager_mux_select(uint8_t bus_index, uint8_t channel) {
    if (bus_index >= SOC_I2C_NUM) {
        return ESP_ERR_INVALID_ARG;
    }
    i2c_manager_bus_t *bus = &i2c_bus[bus_index];
    if (!bus->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (bus->mux.type == I2C_MUX_NONE || !bus->mux.present) {
        return ESP_OK; // nothing to do
    }
    if (channel >= bus->mux.channels) {
        return ESP_ERR_INVALID_ARG;
    }
    if (bus->mux.current_channel == channel) {
        return ESP_OK;
    }
    uint8_t cmd;
    const mux_cap_t *caps = mux_get_caps(bus->mux.type);

    if (caps && !caps->channel_write_oneshot) {
        // The TCA9544A control register format is as follows:
        //   | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
        //   | I | I | I | I | - | E | C | C |
        //
        //   I: Interrupt bits (INT3, INT2, INT1, INT0)
        //   E: Enable bit
        //   C: Channel select bits
        cmd = 0b100 | (channel & 0b011);
    } else {
        cmd = (1u << channel);
    }
    if (mux_write(bus_index, bus->mux.address, cmd) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to select mux channel %u on bus %d", channel, bus_index);
        return ESP_FAIL;
    }
    bus->mux.current_channel = channel;
    return ESP_OK;
}

esp_err_t i2c_manager_mux_status(uint8_t bus_index, uint32_t *raw) {
    if (raw == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (bus_index >= SOC_I2C_NUM) {
        return ESP_ERR_INVALID_ARG;
    }
    i2c_manager_bus_t *bus = &i2c_bus[bus_index];
    if (bus->mux.type == I2C_MUX_NONE || !bus->mux.present) {
        *raw = 0;
        return ESP_OK;
    }
    uint8_t v = 0;
    if (mux_read(bus_index, bus->mux.address, &v) != ESP_OK) {
        return ESP_FAIL;
    }
    *raw = v;
    return ESP_OK;
}

esp_err_t i2c_manager_mux_register_irq(uint8_t bus_index, uint8_t channel, i2c_manager_mux_int_cb_t cb, void *ctx) {
    ESP_RETURN_ON_ERROR(check_bus(bus_index), TAG, "Invalid bus index");
    i2c_manager_bus_t *bus = &i2c_bus[bus_index];
    const mux_cap_t *caps  = mux_get_caps(bus->mux.type);
    if (!caps || !caps->supports_irq || !bus->mux.present) {
        return ESP_ERR_INVALID_STATE;
    }
    if (channel >= bus->mux.channels || channel >= 8) {
        return ESP_ERR_INVALID_ARG;
    }
    mux_irq_callbacks[bus_index][channel] = (mux_irq_slot_t){.cb = cb, .ctx = ctx};
    return ESP_OK;
}

esp_err_t i2c_manager_mux_unregister_irq(uint8_t bus_index, uint8_t channel) {
    ESP_RETURN_ON_ERROR(check_bus(bus_index), TAG, "Invalid bus index");
    if (channel >= 8) {
        return ESP_ERR_INVALID_ARG;
    }
    mux_irq_callbacks[bus_index][channel] = (mux_irq_slot_t){0};
    return ESP_OK;
}

// -------------------------------------------------------------------------------------------------
// Basic validation helpers
// -------------------------------------------------------------------------------------------------

/**
 * @brief Check if the I2C bus is valid.
 *
 * @param bus_index The I2C bus index to check
 * @return esp_err_t
 *     - ESP_OK: Bus is valid
 *     - ESP_ERR_INVALID_STATE: I2C manager is not initialized
 *     - ESP_ERR_INVALID_ARG: Invalid bus index or bus is not initialized
 */
esp_err_t check_bus(uint8_t bus_index) {
    if (!i2c_manager_initialized) {
        ESP_LOGE(TAG, "I2C manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (bus_index >= SOC_I2C_NUM) {
        ESP_LOGE(TAG, "Invalid I2C bus index %d", bus_index);
        return ESP_ERR_INVALID_ARG;
    }
    if (!i2c_bus[bus_index].initialized) {
        ESP_LOGE(TAG, "I2C bus %d not initialized", bus_index);
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

/**
 * @brief Check the I2C device configuration for validity.
 *
 * @param device The I2C device configuration to check
 * @return esp_err_t
 *     - ESP_OK: Device configuration is valid
 *     - ESP_ERR_INVALID_ARG: Device configuration is invalid
 *     - ESP_FAIL: Device configuration is invalid
 */
esp_err_t check_device_config(i2c_manager_device_config_t *device) {
    if (device == NULL) {
        ESP_LOGE(TAG, "Invalid I2C device");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(check_bus(device->bus_index), TAG, "I2C bus validation error for bus %d", device->bus_index);
    if (device->channel != 0xFF) {
        i2c_manager_bus_t *bus = &i2c_bus[device->bus_index];
        if (bus->mux.type == I2C_MUX_NONE || !bus->mux.present) {
            // Allow but ignore channel if no mux
        } else if (device->channel >= bus->mux.channels) {
            ESP_LOGE(TAG, "Invalid mux channel %d (max %d)", device->channel, bus->mux.channels);
            return ESP_ERR_INVALID_ARG;
        }
    }
    // if (device->config.device_address == 0) {
    //     ESP_LOGE(TAG, "Invalid I2C device address 0x%X", device->config.device_address);
    //     return ESP_ERR_INVALID_ARG;
    // }
    if (!i2c_allow_reserved && (device->config.device_address < 8 || device->config.device_address > 120)) {
        ESP_LOGE(TAG, "I2C device address 0x%X is reserved or invalid", device->config.device_address);
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t i2c_manager_init(i2c_manager_pin_config_t *configs, uint8_t count) {
    if (count > SOC_I2C_NUM) {
        return ESP_ERR_INVALID_ARG;
    }
    if (i2c_manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    for (uint8_t i = 0; i < count; i++) {
        i2c_manager_pin_config_t *cfg = &configs[i];
        if (!cfg->enabled) {
            continue;
        }
        if (cfg->bus_index >= SOC_I2C_NUM) {
            return ESP_ERR_INVALID_ARG;
        }
        if (cfg->sda_pin < 0 || cfg->scl_pin < 0) {
            continue;
        }
        if (i2c_bus[cfg->bus_index].initialized) {
            ESP_LOGE(TAG, "Bus %d already initialized", cfg->bus_index);
            return ESP_ERR_INVALID_STATE;
        }
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port                     = cfg->bus_index,
            .sda_io_num                   = cfg->sda_pin,
            .scl_io_num                   = cfg->scl_pin,
            .clk_source                   = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt            = 10,
            .flags.enable_internal_pullup = cfg->enable_internal_pullups,
        };
        i2c_master_bus_handle_t handle;
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &handle));
        if (!i2c_bus[cfg->bus_index].lock) {
            i2c_bus[cfg->bus_index].lock = xSemaphoreCreateMutex();
            if (!i2c_bus[cfg->bus_index].lock) {
                return ESP_FAIL;
            }
        }
        i2c_bus[cfg->bus_index].initialized = true;
        i2c_bus[cfg->bus_index].pin_config  = *cfg;
        i2c_bus[cfg->bus_index].config      = bus_cfg;
        i2c_bus[cfg->bus_index].handle      = handle;
        i2c_bus[cfg->bus_index].mux.type    = I2C_MUX_NONE;
        ESP_LOGI(TAG, "Init bus %d SDA=%d SCL=%d", cfg->bus_index, cfg->sda_pin, cfg->scl_pin);
    }
    if (!device_list_lock) {
        device_list_lock = xSemaphoreCreateMutex();
        if (!device_list_lock) {
            return ESP_FAIL;
        }
    }
    i2c_manager_initialized = true;
    return ESP_OK;
}

esp_err_t i2c_manager_init_auto() {
    if (i2c_manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    i2c_manager_pin_config_t cfgs[2] = {0};
    uint8_t count                    = 0;
#ifdef CONFIG_I2C_MANAGER_BUS_A_ENABLE
    cfgs[count++] = (i2c_manager_pin_config_t){
        .bus_index               = I2C_BUS_A,
        .sda_pin                 = CONFIG_I2C_MANAGER_BUS_A_SDA_GPIO,
        .scl_pin                 = CONFIG_I2C_MANAGER_BUS_A_SCL_GPIO,
        .enable_internal_pullups = true,
        .default_speed_hz        = 100000,
        .enabled                 = true,
    };
#endif
#ifdef CONFIG_I2C_MANAGER_BUS_B_ENABLE
    cfgs[count++] = (i2c_manager_pin_config_t){
        .bus_index               = I2C_BUS_B,
        .sda_pin                 = CONFIG_I2C_MANAGER_BUS_B_SDA_GPIO,
        .scl_pin                 = CONFIG_I2C_MANAGER_BUS_B_SCL_GPIO,
        .enable_internal_pullups = true,
        .default_speed_hz        = 100000,
        .enabled                 = true,
    };
#endif
    esp_err_t r = i2c_manager_init(cfgs, count);
    if (r != ESP_OK) {
        return r;
    }
    // Register muxes per bus if configured
#ifdef CONFIG_I2C_MANAGER_BUS_A_ENABLE
    #if defined(CONFIG_I2C_MANAGER_BUS_A_MUX_TCA9544A)
    i2c_manager_mux_register(I2C_BUS_A, I2C_MUX_TCA9544A, CONFIG_I2C_MANAGER_BUS_A_MUX_ADDRESS);
    #elif defined(CONFIG_I2C_MANAGER_BUS_A_MUX_PCA9548A)
    i2c_manager_mux_register(I2C_BUS_A, I2C_MUX_PCA9548A, CONFIG_I2C_MANAGER_BUS_A_MUX_ADDRESS);
    #endif
#endif
#ifdef CONFIG_I2C_MANAGER_BUS_B_ENABLE
    #if defined(CONFIG_I2C_MANAGER_BUS_B_MUX_TCA9544A)
    i2c_manager_mux_register(I2C_BUS_B, I2C_MUX_TCA9544A, CONFIG_I2C_MANAGER_BUS_B_MUX_ADDRESS);
    #elif defined(CONFIG_I2C_MANAGER_BUS_B_MUX_PCA9548A)
    i2c_manager_mux_register(I2C_BUS_B, I2C_MUX_PCA9548A, CONFIG_I2C_MANAGER_BUS_B_MUX_ADDRESS);
    #endif
#endif
    // Setup INT GPIO(s) and service task if any enabled (only TCA9544A)
#ifdef I2C_MANAGER_MUX_INT_ENABLED
    if (!mux_irq_queue) {
        mux_irq_queue = xQueueCreate(8, sizeof(mux_irq_event_t));
        if (mux_irq_queue) {
            xTaskCreatePinnedToCore(mux_irq_task, "i2c_mux_irq", 3072, NULL, 5, NULL, tskNO_AFFINITY);
        }
    }
#endif

#ifdef CONFIG_I2C_MANAGER_BUS_A_MUX_INT_ENABLE
    if (i2c_bus[I2C_BUS_A].mux.type == I2C_MUX_TCA9544A && i2c_bus[I2C_BUS_A].mux.present) {
        int gpio                        = CONFIG_I2C_MANAGER_BUS_A_MUX_INT_GPIO;
        i2c_bus[I2C_BUS_A].mux.int_gpio = gpio;
        if (gpio >= 0) {
            gpio_config_t io = {.pin_bit_mask = 1ULL << gpio,
                                .mode         = GPIO_MODE_INPUT,
                                .pull_up_en   = 1,
                                .pull_down_en = 0,
                                .intr_type    = GPIO_INTR_NEGEDGE};
            gpio_config(&io);
            gpio_isr_handler_add(gpio, mux_int_isr, (void *)I2C_BUS_A);
        }
    }
#endif
#ifdef CONFIG_I2C_MANAGER_BUS_B_MUX_INT_ENABLE
    if (i2c_bus[I2C_BUS_B].mux.type == I2C_MUX_TCA9544A && i2c_bus[I2C_BUS_B].mux.present) {
        int gpio                        = CONFIG_I2C_MANAGER_BUS_B_MUX_INT_GPIO;
        i2c_bus[I2C_BUS_B].mux.int_gpio = gpio;
        if (gpio >= 0) {
            gpio_config_t io = {.pin_bit_mask = 1ULL << gpio,
                                .mode         = GPIO_MODE_INPUT,
                                .pull_up_en   = 1,
                                .pull_down_en = 0,
                                .intr_type    = GPIO_INTR_NEGEDGE};
            gpio_config(&io);
            gpio_isr_handler_add(gpio, mux_int_isr, (void *)I2C_BUS_B);
        }
    }
#endif
    return ESP_OK;
}

i2c_manager_bus_t *i2c_manager_get_bus(uint8_t bus_index) {
    if (check_bus(bus_index) != ESP_OK) {
        return NULL;
    }
    return &i2c_bus[bus_index];
}

bool i2c_manager_bus_locked(uint8_t bus_index) {
    if (check_bus(bus_index) != ESP_OK) {
        return false;
    }
    if (xSemaphoreTake(i2c_bus[bus_index].lock, 0) == pdTRUE) {
        xSemaphoreGive(i2c_bus[bus_index].lock);
        return false;
    } else {
        return true;
    }
}

bool i2c_manager_reserved_allowed() {
    return i2c_allow_reserved;
}

void i2c_manager_allow_reserved(bool allow) {
    i2c_allow_reserved = allow;
}

esp_err_t i2c_manager_ping(i2c_manager_device_config_t *device) {
    ESP_RETURN_ON_ERROR(check_device_config(device), TAG, "Invalid I2C device configuration");

    // Ping the device to see if it's there
    I2C_BUS_LOCK(device->bus_index, ESP_FAIL)
    if (device->channel != 0xFF) {
        i2c_manager_mux_select(device->bus_index, device->channel); // ignore error; probing will fail if unreachable
    }
    esp_err_t ret = i2c_master_probe(i2c_bus[device->bus_index].handle, device->config.device_address, 20);
    I2C_BUS_UNLOCK(device->bus_index);

    return ret;
}

esp_err_t i2c_manager_scan(uint8_t bus_index, uint8_t *found, uint8_t *count) {
    ESP_RETURN_ON_ERROR(check_bus(bus_index), TAG, "I2C bus validation error for bus %d", bus_index);
    if (!i2c_manager_bus_locked(bus_index)) {
        ESP_LOGW(TAG, "I2C bus %d should be locked before scanning", bus_index);
    }

    ESP_LOGD(TAG, "Scanning I2C bus %d", bus_index);
    for (uint8_t addr = 1; addr < 127; addr++) {
        if (i2c_master_probe(i2c_bus[bus_index].handle, addr, 20) == ESP_OK) {
            ESP_LOGD(TAG, "I2C device found at address 0x%X", addr);
            found[(*count)++] = addr;
        }
    }

    return ESP_OK;
}

uint8_t i2c_manager_discover(i2c_manager_device_criteria_t criteria, i2c_manager_device_config_t **results) {
    uint8_t device_count = 0;

    // Allocate memory for the results array
    if (*results == NULL) {
        *results = malloc(MAX_I2C_DEVICES * sizeof(i2c_manager_device_config_t));
    }

    if (criteria.bus_index == -1) {
        // Scan all buses
        for (uint8_t i = 0; i < SOC_I2C_NUM; i++) {
            if (i2c_bus[i].initialized) {
                device_count += i2c_manager_discover(
                    (i2c_manager_device_criteria_t){
                        .bus_index    = i,
                        .channel_mask = criteria.channel_mask,
                        .address      = criteria.address,
                    },
                    results);
            }
        }
    } else {
        // Scan the specified bus
        if (i2c_bus[criteria.bus_index].initialized) {
            I2C_BUS_LOCK(criteria.bus_index, 0)
            i2c_manager_bus_t *bus = &i2c_bus[criteria.bus_index];
            bool has_mux           = (bus->mux.type != I2C_MUX_NONE) && bus->mux.present;
            uint16_t channel_mask  = criteria.channel_mask;
            if (has_mux) {
                if (channel_mask == 0) {
                    channel_mask = (1U << bus->mux.channels) - 1U; // all channels
                } else {
                    // Mask out any channels beyond mux capability
                    uint16_t valid_mask = (1U << bus->mux.channels) - 1U;
                    channel_mask &= valid_mask;
                }
            } else {
                channel_mask = 0; // indicates no channel iteration
            }

            if (!has_mux) {
                uint8_t found[128] = {0};
                uint8_t count      = 0;
                i2c_manager_scan(criteria.bus_index, found, &count);
                for (uint8_t i = 0; i < count; i++) {
                    if (criteria.address == 0 || criteria.address == found[i]) {
                        (*results)[device_count++] = (i2c_manager_device_config_t){
                            .bus_index = criteria.bus_index,
                            .channel   = 0xFF,
                            .config =
                                {
                                    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                                    .device_address  = found[i],
                                    .scl_speed_hz    = I2C_DEFAULT_BUS_SPEED,
                                },
                        };
                    }
                }
            } else {
                for (uint8_t chan = 0; chan < bus->mux.channels; chan++) {
                    if ((channel_mask & (1U << chan)) == 0) {
                        continue;
                    }
                    if (i2c_manager_mux_select(criteria.bus_index, chan) != ESP_OK) {
                        continue;
                    }
                    uint8_t found[128] = {0};
                    uint8_t count      = 0;
                    i2c_manager_scan(criteria.bus_index, found, &count);
                    for (uint8_t i = 0; i < count; i++) {
                        if (found[i] == bus->mux.address) {
                            continue; // skip mux itself
                        }
                        if (criteria.address != 0 && criteria.address != found[i]) {
                            continue;
                        }
                        (*results)[device_count++] = (i2c_manager_device_config_t){
                            .bus_index = criteria.bus_index,
                            .channel   = chan,
                            .config =
                                {
                                    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                                    .device_address  = found[i],
                                    .scl_speed_hz    = I2C_DEFAULT_BUS_SPEED,
                                },
                        };
                    }
                }
            }

            I2C_BUS_UNLOCK(criteria.bus_index);
        }
    }

    return device_count;
}

esp_err_t i2c_manager_upsert_device(i2c_manager_device_config_t *device, i2c_master_dev_handle_t *handle) {
    ESP_RETURN_ON_ERROR(check_device_config(device), TAG, "Invalid I2C device configuration");

    if (handle == NULL) {
        ESP_LOGE(TAG, "Invalid I2C device handle");
        return ESP_ERR_INVALID_ARG;
    }

    if (i2c_device_count >= MAX_I2C_DEVICES) {
        ESP_LOGE(TAG, "Maximum number of I2C devices reached (%d)", MAX_I2C_DEVICES);
        return ESP_ERR_NO_MEM;
    }

    // Protect list while searching / modifying
    if (device_list_lock) {
        xSemaphoreTake(device_list_lock, portMAX_DELAY);
    }
    // Make sure we don't already have this device added
    for (uint8_t i = 0; i < i2c_device_count; i++) {
        i2c_manager_known_device_t *kd = &i2c_devices[i];

        if (kd->bus_index == device->bus_index && kd->address == device->config.device_address &&
            kd->channel == device->channel) {
            if (memcmp(&kd->config, &device->config, sizeof(i2c_device_config_t)) == 0) {
                // Device already exists with the same configuration, return the handle
                *handle = kd->handle;
                ESP_LOGD(TAG, "I2C device already exists on bus %d with address 0x%X, returning existing handle",
                         device->bus_index, device->config.device_address);
                return ESP_OK;
            } else {
                // Device exists but with a different configuration, remove it first
                esp_err_t rem_err = i2c_manager_remove_device(&i2c_devices[i]);
                if (rem_err != ESP_OK) {
                    if (device_list_lock) {
                        xSemaphoreGive(device_list_lock);
                    }
                    ESP_RETURN_ON_ERROR(rem_err, TAG, "Failed to remove existing I2C device on bus %d with address 0x%X",
                                        device->bus_index, device->config.device_address);
                }
            }
        }
    }

    // Add the device to the bus to get a handle
    esp_err_t add_err = i2c_master_bus_add_device(i2c_bus[device->bus_index].handle, &device->config, handle);
    if (add_err != ESP_OK) {
        if (device_list_lock) {
            xSemaphoreGive(device_list_lock);
        }
        ESP_RETURN_ON_ERROR(add_err, TAG, "Failed to add I2C device to bus %d", device->bus_index);
    }

    // Add the device to the list of known devices we're tracking
    i2c_manager_known_device_t known_device = {
        .bus_index = device->bus_index,
        .channel   = device->channel,
        .address   = device->config.device_address,
        .config    = *device,
        .handle    = *handle,
    };
    i2c_devices[i2c_device_count++] = known_device;
    if (device_list_lock) {
        xSemaphoreGive(device_list_lock);
    }

    return ESP_OK;
}

esp_err_t i2c_manager_remove_device(i2c_manager_known_device_t *device) {
    if (device_list_lock) {
        xSemaphoreTake(device_list_lock, portMAX_DELAY);
    }
    for (uint8_t i = 0; i < i2c_device_count; i++) {
        if (i2c_devices[i].bus_index == device->bus_index && i2c_devices[i].address == device->address &&
            i2c_devices[i].channel == device->channel) {
            i2c_master_bus_rm_device(i2c_devices[i].handle);
            i2c_device_count--;
            for (uint8_t j = i; j < i2c_device_count; j++) {
                i2c_devices[j] = i2c_devices[j + 1];
            }
            memset(&i2c_devices[i2c_device_count], 0, sizeof(i2c_manager_known_device_t));
            if (device_list_lock) {
                xSemaphoreGive(device_list_lock);
            }
            return ESP_OK;
        }
    }
    if (device_list_lock) {
        xSemaphoreGive(device_list_lock);
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t i2c_manager_find_device(i2c_manager_known_device_t *device, i2c_master_dev_handle_t *handle) {
    if (device_list_lock) {
        xSemaphoreTake(device_list_lock, portMAX_DELAY);
    }
    for (uint8_t i = 0; i < i2c_device_count; i++) {
        if (i2c_devices[i].bus_index == device->bus_index && i2c_devices[i].address == device->address &&
            i2c_devices[i].channel == device->channel) {
            *handle = i2c_devices[i].handle;
            if (device_list_lock) {
                xSemaphoreGive(device_list_lock);
            }
            return ESP_OK;
        }
    }
    if (device_list_lock) {
        xSemaphoreGive(device_list_lock);
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t i2c_manager_get_device(i2c_manager_device_config_t *device, i2c_master_dev_handle_t *handle, bool *upserted) {
    ESP_RETURN_ON_ERROR(check_device_config(device), TAG, "Invalid I2C device configuration");
    i2c_manager_known_device_t known_device = {
        .bus_index = device->bus_index,
        .channel   = device->channel,
        .address   = device->config.device_address,
    };
    if (i2c_manager_find_device(&known_device, handle) != ESP_OK) {
        ESP_RETURN_ON_ERROR(i2c_manager_upsert_device(device, handle), TAG, "Failed to upsert I2C device");
        *upserted = true;
    }

    return ESP_OK;
}

esp_err_t i2c_manager_transmit(i2c_manager_device_config_t *device, const uint8_t *data, size_t size, TickType_t timeout_ms) {
    esp_err_t ret                      = ESP_OK;
    i2c_master_dev_handle_t dev_handle = NULL;
    bool device_added                  = false;

    // Get the device handle
    ESP_RETURN_ON_ERROR(i2c_manager_get_device(device, &dev_handle, &device_added), TAG, "Failed to get I2C device");
    I2C_BUS_LOCK(device->bus_index, ESP_FAIL)

    if (device->channel != 0xFF) {
        ESP_RETURN_ON_ERROR(i2c_manager_mux_select(device->bus_index, device->channel), TAG, "Failed to select mux channel");
    }

    // VERBOSE: show the device address and data being transmitted
    if (esp_log_level_get(TAG) >= ESP_LOG_VERBOSE || device->log_level >= ESP_LOG_VERBOSE) {
        char speed_str[16];
        si_format(device->config.scl_speed_hz, "Hz", speed_str, sizeof(speed_str));
        ESP_LOGD(TAG, "Transmitting to I2C device 0x%X on bus %d at %s, data: ", device->config.device_address, device->bus_index,
                 speed_str);
        ESP_LOG_BUFFER_HEXDUMP(TAG, data, size, ESP_LOG_DEBUG);
    }

    // Transmit the data
    if ((ret = i2c_master_transmit(dev_handle, data, size, timeout_ms)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to transmit I2C data: %s", esp_err_to_name(ret));
        ret = ESP_FAIL;
    }

    // Cleanup
    I2C_BUS_UNLOCK(device->bus_index);
    if (device_added) {
        i2c_manager_remove_device(&(i2c_manager_known_device_t){
            .bus_index = device->bus_index, .channel = device->channel, .address = device->config.device_address});
    }

    return ret;
}

esp_err_t i2c_manager_receive(i2c_manager_device_config_t *device, uint8_t *data, size_t size, TickType_t timeout_ms) {
    esp_err_t ret                      = ESP_OK;
    i2c_master_dev_handle_t dev_handle = NULL;
    bool device_added                  = false;

    // Get the device handle
    ESP_RETURN_ON_ERROR(i2c_manager_get_device(device, &dev_handle, &device_added), TAG, "Failed to get I2C device");
    I2C_BUS_LOCK(device->bus_index, ESP_FAIL);

    if (device->channel != 0xFF) {
        ESP_RETURN_ON_ERROR(i2c_manager_mux_select(device->bus_index, device->channel), TAG, "Failed to select mux channel");
    }

    // Receive the data
    if ((ret = i2c_master_receive(dev_handle, data, size, timeout_ms)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to receive I2C data: %s", esp_err_to_name(ret));
        ret = ESP_FAIL;
    }

    // VERBOSE: show the device address and data being received
    if (esp_log_level_get(TAG) >= ESP_LOG_VERBOSE || device->log_level >= ESP_LOG_VERBOSE) {
        char speed_str[16];
        si_format(device->config.scl_speed_hz, "Hz", speed_str, sizeof(speed_str));
        ESP_LOGD(TAG, "Received %d bytes from I2C device 0x%X on bus %d at %s", size, device->config.device_address,
                 device->bus_index, speed_str);
        ESP_LOG_BUFFER_HEXDUMP(TAG, data, size, ESP_LOG_DEBUG);
    }

    // Cleanup
    I2C_BUS_UNLOCK(device->bus_index);
    if (device_added) {
        i2c_manager_remove_device(&(i2c_manager_known_device_t){
            .bus_index = device->bus_index, .channel = device->channel, .address = device->config.device_address});
    }

    return ret;
}

esp_err_t i2c_manager_transmit_receive(i2c_manager_device_config_t *device, uint8_t *tx_data, size_t tx_size, uint8_t *rx_data,
                                       size_t rx_size, TickType_t timeout_ms) {
    esp_err_t ret                      = ESP_OK;
    i2c_master_dev_handle_t dev_handle = NULL;
    bool device_added                  = false;

    // Get the device handle
    ESP_RETURN_ON_ERROR(i2c_manager_get_device(device, &dev_handle, &device_added), TAG, "Failed to get I2C device");
    I2C_BUS_LOCK(device->bus_index, ESP_FAIL);

    if (device->channel != 0xFF) {
        ESP_RETURN_ON_ERROR(i2c_manager_mux_select(device->bus_index, device->channel), TAG, "Failed to select mux channel");
    }

    // VERBOSE: show the device address, speed, and data being transmitted/received
    char speed_str[16];
    if (esp_log_level_get(TAG) >= ESP_LOG_VERBOSE || device->log_level >= ESP_LOG_VERBOSE) {
        si_format(device->config.scl_speed_hz, "Hz", speed_str, sizeof(speed_str));
        ESP_LOGD(TAG, "Transmitting %d bytes to I2C device 0x%X on bus %d at %s", tx_size, device->config.device_address,
                 device->bus_index, speed_str);
        ESP_LOG_BUFFER_HEXDUMP(TAG, tx_data, tx_size, ESP_LOG_DEBUG);
    }

    // Transmit and receive the data
    if ((ret = i2c_master_transmit_receive(dev_handle, tx_data, tx_size, rx_data, rx_size, timeout_ms)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to transmit/receive I2C data: %s", esp_err_to_name(ret));
        ret = ESP_FAIL;
    }

    // VERBOSE: show the data received
    if (esp_log_level_get(TAG) >= ESP_LOG_VERBOSE || device->log_level >= ESP_LOG_VERBOSE) {
        ESP_LOGD(TAG, "Received %d bytes from I2C device 0x%X on bus %d at %s", rx_size, device->config.device_address,
                 device->bus_index, speed_str);
        ESP_LOG_BUFFER_HEXDUMP(TAG, rx_data, rx_size, ESP_LOG_DEBUG);
    }

    // Cleanup
    I2C_BUS_UNLOCK(device->bus_index);
    if (device_added) {
        i2c_manager_remove_device(&(i2c_manager_known_device_t){
            .bus_index = device->bus_index, .channel = device->channel, .address = device->config.device_address});
    }

    return ret;
}

esp_err_t i2c_manager_read_eeprom(i2c_manager_device_config_t *device, uint32_t address, uint8_t *data, size_t size) {
    // Write a single byte to the EEPROM specifying the address to read from and then read the data
    uint8_t addr_buffer = address & 0xFF;
    ESP_RETURN_ON_ERROR(i2c_manager_transmit_receive(device, &addr_buffer, sizeof(addr_buffer), data, size, 100), TAG,
                        "Failed to read EEPROM data");

    return ESP_OK;
}
