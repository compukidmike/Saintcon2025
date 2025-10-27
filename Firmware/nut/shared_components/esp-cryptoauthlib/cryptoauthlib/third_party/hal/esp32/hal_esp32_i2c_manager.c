#include "atca_hal.h"
#include "i2c_manager.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "cryptoauthlib_hal";

#define HAL_I2C_MAX_TRANSFER 1 + 256 // Maximum transfer size for I2C (address + payload)

static bool i2c_reserved_addresses_allowed = false;

ATCA_STATUS hal_i2c_init(ATCAIface iface, ATCAIfaceCfg *cfg) {
    static i2c_manager_device_config_t dev_cfg = {0};
    static i2c_master_dev_handle_t dev_handle  = NULL;
    if (dev_handle == NULL) {
        if (cfg == NULL || cfg->iface_type != ATCA_I2C_IFACE) {
            ESP_LOGE(TAG, "Invalid ATCAIfaceCfg or iface type");
            return ATCA_BAD_PARAM;
        }

        // Initialize the device configuration
        dev_cfg.bus_index                      = cfg->atcai2c.bus;
        dev_cfg.config.dev_addr_length         = I2C_ADDR_BIT_LEN_7;        // 7-bit address
        dev_cfg.config.device_address          = cfg->atcai2c.address >> 1; // Convert to 7-bit address
        dev_cfg.config.scl_speed_hz            = cfg->atcai2c.baud;         // Set the SCL speed
        dev_cfg.config.scl_wait_us             = 0;                         // Use default wait time
        dev_cfg.config.flags.disable_ack_check = 1; // Disable ACK check by default to allow polling without flooding the serial
                                                    // console with errors

        esp_err_t err = i2c_manager_upsert_device(&dev_cfg, &dev_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "i2c_manager_upsert_device failed: %s", esp_err_to_name(err));
            return ATCA_COMM_FAIL;
        }

        // Store the device configuration in the iface now that we've successfully created it
        iface->hal_data = &dev_cfg;
    }
    return ATCA_SUCCESS;
}

ATCA_STATUS hal_i2c_post_init(ATCAIface iface) {
    // Allow reserved I2C addresses since the calib_wakeup_i2c uses a hack of setting the address to 0x00
    i2c_reserved_addresses_allowed = i2c_manager_reserved_allowed();
    i2c_manager_allow_reserved(true);
    return ATCA_SUCCESS;
}

ATCA_STATUS hal_i2c_send(ATCAIface iface, uint8_t address, uint8_t *txdata, int txlength) {
    size_t total = (size_t)txlength + 1;
    if (total > HAL_I2C_MAX_TRANSFER) {
        ESP_LOGE(TAG, "hal_i2c_send: transfer size %u too large", total);
        return ATCA_COMM_FAIL;
    }

    // Get both the ATCAIfaceCfg and the i2c_manager_device_config_t from the iface
    ATCAIfaceCfg *atca_cfg = atgetifacecfg(iface);
    if (atca_cfg == NULL) {
        ESP_LOGE(TAG, "hal_i2c_send: ATCAIfaceCfg is NULL");
        return ATCA_COMM_FAIL;
    }
    i2c_manager_device_config_t *hal_cfg = (i2c_manager_device_config_t *)iface->hal_data;
    if (hal_cfg == NULL) {
        ESP_LOGE(TAG, "hal_i2c_send: iface->hal_data is NULL");
        return ATCA_COMM_FAIL;
    }

    // Build the device configuration for i2c_manager
    i2c_manager_device_config_t dev_cfg = *hal_cfg;
    dev_cfg.config.device_address       = atca_cfg->atcai2c.address >> 1;
    dev_cfg.config.scl_speed_hz         = hal_cfg->config.scl_speed_hz;
    if (atca_cfg->atcai2c.address == 0x00) {
        dev_cfg.config.flags.disable_ack_check = 1; // Disable ACK check for address 0x00
    }

    // Send the address/command byte followed by any given data
    uint8_t buf[HAL_I2C_MAX_TRANSFER];
    buf[0] = address;
    if (txdata && txlength > 0) {
        memcpy(&buf[1], txdata, txlength);
    }
    esp_err_t err = i2c_manager_transmit(&dev_cfg, buf, total, pdMS_TO_TICKS(1000));
    return (err == ESP_OK) ? ATCA_SUCCESS : ATCA_COMM_FAIL;
}

ATCA_STATUS hal_i2c_receive(ATCAIface iface, uint8_t address, uint8_t *rxdata, uint16_t *rxlength) {
    // Get both the ATCAIfaceCfg and the i2c_manager_device_config_t from the iface
    ATCAIfaceCfg *atca_cfg = atgetifacecfg(iface);
    if (atca_cfg == NULL) {
        ESP_LOGE(TAG, "hal_i2c_receive: ATCAIfaceCfg is NULL");
        return ATCA_COMM_FAIL;
    }
    i2c_manager_device_config_t *hal_cfg = (i2c_manager_device_config_t *)iface->hal_data;
    if (hal_cfg == NULL) {
        ESP_LOGE(TAG, "hal_i2c_receive: iface->hal_data is NULL");
        return ATCA_COMM_FAIL;
    }

    // Build the device configuration for i2c_manager
    i2c_manager_device_config_t dev_cfg = *hal_cfg;
    dev_cfg.config.device_address       = address >> 1;
    dev_cfg.config.scl_speed_hz         = hal_cfg->config.scl_speed_hz;

    // Receive data from the device
    esp_err_t err = i2c_manager_receive(&dev_cfg, rxdata, *rxlength, pdMS_TO_TICKS(1000));
    if (err == ESP_OK) {
        return ATCA_SUCCESS;
    } else {
        ESP_LOGE(TAG, "hal_i2c_receive: i2c_manager_receive failed: %s", esp_err_to_name(err));
        return ATCA_COMM_FAIL;
    }
}

ATCA_STATUS hal_i2c_change_baud(ATCAIface iface, uint32_t baud) {
    // In hal_i2c_(send|receive) we will build a device configuration based on a mix of the iface
    // config and the i2c_manager_device_config_t* stored in iface->hal_data so we can just update
    // the latter to track it
    i2c_manager_device_config_t *cfg = (i2c_manager_device_config_t *)iface->hal_data;
    if (cfg) {
        cfg->config.scl_speed_hz = baud;
        return ATCA_SUCCESS;
    }

    return ATCA_BAD_PARAM;
}

ATCA_STATUS hal_i2c_release(void *hal_data) {
    // Reset the I2C reserved addresses allowance
    i2c_manager_allow_reserved(i2c_reserved_addresses_allowed);
    return ATCA_SUCCESS;
}

ATCA_STATUS hal_i2c_control(ATCAIface iface, uint8_t option, void *param, size_t paramlen) {
    if (iface && iface->mIfaceCFG) {
        if (ATCA_HAL_CHANGE_BAUD == option) {
            return hal_i2c_change_baud(iface, *(uint32_t *)param);
        } else {
            return ATCA_UNIMPLEMENTED;
        }
    }
    return ATCA_BAD_PARAM;
}