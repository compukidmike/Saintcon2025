#include <math.h>
#include <stdlib.h>
#include "esp_timer.h"

#include "nut/led_patterns.h"
#include "led.h"

// -------------------------------------------------------------------------------------------------
// Flash arbitrary color pattern
// -------------------------------------------------------------------------------------------------
typedef struct {
    uint64_t start_ms;
    uint32_t on_ms;
    uint32_t off_ms;
    int flashes;
    uint8_t r, g, b;
    uint8_t brightness;
} flash_ctx_t;

static flash_ctx_t g_flash_ctx;
static int g_flash_layer = -1;

static void flash_render(uint32_t now_ms, void *user_ctx) {
    (void)user_ctx;
    uint32_t elapsed   = (uint32_t)(now_ms - g_flash_ctx.start_ms);
    uint32_t period_ms = g_flash_ctx.on_ms + g_flash_ctx.off_ms;
    int flash_index    = elapsed / period_ms;
    if (flash_index >= g_flash_ctx.flashes) {
        return;
    }
    uint32_t in_cycle = elapsed % period_ms;
    bool on           = in_cycle < g_flash_ctx.on_ms;
    if (on) {
        for (int i = 0; i < NUM_LEDS; i++) {
            led_set_with_brightness(i, g_flash_ctx.r, g_flash_ctx.g, g_flash_ctx.b, g_flash_ctx.brightness);
        }
    }
}

void flash_leds(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness, int flashes, uint32_t on_ms, uint32_t off_ms) {
    if (g_flash_layer >= 0) {
        led_layer_destroy(g_flash_layer);
        g_flash_layer = -1;
    }
    g_flash_ctx.start_ms   = esp_timer_get_time() / 1000ULL;
    g_flash_ctx.on_ms      = on_ms;
    g_flash_ctx.off_ms     = off_ms;
    g_flash_ctx.flashes    = flashes;
    g_flash_ctx.r          = r;
    g_flash_ctx.g          = g;
    g_flash_ctx.b          = b;
    g_flash_ctx.brightness = brightness;
    uint32_t duration      = flashes * (on_ms + off_ms);
    g_flash_layer          = led_layer_create_timed(LED_PRIORITY_HIGH, flash_render, &g_flash_ctx, true, true, duration, true);
}

void led_pattern_flash_red() {
    flash_leds(255, 0, 0, 80, 3, 150, 150);
}

// -------------------------------------------------------------------------------------------------
// Success "sparkle" (random white LEDs with fade in/out)
// -------------------------------------------------------------------------------------------------
typedef struct {
    uint64_t start_ms;
    uint32_t duration_ms;
    uint32_t sparkle_count;
    uint32_t sparkle_period_ms;
    uint8_t brightness;
    uint32_t seed;
} sparkle_ctx_t;

static sparkle_ctx_t g_sparkle_ctx;
static int g_sparkle_layer = -1;

static void sparkle_render(uint32_t now_ms, void *user_ctx) {
    sparkle_ctx_t *ctx = (sparkle_ctx_t *)user_ctx;
    uint32_t elapsed   = (uint32_t)(now_ms - ctx->start_ms);
    if (elapsed >= ctx->duration_ms) {
        return;
    }

    // Each sparkle lasts sparkle_period_ms, fades in/out
    float sparkle_phase        = (float)(elapsed % ctx->sparkle_period_ms) / (float)ctx->sparkle_period_ms;
    float fade                 = 0.5f * (1.0f - cosf(2.0f * M_PI * sparkle_phase)); // cosine fade in/out
    uint8_t sparkle_brightness = (uint8_t)(ctx->brightness * fade);

    // Use a simple LCG for repeatable pseudo-randomness per frame
    uint32_t seed = ctx->seed + elapsed / 16;
    for (int i = 0; i < ctx->sparkle_count; i++) {
        seed    = seed * 1664525 + 1013904223;
        int led = seed % NUM_LEDS;
        led_set_with_brightness(led, 255, 255, 255, sparkle_brightness);
    }
}

void led_pattern_sparkles() {
    if (g_sparkle_layer >= 0) {
        led_layer_destroy(g_sparkle_layer);
        g_sparkle_layer = -1;
    }
    g_sparkle_ctx.start_ms          = esp_timer_get_time() / 1000ULL;
    g_sparkle_ctx.duration_ms       = 1000; // 1 second
    g_sparkle_ctx.sparkle_count     = 6;    // number of sparkles per frame
    g_sparkle_ctx.sparkle_period_ms = 180;  // ms per sparkle fade
    g_sparkle_ctx.brightness        = 90;
    g_sparkle_ctx.seed              = (uint32_t)g_sparkle_ctx.start_ms;
    g_sparkle_layer =
        led_layer_create_timed(LED_PRIORITY_HIGH, sparkle_render, &g_sparkle_ctx, true, true, g_sparkle_ctx.duration_ms, true);
}
