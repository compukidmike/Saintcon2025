/** \file
    ---------------------------------------------------------------
    SPDX-License-Identifier: BSD-3-Clause

    Copyright (c) 2024, Renesas Electronics Corporation and/or its affiliates


    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

    3. Neither the name of Renesas nor the names of its
       contributors may be used to endorse or promote products derived from this
       software without specific prior written permission.



   THIS SOFTWARE IS PROVIDED BY Renesas "AS IS" AND ANY EXPRESS
   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
   OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL RENESAS OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    ---------------------------------------------------------------

    Project     : PTX105R Arduino
    Module      : SPI
    File        : PtxPlatSPI.cpp

    Description : SPI interface implementation
*/

#include "PtxPlatSPI.h"

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#include <algorithm>
#include <array>

#include "ptxPLAT_INT.h"

namespace {
uint8_t *spiBuffer                      = nullptr; // DMA-capable buffer allocated at init
static constexpr size_t SPI_BUFFER_SIZE = 4096;    // must be <= max_transfer_sz used when initializing SPI bus
ptxPLAT_GPIO_t gpioCtx;
static constexpr gpio_num_t pinIRQ = GPIO_NUM_3;
static constexpr gpio_num_t pinNSS = GPIO_NUM_4;
static constexpr char TAG[]        = "NFC-SPI";

static spi_device_handle_t spi_device = nullptr;
static bool spi_initialized           = false;
} // namespace

ptxStatus_t ptxPlat_spiInit(ptxPLAT_GPIO_t **gpio) {
    if (gpio != nullptr) {
        memset(&gpioCtx, 0, sizeof(gpioCtx));
        gpioCtx.pinIRQ = pinIRQ;
        gpioCtx.pinNSS = pinNSS;
        *gpio          = &gpioCtx;

        // Configure GPIO pins using ESP-IDF native functions
        gpio_config_t io_conf = {};

        // Configure IRQ pin (input with pullup)
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << pinIRQ);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);

        // Configure NSS pin (output, initially high)
        io_conf.intr_type    = GPIO_INTR_DISABLE;
        io_conf.mode         = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << pinNSS);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
        gpio_set_level(pinNSS, 1);

        if (!spi_initialized) {
            // Configure SPI device on existing SPI2 bus (already initialized by ethernet)
            spi_device_interface_config_t dev_config = {};
            dev_config.clock_speed_hz                = 10000000; // 10MHz NFC SPI clock
            dev_config.mode                          = 0;        // SPI_MODE0 (CPOL=0, CPHA=0)
            dev_config.spics_io_num                  = -1;       // Manual CS control
            dev_config.queue_size                    = 1;
            dev_config.flags                         = 0; // Full duplex mode
            dev_config.command_bits                  = 0;
            dev_config.address_bits                  = 0;
            dev_config.dummy_bits                    = 0;
            dev_config.cs_ena_pretrans               = 0; // No CS setup time
            dev_config.cs_ena_posttrans              = 0; // No CS hold time

            esp_err_t ret = spi_bus_add_device(SPI2_HOST, &dev_config, &spi_device);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to add NFC SPI device: %s", esp_err_to_name(ret));
                return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InternalError);
            }

            spi_initialized = true;
            ESP_LOGI(TAG, "NFC SPI device added to existing SPI2 bus");

            // Allocate a DMA-capable buffer for SPI transfers to avoid the SPI
            // driver allocating DMA buffers at runtime. Use heap_caps_malloc to
            // ensure MALLOC_CAP_DMA.
            spiBuffer = (uint8_t *)heap_caps_malloc(SPI_BUFFER_SIZE, MALLOC_CAP_DMA);
            if (spiBuffer == nullptr) {
                ESP_LOGW(TAG, "Failed to allocate DMA-capable spiBuffer (%u bytes)", (unsigned)SPI_BUFFER_SIZE);
            } else {
                memset(spiBuffer, 0, SPI_BUFFER_SIZE);
                ESP_LOGI(TAG, "Allocated DMA-capable spiBuffer (%u bytes)", (unsigned)SPI_BUFFER_SIZE);
            }

            // Power-on reset sequence for NFC chip
            // Hold CS high (inactive) for a longer period
            gpio_set_level(pinNSS, 1);
            vTaskDelay(pdMS_TO_TICKS(1000)); // 1000ms power-on delay for stable initialization

            ESP_LOGI(TAG, "NFC chip power-on reset sequence completed");
        }

        return ptxStatus_Success;
    } else {
        return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }
}

ptxStatus_t ptxPlat_spiDeinit(ptxPLAT_GPIO_t *gpio) {
    if (gpio != nullptr) {
        gpio_set_level(pinNSS, 0);

        if (spi_initialized && spi_device != nullptr) {
            spi_bus_remove_device(spi_device);
            spi_device      = nullptr;
            spi_initialized = false;
            ESP_LOGI(TAG, "NFC SPI device removed");
        }

        if (spiBuffer != nullptr) {
            heap_caps_free(spiBuffer);
            spiBuffer = nullptr;
            ESP_LOGI(TAG, "Freed DMA-capable spiBuffer");
        }

        memset(&gpioCtx, 0, sizeof(gpioCtx));
        return ptxStatus_Success;
    } else {
        return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }
}

ptxStatus_t ptxPlat_spiTrx(const ptxPLAT_GPIO_t *gpio, const uint8_t *txBuf[], size_t txLen[], size_t numTxBuffers,
                           uint8_t *rxBuf[], size_t *rxLen[], size_t numRxBuffers) {
    auto status                 = ptxStatus_Success;
    const size_t numBuffers_max = 5;

    // Validate parameters and ensure SPI device initialized. If SPI isn't
    // initialized yet, refuse the transaction so the SPI driver won't attempt
    // to allocate DMA buffers or access hardware.
    if (gpio == nullptr) {
        return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    if (!spi_initialized || spi_device == nullptr) {
        ESP_LOGW(TAG, "SPI device not initialized - transaction refused");
        return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InterfaceError);
    }

    if (!(numTxBuffers < numBuffers_max && numRxBuffers <= numBuffers_max)) {
        return PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    // Assert CS (active low) for the entire transaction
    gpio_set_level(pinNSS, 0);

    // Small delay after CS assertion
    vTaskDelay(pdMS_TO_TICKS(1));

    // Perform TX operations (send data, discard received data)
    if (txBuf != nullptr && txLen != nullptr) {
        for (uint8_t index = 0; index < numTxBuffers && status == ptxStatus_Success; index++) {
            if (txBuf[index] != nullptr && txLen[index] > 0) {
                // ESP_LOGD(TAG, "TX[%d]: len=%zu, data=0x%02x...", index, txLen[index], txBuf[index][0]);

                spi_transaction_t trans = {};
                trans.length            = txLen[index] * 8; // Length in bits
                // Copy TX data into DMA-capable static buffer to avoid driver allocating DMA buffers
                if (spiBuffer != nullptr && txLen[index] <= SPI_BUFFER_SIZE) {
                    memcpy(spiBuffer, txBuf[index], txLen[index]);
                    trans.tx_buffer = spiBuffer;
                    // Provide a DMA-capable RX buffer but we will ignore contents for TX-only phases
                    trans.rx_buffer = (void *)spiBuffer;
                } else {
                    // Fallback: reference original buffer - driver may allocate DMA buffer
                    if (spiBuffer == nullptr) {
                        ESP_LOGW(TAG, "No DMA spiBuffer available; driver may allocate DMA buffers");
                    } else {
                        ESP_LOGW(TAG, "TX len %zu exceeds spiBuffer (%u); driver may allocate DMA buffers", txLen[index],
                                 (unsigned)SPI_BUFFER_SIZE);
                    }
                    trans.tx_buffer = (void *)txBuf[index];
                    // For TX-only fallback, prefer using our DMA buffer for rx_buffer;
                    // if not available, leave rx_buffer NULL (driver may allocate internal buffer).
                    if (spiBuffer != nullptr) {
                        trans.rx_buffer = (void *)spiBuffer;
                    } else {
                        trans.rx_buffer = NULL;
                    }
                }

                esp_err_t ret = spi_device_transmit(spi_device, &trans);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "SPI TX failed: %s", esp_err_to_name(ret));
                    status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InternalError);
                } else {
                    // ESP_LOGD(TAG, "TX[%d]: success", index);
                }
            } else {
                status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
            }
        }
    }

    // Perform RX operations (send zeros, capture received data)
    if (status == ptxStatus_Success && rxBuf != nullptr && rxLen != nullptr) {
        for (uint8_t index = 0; index < numRxBuffers && status == ptxStatus_Success; index++) {
            if (rxBuf[index] != nullptr && rxLen[index] != nullptr && *rxLen[index] > 0) {
                // ESP_LOGD(TAG, "RX[%d]: requesting %zu bytes", index, *rxLen[index]);

                spi_transaction_t trans = {};
                trans.length            = (*rxLen[index]) * 8; // Length in bits

                // Use DMA-capable static buffer as TX (zeros) and RX target; after the
                // transaction we'll copy the data out to the caller buffer. This avoids
                // the SPI driver allocating a separate DMA buffer for each call.
                if (spiBuffer != nullptr && *rxLen[index] <= SPI_BUFFER_SIZE) {
                    memset(spiBuffer, 0, *rxLen[index]);
                    trans.tx_buffer = spiBuffer;
                    trans.rx_buffer = (void *)spiBuffer;

                    esp_err_t ret = spi_device_transmit(spi_device, &trans);
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "SPI RX failed: %s", esp_err_to_name(ret));
                        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InternalError);
                    } else {
                        // Copy out to caller buffer
                        memcpy(rxBuf[index], spiBuffer, *rxLen[index]);
                    }
                } else {
                    // Fallback: requested length exceeds our buffer or no DMA buffer available,
                    // let driver handle (may allocate DMA buffers internally)
                    if (spiBuffer == nullptr) {
                        ESP_LOGW(TAG, "No DMA spiBuffer available; RX request of %zu may cause driver DMA allocation",
                                 *rxLen[index]);
                    } else {
                        ESP_LOGW(TAG, "RX len %zu exceeds spiBuffer (%u); driver may allocate DMA buffers", *rxLen[index],
                                 (unsigned)SPI_BUFFER_SIZE);
                    }
                    if (spiBuffer != nullptr) {
                        memset(spiBuffer, 0, SPI_BUFFER_SIZE);
                    }
                    trans.tx_buffer = (void *)(spiBuffer != nullptr ? spiBuffer : NULL);
                    trans.rx_buffer = rxBuf[index];

                    esp_err_t ret = spi_device_transmit(spi_device, &trans);
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "SPI RX failed: %s", esp_err_to_name(ret));
                        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InternalError);
                    }
                }
            } else {
                status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
            }
        }
    }

    // Deassert CS (inactive high) and add some delay
    gpio_set_level(pinNSS, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    return status;
}