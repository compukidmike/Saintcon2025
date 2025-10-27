/*
 * Copyright (c) 2018 Ruslan V. Uss <unclerus@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of itscontributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file mcp23x17.c
 *
 * ESP-IDF driver for I2C/SPI 16 bit GPIO expanders MCP23017/MCP23S17
 *
 * Copyright (c) 2018 Ruslan V. Uss <unclerus@gmail.com>
 *
 * BSD Licensed as described in the file LICENSE
 */

#include <esp_log.h>
#include <string.h>
// #include <esp_idf_lib_helpers.h>
#include "mcp23x17.h"
#include "i2c_manager.h"

static const char *TAG = "mcp23x17";

#define I2C_FREQ_HZ 1000000 // Max 1MHz for esp-idf, but device supports up to 1.7Mhz

#define REG_IODIRA   0x00
#define REG_IODIRB   0x01
#define REG_IPOLA    0x02
#define REG_IPOLB    0x03
#define REG_GPINTENA 0x04
#define REG_GPINTENB 0x05
#define REG_DEFVALA  0x06
#define REG_DEFVALB  0x07
#define REG_INTCONA  0x08
#define REG_INTCONB  0x09
#define REG_IOCON    0x0A
#define REG_GPPUA    0x0C
#define REG_GPPUB    0x0D
#define REG_INTFA    0x0E
#define REG_INTFB    0x0F
#define REG_INTCAPA  0x10
#define REG_INTCAPB  0x11
#define REG_GPIOA    0x12
#define REG_GPIOB    0x13
#define REG_OLATA    0x14
#define REG_OLATB    0x15

#define BIT_IOCON_INTPOL 1
#define BIT_IOCON_ODR    2
#define BIT_IOCON_HAEN   3
#define BIT_IOCON_DISSLW 4
#define BIT_IOCON_SEQOP  5
#define BIT_IOCON_MIRROR 6
#define BIT_IOCON_BANK   7

#define CHECK(x)                \
    do {                        \
        esp_err_t __;           \
        if ((__ = x) != ESP_OK) \
            return __;          \
    } while (0)
#define CHECK_ARG(VAL)                  \
    do {                                \
        if (!(VAL))                     \
            return ESP_ERR_INVALID_ARG; \
    } while (0)
#define BV(x) (1 << (x))

static esp_err_t read_reg_16(mcp23x17_t *dev, uint8_t reg, uint16_t *val) {
    uint8_t txbuf[1] = {reg};
    uint8_t rxbuf[2];
    esp_err_t err = i2c_manager_transmit_receive(dev, txbuf, 1, rxbuf, 2, pdMS_TO_TICKS(1000));
    *val          = rxbuf[1] << 8 | rxbuf[0];

    // CHECK_ARG(dev && val);
    // I2C_DEV_TAKE_MUTEX(dev);
    // I2C_DEV_CHECK(dev, i2c_dev_read_reg(dev, reg, val, 2));
    // I2C_DEV_GIVE_MUTEX(dev);

    return ESP_OK;
}

static esp_err_t write_reg_16(mcp23x17_t *dev, uint8_t reg, uint16_t val) {
    uint8_t buf[3] = {reg, val & 0xFF, (val >> 8) & 0xFF};
    esp_err_t err  = i2c_manager_transmit(dev, buf, 3, pdMS_TO_TICKS(1000));

    // CHECK_ARG(dev);
    //  I2C_DEV_TAKE_MUTEX(dev);
    //  I2C_DEV_CHECK(dev, i2c_dev_write_reg(dev, reg, &val, 2));
    //  I2C_DEV_GIVE_MUTEX(dev);

    return ESP_OK;
}

static esp_err_t write_reg_bit_16(mcp23x17_t *dev, uint8_t reg, bool val, uint8_t bit) {
    // CHECK_ARG(dev);
    uint8_t txbuf[1] = {reg};
    uint8_t rxbuf[2];
    esp_err_t err = i2c_manager_transmit_receive(dev, txbuf, 1, rxbuf, 2, pdMS_TO_TICKS(1000));

    uint16_t buf;
    buf = rxbuf[1] << 8 | rxbuf[0];
    buf = (buf & ~BV(bit)) | (val ? BV(bit) : 0);
    err = write_reg_16(dev, reg, buf);
    // I2C_DEV_TAKE_MUTEX(dev);
    // I2C_DEV_CHECK(dev, i2c_dev_read_reg(dev, reg, &buf, 2));
    // buf = (buf & ~BV(bit)) | (val ? BV(bit) : 0);
    // I2C_DEV_CHECK(dev, i2c_dev_write_reg(dev, reg, &buf, 2));
    // I2C_DEV_GIVE_MUTEX(dev);

    return ESP_OK;
}

static esp_err_t read_reg_bit_8(mcp23x17_t *dev, uint8_t reg, bool *val, uint8_t bit) {
    uint8_t txbuf[1] = {reg};
    uint8_t rxbuf[1];
    esp_err_t err = i2c_manager_transmit_receive(dev, txbuf, 1, rxbuf, 1, pdMS_TO_TICKS(1000));

    // CHECK_ARG(dev && val);
    // uint8_t buf;
    // I2C_DEV_TAKE_MUTEX(dev);
    // I2C_DEV_CHECK(dev, i2c_dev_read_reg(dev, reg, &buf, 1));
    // I2C_DEV_GIVE_MUTEX(dev);

    *val = (rxbuf[0] & BV(bit)) >> bit;

    return ESP_OK;
}

static esp_err_t write_reg_bit_8(mcp23x17_t *dev, uint8_t reg, bool val, uint8_t bit) {
    uint8_t txbuf[2] = {reg, 0};
    uint8_t rxbuf[1];
    esp_err_t err = i2c_manager_transmit_receive(dev, txbuf, 1, rxbuf, 1, pdMS_TO_TICKS(1000));

    txbuf[1] = (rxbuf[0] & ~BV(bit)) | (val ? BV(bit) : 0);
    err      = i2c_manager_transmit(dev, txbuf, 2, pdMS_TO_TICKS(1000));

    // CHECK_ARG(dev);
    // uint8_t buf;
    // I2C_DEV_TAKE_MUTEX(dev);
    // I2C_DEV_CHECK(dev, i2c_dev_read_reg(dev, reg, &buf, 1));
    // buf = (buf & ~BV(bit)) | (val ? BV(bit) : 0);
    // I2C_DEV_CHECK(dev, i2c_dev_write_reg(dev, reg, &buf, 1));
    // I2C_DEV_GIVE_MUTEX(dev);
    return ESP_OK;
}

static esp_err_t read_reg_bit_16(mcp23x17_t *dev, uint8_t reg, bool *val, uint8_t bit) {
    uint16_t buf;

    // CHECK(read_reg_16(dev, reg, &buf));

    uint8_t txbuf[1] = {reg};
    uint8_t rxbuf[2];
    esp_err_t err = i2c_manager_transmit_receive(dev, txbuf, 1, rxbuf, 2, pdMS_TO_TICKS(1000));
    buf           = rxbuf[1] << 8 | rxbuf[0];

    *val = (buf & BV(bit)) >> bit;

    return ESP_OK;
}

///////////////////////////////////////////////////////////////////////////////

esp_err_t mcp23x17_init_desc(mcp23x17_t *dev, uint8_t addr, i2c_port_t port, mcp23x17_handle_t *handle) {

    // if (handle == NULL) {
    //  Initialize the device configuration
    dev->bus_index                      = port;
    dev->config.dev_addr_length         = I2C_ADDR_BIT_LEN_7; // 7-bit address
    dev->config.device_address          = addr;               // Convert to 7-bit address
    dev->config.scl_speed_hz            = 100000;             // Set the SCL speed
    dev->config.scl_wait_us             = 0;                  // Use default wait time
    dev->config.flags.disable_ack_check = 0; // Disable ACK check by default to allow polling without flooding the serial console
                                             // with errors

    // CHECK_ARG(dev);
    if (addr < MCP23X17_ADDR_BASE || addr > MCP23X17_ADDR_BASE + 7) {
        ESP_LOGE(TAG, "Invalid device address: 0x%02x", addr);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = i2c_manager_upsert_device(dev, handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_manager_upsert_device failed: %s", esp_err_to_name(err));
    }
    return err;
    // }
    // return ESP_ERR_INVALID_ARG;
}

esp_err_t mcp23x17_free_desc(mcp23x17_t *dev) {
    CHECK_ARG(dev);

    // return i2c_dev_delete_mutex(dev);
    return ESP_OK;
}

esp_err_t mcp23x17_get_int_out_mode(mcp23x17_t *dev, mcp23x17_int_out_mode_t *mode) {
    CHECK_ARG(mode);

    bool buf;
    CHECK(read_reg_bit_8(dev, REG_IOCON, &buf, BIT_IOCON_ODR));
    if (buf) {
        *mode = MCP23X17_OPEN_DRAIN;
        return ESP_OK;
    }
    CHECK(read_reg_bit_8(dev, REG_IOCON, &buf, BIT_IOCON_INTPOL));
    *mode = buf ? MCP23X17_ACTIVE_HIGH : MCP23X17_ACTIVE_LOW;

    return ESP_OK;
}

esp_err_t mcp23x17_set_int_out_mode(mcp23x17_t *dev, mcp23x17_int_out_mode_t mode) {
    if (mode == MCP23X17_OPEN_DRAIN) {
        return write_reg_bit_8(dev, REG_IOCON, true, BIT_IOCON_ODR);
    }

    // The INTPOL bit is only functional if the ODR bit is cleared.
    write_reg_bit_8(dev, REG_IOCON, false, BIT_IOCON_ODR);
    return write_reg_bit_8(dev, REG_IOCON, mode == MCP23X17_ACTIVE_HIGH, BIT_IOCON_INTPOL);
}

esp_err_t mcp23x17_port_get_mode(mcp23x17_t *dev, uint16_t *val) {
    return read_reg_16(dev, REG_IODIRA, val);
}

esp_err_t mcp23x17_port_set_mode(mcp23x17_t *dev, uint16_t val) {
    return write_reg_16(dev, REG_IODIRA, val);
}

esp_err_t mcp23x17_port_get_pullup(mcp23x17_t *dev, uint16_t *val) {
    return read_reg_16(dev, REG_GPPUA, val);
}

esp_err_t mcp23x17_port_set_pullup(mcp23x17_t *dev, uint16_t val) {
    return write_reg_16(dev, REG_GPPUA, val);
}

esp_err_t mcp23x17_port_read(mcp23x17_t *dev, uint16_t *val) {
    return read_reg_16(dev, REG_GPIOA, val);
}

esp_err_t mcp23x17_port_write(mcp23x17_t *dev, uint16_t val) {
    return write_reg_16(dev, REG_GPIOA, val);
}

esp_err_t mcp23x17_get_mode(mcp23x17_t *dev, uint8_t pin, mcp23x17_gpio_mode_t *mode) {
    CHECK_ARG(mode);

    bool buf;
    CHECK(read_reg_bit_16(dev, REG_IODIRA, &buf, pin));
    *mode = buf ? MCP23X17_GPIO_INPUT : MCP23X17_GPIO_OUTPUT;

    return ESP_OK;
}

esp_err_t mcp23x17_set_mode(mcp23x17_t *dev, uint8_t pin, mcp23x17_gpio_mode_t mode) {
    return write_reg_bit_16(dev, REG_IODIRA, mode, pin);
}

esp_err_t mcp23x17_get_pullup(mcp23x17_t *dev, uint8_t pin, bool *enable) {
    return read_reg_bit_16(dev, REG_GPPUA, enable, pin);
}

esp_err_t mcp23x17_set_pullup(mcp23x17_t *dev, uint8_t pin, bool enable) {
    return write_reg_bit_16(dev, REG_GPPUA, enable, pin);
}

esp_err_t mcp23x17_get_level(mcp23x17_t *dev, uint8_t pin, bool *val) {
    CHECK_ARG(val);

    bool buf;
    CHECK(read_reg_bit_16(dev, REG_GPIOA, &buf, pin));
    *val = buf ? 1 : 0;

    return ESP_OK;
}

esp_err_t mcp23x17_set_level(mcp23x17_t *dev, uint8_t pin, bool val) {
    return write_reg_bit_16(dev, REG_GPIOA, val, pin);
}

esp_err_t mcp23x17_port_set_interrupt(mcp23x17_t *dev, uint16_t mask, mcp23x17_gpio_intr_t intr) {
    // CHECK_ARG(dev);

    // uint16_t int_en;
    // CHECK(read_reg_16(dev, REG_GPINTENA, &int_en));

    // if (intr == MCP23X17_INT_DISABLED)
    // {
    //     // disable interrupts
    //     int_en &= ~mask;
    //     CHECK(write_reg_16(dev, REG_GPINTENA, int_en));

    //     return ESP_OK;
    // }

    // uint16_t int_con;
    // CHECK(read_reg_16(dev, REG_INTCONA, &int_con));

    // if (intr == MCP23X17_INT_ANY_EDGE)
    //     int_con &= ~mask;
    // else
    // {
    //     int_con |= mask;

    //     uint16_t int_def;
    //     CHECK(read_reg_16(dev, REG_DEFVALA, &int_def));
    //     if (intr == MCP23X17_INT_LOW_EDGE)
    //         int_def |= mask;
    //     else
    //         int_def &= ~mask;
    //     CHECK(write_reg_16(dev, REG_DEFVALA, int_def));
    // }

    // CHECK(write_reg_16(dev, REG_INTCONA, int_con));

    // // enable interrupts
    // int_en |= mask;
    // CHECK(write_reg_16(dev, REG_GPINTENA, int_en));

    return ESP_OK;
}

esp_err_t mcp23x17_set_interrupt(mcp23x17_t *dev, uint8_t pin, mcp23x17_gpio_intr_t intr) {
    return mcp23x17_port_set_interrupt(dev, BV(pin), intr);
}
