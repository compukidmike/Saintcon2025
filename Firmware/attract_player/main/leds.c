#include "leds.h"

#include <math.h>
#include <string.h>

#include "esp_log.h"
#include "led_strip.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *TAG = "leds";

#define LED_GPIO 3

static led_strip_handle_t s_strip = NULL;

/* Attract scene windows (ms) — matches preview/make_attract.py */
enum {
    SC_SPLASH_END  = 5000,
    SC_MENU_END    = 11000,
    SC_FACTION_END = 25000,
    SC_MAP_END     = 30000,
    SC_NODES_END   = 48000,
    SC_WRENCH_END  = 56000,
    SC_LOOP_MS     = 60000,
};

static const uint32_t faction_colors[6] = {
    0xDA3832, 0x00AAE9, 0x00A359, 0xEA983E, 0x862F8B, 0xFFF34A,
};

/** HSV → RGB, h in [0,360), s/v in [0,1] */
static void hsv_to_rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b) {
    while (h < 0) {
        h += 360.0f;
    }
    while (h >= 360.0f) {
        h -= 360.0f;
    }
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float rf, gf, bf;
    if (h < 60) {
        rf = c;
        gf = x;
        bf = 0;
    } else if (h < 120) {
        rf = x;
        gf = c;
        bf = 0;
    } else if (h < 180) {
        rf = 0;
        gf = c;
        bf = x;
    } else if (h < 240) {
        rf = 0;
        gf = x;
        bf = c;
    } else if (h < 300) {
        rf = x;
        gf = 0;
        bf = c;
    } else {
        rf = c;
        gf = 0;
        bf = x;
    }
    *r = (uint8_t)((rf + m) * 255.0f);
    *g = (uint8_t)((gf + m) * 255.0f);
    *b = (uint8_t)((bf + m) * 255.0f);
}

/** Smooth shifting color wash — hue drifts; each LED is slightly phase-offset */
static void effect_color_shift(uint32_t local_ms, float speed_deg_per_s, uint8_t brightness) {
    float base = fmodf(local_ms * 0.001f * speed_deg_per_s, 360.0f);
    for (int i = 0; i < LED_COUNT; i++) {
        float h = base + i * (360.0f / LED_COUNT);
        uint8_t r, g, b;
        hsv_to_rgb(h, 1.0f, 1.0f, &r, &g, &b);
        leds_set(i, r, g, b, brightness);
    }
}

esp_err_t leds_init(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num   = LED_GPIO,
        .max_leds         = LED_COUNT,
        .led_model        = LED_MODEL_WS2812,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src        = RMT_CLK_SRC_DEFAULT,
        .resolution_hz  = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip));
    leds_clear();
    leds_show();
    ESP_LOGI(TAG, "WS2812 x%d on GPIO %d", LED_COUNT, LED_GPIO);
    return ESP_OK;
}

void leds_clear(void) {
    if (!s_strip) {
        return;
    }
    led_strip_clear(s_strip);
}

void leds_set(int index, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    if (!s_strip || index < 0 || index >= LED_COUNT) {
        return;
    }
    /* Global 30% cut → keep 70% */
    brightness = (uint8_t)((brightness * 70) / 100);
    uint8_t sr = (uint8_t)((r * brightness) / 255);
    uint8_t sg = (uint8_t)((g * brightness) / 255);
    uint8_t sb = (uint8_t)((b * brightness) / 255);
    led_strip_set_pixel(s_strip, index, sr, sg, sb);
}

void leds_show(void) {
    if (s_strip) {
        led_strip_refresh(s_strip);
    }
}

void leds_sync_attract(uint32_t local_ms) {
    uint32_t t = local_ms % SC_LOOP_MS;
    float tf   = t * 0.001f;
    const int n = LED_COUNT;

    if (t < SC_SPLASH_END) {
        /* Soft rainbow wash instead of flat white */
        float breath = 0.45f + 0.55f * (0.5f + 0.5f * cosf(tf * 2.2f));
        effect_color_shift(t, 40.0f, (uint8_t)(50 + 70 * breath));
    } else if (t < SC_MENU_END) {
        uint32_t local = t - SC_SPLASH_END;
        int head       = (local / 40) % n;
        for (int i = 0; i < n; i++) {
            int d = (i - head + n) % n;
            if (d < 4) {
                leds_set(i, 0, 180, 220, (uint8_t)(100 - d * 22));
            } else {
                leds_set(i, 0, 0, 0, 0);
            }
        }
    } else if (t < SC_FACTION_END) {
        uint32_t local = t - SC_MENU_END;
        int fac        = (local / 2200) % 6;
        uint32_t c     = faction_colors[fac];
        uint8_t r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
        int head = (local / 30) % n;
        for (int i = 0; i < n; i++) {
            int d = (i - head + n) % n;
            if (d < 8) {
                float fall = 1.0f - d / 8.0f;
                leds_set(i, r, g, b, (uint8_t)(20 + 100 * fall));
            } else {
                leds_set(i, 0, 0, 0, 0);
            }
        }
    } else if (t < SC_MAP_END) {
        uint32_t local = t - SC_FACTION_END;
        int head       = (local / 50) % n;
        for (int i = 0; i < n; i++) {
            int d = abs(i - head);
            if (d > n / 2) {
                d = n - d;
            }
            if (d < 3) {
                leds_set(i, 40, 220, 120, (uint8_t)(90 - d * 25));
            } else {
                leds_set(i, 0, 0, 0, 0);
            }
        }
    } else if (t < SC_NODES_END) {
        uint32_t local = t - SC_MAP_END;
        float progress = local / (float)(SC_NODES_END - SC_MAP_END);
        if (progress > 1.0f) {
            progress = 1.0f;
        }
        int lit = (int)(progress * n + 0.5f);
        for (int i = 0; i < n; i++) {
            if (i < lit) {
                float pulse = 0.7f + 0.3f * sinf(tf * 6.0f + i);
                leds_set(i, 0, 255, 80, (uint8_t)(40 + 70 * pulse));
            } else {
                leds_set(i, 0, 0, 0, 0);
            }
        }
    } else if (t < SC_WRENCH_END) {
        uint32_t local = t - SC_NODES_END;
        int head       = (int)(local * 0.012f) % n;
        for (int i = 0; i < n; i++) {
            int d = (i - head + n) % n;
            if (d < 6) {
                float fall = 1.0f - d / 6.0f;
                leds_set(i, 255, 180, 40, (uint8_t)(25 + 95 * fall));
            } else {
                leds_set(i, 0, 0, 0, 0);
            }
        }
    } else {
        /* Finale: color-shift wash instead of white/gold flash */
        uint32_t local = t - SC_WRENCH_END;
        effect_color_shift(local, 90.0f, 110);
    }
    leds_show();
}
