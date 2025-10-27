#include <math.h>
#include <stdlib.h>
#include "esp_timer.h"
#include "esp_log.h"

#include "badge.h"
#include "badge/led_patterns.h"
#include "badge_game.h"
#include "led.h"

static const char *TAG = "led_patterns";

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

// -------------------------------------------------------------------------------------------------
// Generic ease in/out pattern
// -------------------------------------------------------------------------------------------------
#define EASE_FIXED_POINT_SCALE 1024

typedef enum { EASE_PHASE_FADE_IN, EASE_PHASE_HOLD_IN, EASE_PHASE_FADE_OUT, EASE_PHASE_HOLD_OUT } ease_phase_t;

typedef struct {
    uint8_t r, g, b;
    uint8_t max_brightness;
    uint32_t fade_in_ms;
    uint32_t hold_in_ms;
    uint32_t fade_out_ms;
    uint32_t hold_out_ms;
    bool use_cosine;
    ease_phase_t phase;
    uint64_t phase_start_ms;
} ease_ctx_t;

static ease_ctx_t g_ease_ctx;
static int g_ease_layer = -1;

static void ease_render(uint32_t now_ms, void *user_ctx) {
    ease_ctx_t *ctx  = (ease_ctx_t *)user_ctx;
    uint32_t elapsed = (uint32_t)(now_ms - ctx->phase_start_ms);

    // Fixed-point brightness factor (0..EASE_FIXED_POINT_SCALE)
    uint32_t f_fp = 0;

    switch (ctx->phase) {
        case EASE_PHASE_FADE_IN: {
            if (elapsed >= ctx->fade_in_ms) {
                ctx->phase          = EASE_PHASE_HOLD_IN;
                ctx->phase_start_ms = now_ms;
                f_fp                = EASE_FIXED_POINT_SCALE;
            } else {
                uint64_t num = (uint64_t)elapsed * EASE_FIXED_POINT_SCALE;
                f_fp         = (uint32_t)(num / ctx->fade_in_ms);
                if (ctx->use_cosine) {
                    float t  = (float)f_fp / (float)EASE_FIXED_POINT_SCALE;
                    float cf = 0.5f - 0.5f * cosf((float)M_PI * t);
                    f_fp     = (uint32_t)(cf * (float)EASE_FIXED_POINT_SCALE + 0.5f);
                }
            }
            break;
        }
        case EASE_PHASE_HOLD_IN: {
            f_fp = EASE_FIXED_POINT_SCALE;
            if (elapsed >= ctx->hold_in_ms) {
                ctx->phase          = EASE_PHASE_FADE_OUT;
                ctx->phase_start_ms = now_ms;
            }
            break;
        }
        case EASE_PHASE_FADE_OUT: {
            if (elapsed >= ctx->fade_out_ms) {
                ctx->phase          = EASE_PHASE_HOLD_OUT;
                ctx->phase_start_ms = now_ms;
                f_fp                = 0;
            } else {
                uint64_t num = (uint64_t)(ctx->fade_out_ms - elapsed) * EASE_FIXED_POINT_SCALE;
                f_fp         = (uint32_t)(num / ctx->fade_out_ms);
                if (ctx->use_cosine) {
                    float t  = (float)f_fp / (float)EASE_FIXED_POINT_SCALE;
                    float cf = 0.5f - 0.5f * cosf((float)M_PI * t);
                    f_fp     = (uint32_t)(cf * (float)EASE_FIXED_POINT_SCALE + 0.5f);
                }
            }
            break;
        }
        case EASE_PHASE_HOLD_OUT: {
            f_fp = 0;
            if (elapsed >= ctx->hold_out_ms) {
                ctx->phase          = EASE_PHASE_FADE_IN;
                ctx->phase_start_ms = now_ms;
            }
            break;
        }
        default: f_fp = 0; break;
    }

    // Scale brightness and apply to all LEDs
    const uint32_t peak = ctx->max_brightness;
    const uint32_t FP   = EASE_FIXED_POINT_SCALE;

    for (int i = 0; i < NUM_LEDS; i++) {
        uint32_t r_lin = (uint32_t)ctx->r * f_fp * peak / (FP * 255);
        uint32_t g_lin = (uint32_t)ctx->g * f_fp * peak / (FP * 255);
        uint32_t b_lin = (uint32_t)ctx->b * f_fp * peak / (FP * 255);
        uint8_t r      = (r_lin > 255) ? 255 : (uint8_t)r_lin;
        uint8_t g      = (g_lin > 255) ? 255 : (uint8_t)g_lin;
        uint8_t b      = (b_lin > 255) ? 255 : (uint8_t)b_lin;
        led_gamma_apply_rgb(&r, &g, &b);
        led_set(i, r, g, b);
    }
}

int led_pattern_ease(uint8_t r, uint8_t g, uint8_t b, uint32_t fade_in_ms, uint32_t hold_in_ms, uint32_t fade_out_ms,
                     uint32_t hold_out_ms, bool use_cosine_easing, uint8_t max_brightness) {
    if (g_ease_layer >= 0) {
        led_layer_destroy(g_ease_layer);
        g_ease_layer = -1;
    }

    g_ease_ctx.r              = r;
    g_ease_ctx.g              = g;
    g_ease_ctx.b              = b;
    g_ease_ctx.max_brightness = max_brightness;
    g_ease_ctx.fade_in_ms     = fade_in_ms;
    g_ease_ctx.hold_in_ms     = hold_in_ms;
    g_ease_ctx.fade_out_ms    = fade_out_ms;
    g_ease_ctx.hold_out_ms    = hold_out_ms;
    g_ease_ctx.use_cosine     = use_cosine_easing;
    g_ease_ctx.phase          = EASE_PHASE_FADE_IN;
    g_ease_ctx.phase_start_ms = esp_timer_get_time() / 1000ULL;

    g_ease_layer = led_layer_create(LED_PRIORITY_LOW, ease_render, &g_ease_ctx, true, false, true);
    if (g_ease_layer < 0) {
        ESP_LOGE(TAG, "Failed to create ease LED layer (err=%d)", g_ease_layer);
    }
    return g_ease_layer;
}

// -------------------------------------------------------------------------------------------------
// Faction-specific helpers
// -------------------------------------------------------------------------------------------------
void led_pattern_faction_start() {
    if (!badge_config.faction_leds) {
        ESP_LOGD(TAG, "Faction LEDs disabled in config");
        led_pattern_faction_stop();
        return;
    }

    if (badge_config.team_id == 0) {
        ESP_LOGD(TAG, "No team assigned, skipping faction LED pattern");
        led_pattern_faction_stop();
        return;
    }

    if (!badge_config.sorting_hat) {
        ESP_LOGD(TAG, "Sorting hat not completed, skipping faction LED pattern");
        led_pattern_faction_stop();
        return;
    }

    faction_id_t faction = faction_from_team_id(badge_config.team_id);
    if (faction == FACTION_NONE) {
        ESP_LOGD(TAG, "Invalid team_id %d, skipping faction LED pattern", badge_config.team_id);
        led_pattern_faction_stop();
        return;
    }

    faction_t faction_info = get_faction(faction);

    // Timing constants from original implementation
    const uint32_t fade_in_ms    = 2000;
    const uint32_t hold_in_ms    = 2000;
    const uint32_t fade_out_ms   = 2000;
    const uint32_t hold_out_ms   = 5000;
    const uint8_t max_brightness = 50;

    int handle = led_pattern_ease(faction_info.led_color.red, faction_info.led_color.green, faction_info.led_color.blue,
                                  fade_in_ms, hold_in_ms, fade_out_ms, hold_out_ms, true, max_brightness);

    if (handle >= 0) {
        ESP_LOGD(TAG, "Started faction LED pattern (handle=%d, faction=%s)", handle, faction_info.name);
    }
}

void led_pattern_faction_stop() {
    if (g_ease_layer >= 0) {
        ESP_LOGD(TAG, "Stopping faction LED pattern (handle=%d)", g_ease_layer);
        led_layer_destroy(g_ease_layer);
        g_ease_layer = -1;
    }
}

// -------------------------------------------------------------------------------------------------
// Dawn Accord Violation Pattern
// -------------------------------------------------------------------------------------------------

typedef struct {
    int layer_handle;
    uint8_t state; // 0=flash1_on, 1=flash1_off, 2=flash2_on, 3=pause
    uint32_t next_time_ms;
} dawn_accord_violation_ctx_t;

static dawn_accord_violation_ctx_t *g_violation_ctx = NULL;

static void violation_render(uint32_t now_ms, void *user_ctx) {
    dawn_accord_violation_ctx_t *ctx = (dawn_accord_violation_ctx_t *)user_ctx;
    if (!ctx) {
        return;
    }

    // Check if we need to transition to next state
    if (now_ms >= ctx->next_time_ms) {
        switch (ctx->state) {
            case 0: // First flash ON -> OFF
                ctx->state        = 1;
                ctx->next_time_ms = now_ms + 100;
                break;

            case 1: // First flash OFF -> Second flash ON
                ctx->state        = 2;
                ctx->next_time_ms = now_ms + 100;
                break;

            case 2: // Second flash ON -> Pause
                ctx->state        = 3;
                ctx->next_time_ms = now_ms + 500;
                break;

            case 3: // Pause -> First flash ON
                ctx->state        = 0;
                ctx->next_time_ms = now_ms + 100;
                break;
        }
    }

    // Render current state (called every frame)
    switch (ctx->state) {
        case 0: // First flash ON
        case 2: // Second flash ON
            for (int i = 0; i < NUM_LEDS; i++) {
                led_set(i, 255, 0, 0);
            }
            break;

        case 1: // First flash OFF
        case 3: // Pause
            for (int i = 0; i < NUM_LEDS; i++) {
                led_set(i, 0, 0, 0);
            }
            break;
    }
}

int led_pattern_dawn_accord_violation_start() {
    if (g_violation_ctx) {
        ESP_LOGD(TAG, "Dawn Accord violation pattern already running (handle=%d)", g_violation_ctx->layer_handle);
        return g_violation_ctx->layer_handle;
    }

    dawn_accord_violation_ctx_t *ctx = malloc(sizeof(dawn_accord_violation_ctx_t));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to allocate Dawn Accord violation context");
        return -1;
    }

    ctx->state        = 0;
    ctx->next_time_ms = 0;
    ctx->layer_handle = led_layer_create(LED_PRIORITY_HIGH, violation_render, ctx, true, true, true);

    if (ctx->layer_handle < 0) {
        ESP_LOGE(TAG, "Failed to create Dawn Accord violation layer");
        free(ctx);
        return -1;
    }

    g_violation_ctx = ctx;
    ESP_LOGI(TAG, "Started Dawn Accord violation pattern (handle=%d)", ctx->layer_handle);
    return ctx->layer_handle;
}

void led_pattern_dawn_accord_violation_stop() {
    if (!g_violation_ctx) {
        ESP_LOGD(TAG, "No Dawn Accord violation pattern to stop");
        return;
    }

    ESP_LOGI(TAG, "Stopping Dawn Accord violation pattern (handle=%d)", g_violation_ctx->layer_handle);
    led_layer_destroy(g_violation_ctx->layer_handle);
    free(g_violation_ctx);
    g_violation_ctx = NULL;
}
