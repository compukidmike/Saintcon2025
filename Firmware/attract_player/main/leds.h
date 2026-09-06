#pragma once

#include "esp_err.h"
#include <stdint.h>

#define LED_COUNT 20

esp_err_t leds_init(void);
void leds_clear(void);
void leds_set(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);
void leds_show(void);

/** Drive ring effects from attract timeline (ms into loop). */
void leds_sync_attract(uint32_t local_ms);
