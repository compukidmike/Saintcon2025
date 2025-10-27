#include "led.h"
#include "esp_log.h"
#include "esp_check.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <string.h>
#include <math.h>

static const char *TAG = "LED Strip";

// Create the LED strip object
led_strip_handle_t led_strip;
static uint8_t global_brightness = 255; // Default to full brightness

// Frame buffering to reduce flicker
static uint8_t frame_buf[NUM_LEDS][3];  // current frame being built (scaled RGB values)
static uint8_t last_frame[NUM_LEDS][3]; // last frame pushed to hardware
static bool frame_capture = false;      // true while a pattern layer is rendering

// -------------------------------------------------------------------------------------------------
// Gamma Correction Support
// -------------------------------------------------------------------------------------------------

#ifdef CONFIG_LED_GAMMA_CORRECTION
static bool gamma_enabled = true;
#else
static bool gamma_enabled = false;
#endif
// gamma ≈ 2.2 table (linear 0..255 -> gamma-encoded 0..255)
static const uint8_t led_gamma_table[256] = {
    0,   20,  26,  30,  34,  37,  40,  43,  46,  48,  50,  52,  54,  56,  58,  60,  62,  64,  66,  68,  70,  71,  73,  75,
    77,  78,  80,  82,  83,  85,  87,  88,  90,  91,  93,  95,  96,  98,  99,  101, 102, 104, 105, 107, 108, 110, 111, 113,
    114, 116, 117, 119, 120, 121, 123, 124, 126, 127, 128, 130, 131, 132, 134, 135, 136, 138, 139, 140, 142, 143, 144, 146,
    147, 148, 149, 151, 152, 153, 154, 156, 157, 158, 159, 161, 162, 163, 164, 165, 167, 168, 169, 170, 171, 172, 174, 175,
    176, 177, 178, 179, 180, 181, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 197, 198, 199, 200, 201,
    202, 203, 204, 205, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 216, 217, 218, 219, 220, 221, 222, 222,
    223, 224, 225, 226, 227, 227, 228, 229, 230, 231, 231, 232, 233, 234, 235, 235, 236, 237, 238, 238, 239, 240, 241, 241,
    242, 243, 244, 244, 245, 246, 246, 247, 248, 249, 249, 250, 251, 251, 252, 253, 253, 254, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

void led_gamma_enable(bool enable) {
    gamma_enabled = enable;
}
bool led_gamma_is_enabled() {
    return gamma_enabled;
}
uint8_t led_gamma_apply(uint8_t v) {
    return gamma_enabled ? led_gamma_table[v] : v;
}
void led_gamma_apply_rgb(uint8_t *r, uint8_t *g, uint8_t *b) {
    if (!gamma_enabled) {
        return;
    }
    *r = led_gamma_table[*r];
    *g = led_gamma_table[*g];
    *b = led_gamma_table[*b];
}

// -------------------------------------------------------------------------------------------------
// Layered Pattern Manager
// -------------------------------------------------------------------------------------------------

#define LED_MAX_LAYERS        10
#define LED_RENDER_TASK_STACK 3072
#define LED_RENDER_TASK_PRIO  4
#define LED_FRAME_INTERVAL_MS 40 // 25 FPS default

typedef enum {
    LED_LAYER_TYPE_DYNAMIC = 0,
    LED_LAYER_TYPE_STATIC_COLOR,
} led_layer_type_t;

typedef struct {
    bool in_use;
    bool active;
    bool paused;
    bool exclusive;
    bool clear_before;
    led_pattern_priority_t priority;
    uint32_t creation_seq;
    led_pattern_render_cb render_cb;
    void *user_ctx;
    led_layer_type_t type;
    // Static color params
    uint8_t r, g, b, b_brightness;
    // Timed layer support
    bool timed;
    bool auto_destroy;
    uint32_t duration_ms;
    uint64_t start_time_ms;
} led_layer_t;

static led_layer_t layers[LED_MAX_LAYERS];
static uint32_t layer_creation_counter     = 0;
static TaskHandle_t led_render_task_handle = NULL;
static SemaphoreHandle_t layer_lock;
static volatile bool force_refresh = false;
static int current_render_layer    = -1;
static bool pending_destroy[LED_MAX_LAYERS];

static void led_render_task(void *arg);
static void led_render_layer(led_layer_t *layer, uint32_t now_ms);
static int pick_active_layer();

// LED strip common configuration (using Kconfig values)
led_strip_config_t strip_config = {
    .strip_gpio_num = CONFIG_LED_GPIO,     // The GPIO that connected to the LED strip's data line
    .max_leds       = CONFIG_LED_NUM_LEDS, // The number of LEDs in the strip,
#ifdef CONFIG_LED_STRIP_MODEL_WS2812
    .led_model = LED_MODEL_WS2812, // LED strip model, it determines the bit timing
#elif defined(CONFIG_LED_STRIP_MODEL_SK6812)
    .led_model = LED_MODEL_SK6812,
#elif defined(CONFIG_LED_STRIP_MODEL_WS2811)
    .led_model = LED_MODEL_WS2811,
#else
    .led_model = LED_MODEL_WS2812, // Default to WS2812
#endif
#ifdef CONFIG_LED_COLOR_FORMAT_GRB
    .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color component format
#elif defined(CONFIG_LED_COLOR_FORMAT_RGB)
    .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB,
#elif defined(CONFIG_LED_COLOR_FORMAT_BGR)
    .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_BGR,
#else
    .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // Default to GRB
#endif
    .flags =
        {
            .invert_out = false, // don't invert the output signal
        },
};

// RMT backend specific configuration
led_strip_rmt_config_t rmt_config = {
    .clk_src           = RMT_CLK_SRC_DEFAULT, // different clock source can lead to different power consumption
    .resolution_hz     = 10 * 1000 * 1000,    // RMT counter clock frequency: 10MHz
    .mem_block_symbols = 64,                  // the memory size of each RMT channel, in words (4 bytes)
    .flags =
        {
#if defined(CONFIG_IDF_TARGET_ESP32S3) // Currently only ESP32-S3 supports RMT with DMA
            .with_dma = true,
#else
            .with_dma = false,
#endif
        },
};

esp_err_t led_init() {
    if (led_strip != NULL) {
        ESP_LOGW(TAG, "led_init: LED strip already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing LED strip on GPIO %d with %d LEDs", LED_GPIO, NUM_LEDS);
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "LED strip created successfully");

    if (!layer_lock) {
        layer_lock = xSemaphoreCreateMutex();
    }
    if (led_render_task_handle == NULL) {
        xTaskCreate(led_render_task, "led_render", LED_RENDER_TASK_STACK, NULL, LED_RENDER_TASK_PRIO, &led_render_task_handle);
    }
    return ESP_OK;
}

esp_err_t led_set(uint32_t index, uint32_t red, uint32_t green, uint32_t blue) {
    if (index >= NUM_LEDS) {
        ESP_LOGE(TAG, "LED index %d out of range (max %d)", index, NUM_LEDS - 1);
        return ESP_ERR_INVALID_ARG;
    }
    if (led_strip == NULL) {
        ESP_LOGE(TAG, "LED strip not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (frame_capture) {
        frame_buf[index][0] = (uint8_t)red;
        frame_buf[index][1] = (uint8_t)green;
        frame_buf[index][2] = (uint8_t)blue;
        return ESP_OK;
    }
    return led_strip_set_pixel(led_strip, index, red, green, blue);
}

esp_err_t led_set_brightness(uint8_t brightness) {
    global_brightness = brightness;
    return ESP_OK;
}

uint8_t led_get_brightness() {
    return global_brightness;
}

esp_err_t led_set_scaled(uint32_t index, uint32_t red, uint32_t green, uint32_t blue) {
    if (index >= NUM_LEDS) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t scaled_red   = (red * global_brightness) / 255;
    uint8_t scaled_green = (green * global_brightness) / 255;
    uint8_t scaled_blue  = (blue * global_brightness) / 255;
    if (frame_capture) {
        frame_buf[index][0] = scaled_red;
        frame_buf[index][1] = scaled_green;
        frame_buf[index][2] = scaled_blue;
        return ESP_OK;
    }
    return led_strip_set_pixel(led_strip, index, scaled_red, scaled_green, scaled_blue);
}

esp_err_t led_set_with_brightness(uint32_t index, uint32_t red, uint32_t green, uint32_t blue, uint8_t brightness) {
    if (index >= NUM_LEDS) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t scaled_red   = (red * brightness) / 255;
    uint8_t scaled_green = (green * brightness) / 255;
    uint8_t scaled_blue  = (blue * brightness) / 255;
    if (frame_capture) {
        frame_buf[index][0] = scaled_red;
        frame_buf[index][1] = scaled_green;
        frame_buf[index][2] = scaled_blue;
        return ESP_OK;
    }
    return led_strip_set_pixel(led_strip, index, scaled_red, scaled_green, scaled_blue);
}

esp_err_t led_clear() {
    if (frame_capture) {
        memset(frame_buf, 0, sizeof(frame_buf));
        return ESP_OK;
    }
    return led_strip_clear(led_strip);
}

esp_err_t led_show() {
    return led_strip_refresh(led_strip);
}

// -------------------------------------------------------------------------------------------------
// Internal helpers
// -------------------------------------------------------------------------------------------------

static void lock_layers() {
    if (layer_lock) {
        xSemaphoreTake(layer_lock, portMAX_DELAY);
    }
}
static void unlock_layers() {
    if (layer_lock) {
        xSemaphoreGive(layer_lock);
    }
}

static void led_render_layer(led_layer_t *layer, uint32_t now_ms) {
    // Prepare frame buffer - start from previous frame (to allow partial updates) or clear.
    if (layer->clear_before) {
        memset(frame_buf, 0, sizeof(frame_buf));
    } else {
        memcpy(frame_buf, last_frame, sizeof(frame_buf));
    }
    if (layer->type == LED_LAYER_TYPE_STATIC_COLOR) {
        for (int i = 0; i < NUM_LEDS; i++) {
            led_set_with_brightness(i, layer->r, layer->g, layer->b, layer->b_brightness);
        }
    } else if (layer->render_cb) {
        layer->render_cb(now_ms, layer->user_ctx);
    }
}

static int pick_active_layer() {
    // Determine the highest priority that currently has any active, unpaused layer.
    bool priority_present[LED_PRIORITY_LOW + 1] = {false};
    for (int i = 0; i < LED_MAX_LAYERS; i++) {
        if (!layers[i].in_use || !layers[i].active || layers[i].paused) {
            continue;
        }
        if (layers[i].priority <= LED_PRIORITY_LOW) {
            priority_present[layers[i].priority] = true;
        }
    }
    led_pattern_priority_t top_priority = LED_PRIORITY_LOW + 1;
    for (int p = LED_PRIORITY_HIGH; p <= LED_PRIORITY_LOW; p++) {
        if (priority_present[p]) {
            top_priority = p;
            break;
        }
    }
    if (top_priority > LED_PRIORITY_LOW) {
        return -1;
    }

    // Among layers of that priority pick newest (highest creation_seq)
    int selected      = -1;
    uint32_t best_seq = 0;
    for (int i = 0; i < LED_MAX_LAYERS; i++) {
        if (!layers[i].in_use || !layers[i].active || layers[i].paused) {
            continue;
        }
        if (layers[i].priority != top_priority) {
            continue;
        }
        if (selected < 0 || layers[i].creation_seq > best_seq) {
            selected = i;
            best_seq = layers[i].creation_seq;
        }
    }
    return selected;
}

static void led_render_task(void *arg) {
    uint32_t last_wake = xTaskGetTickCount();
    while (1) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(LED_FRAME_INTERVAL_MS));
        uint32_t now_ms = esp_timer_get_time() / 1000ULL;
        lock_layers();
        int layer_index      = pick_active_layer();
        current_render_layer = layer_index;
#ifdef CONFIG_LED_DEBUG_RENDER
        static uint32_t dbg_frame = 0;
        if ((dbg_frame++ % 50) == 0) {
            ESP_LOGD(TAG, "frame=%u active_layer=%d force_refresh=%d", dbg_frame, layer_index, force_refresh);
        }
#endif
        if (layer_index >= 0) {
            frame_capture = true;
            led_render_layer(&layers[layer_index], now_ms);
            frame_capture = false;
            // Diff & push
            bool any_change = false;
            for (int i = 0; i < NUM_LEDS; i++) {
                if (frame_buf[i][0] != last_frame[i][0] || frame_buf[i][1] != last_frame[i][1] ||
                    frame_buf[i][2] != last_frame[i][2]) {
                    any_change = true;
                    led_strip_set_pixel(led_strip, i, frame_buf[i][0], frame_buf[i][1], frame_buf[i][2]);
                }
            }
            if (any_change) {
                led_strip_refresh(led_strip);
                memcpy(last_frame, frame_buf, sizeof(frame_buf));
            }
            // Timed layer expiration
            if (layers[layer_index].timed) {
                uint64_t elapsed = now_ms - layers[layer_index].start_time_ms;
                if (elapsed >= layers[layer_index].duration_ms) {
                    if (layers[layer_index].auto_destroy) {
                        layers[layer_index].in_use = false;
                    } else {
                        layers[layer_index].active = false;
                    }
                    force_refresh = true;
                }
            }
        }
        // Apply deferred destroys
        for (int i = 0; i < LED_MAX_LAYERS; i++) {
            if (pending_destroy[i]) {
                pending_destroy[i] = false;
                memset(&layers[i], 0, sizeof(layers[i]));
                force_refresh = true;
            }
        }
        bool fr       = force_refresh;
        force_refresh = false;
        unlock_layers();

        // Auto-blank if no active layer and residual pixels still lit
        if (layer_index < 0 && led_strip) {
            bool any_on = false;
            for (int i = 0; i < NUM_LEDS && !any_on; i++) {
                any_on = (last_frame[i][0] | last_frame[i][1] | last_frame[i][2]) != 0;
            }
            if (any_on) {
                led_strip_clear(led_strip);
                memset(last_frame, 0, sizeof(last_frame));
            }
        }
        if (fr) {
            // immediate extra frame
            last_wake = xTaskGetTickCount();
        }
    }
}

void led_layers_dump() {
    lock_layers();
    ESP_LOGD(TAG, "--- LED Layers Dump ---");
    for (int i = 0; i < LED_MAX_LAYERS; i++) {
        if (!layers[i].in_use) {
            continue;
        }
        ESP_LOGD(TAG, "[%d] prio=%d active=%d paused=%d excl=%d type=%d seq=%u timed=%d dur=%u ms", i, layers[i].priority,
                 layers[i].active, layers[i].paused, layers[i].exclusive, layers[i].type, layers[i].creation_seq, layers[i].timed,
                 layers[i].duration_ms);
    }
    unlock_layers();
}

// -------------------------------------------------------------------------------------------------
// Public Layer API
// -------------------------------------------------------------------------------------------------

int led_layer_create(led_pattern_priority_t priority, led_pattern_render_cb render_cb, void *user_ctx, bool active,
                     bool exclusive, bool clear_before) {
    if (led_init() != ESP_OK) {
        return -1;
    }
    if (!render_cb) {
        return -2;
    }
    lock_layers();
    int slot = -1;
    for (int i = 0; i < LED_MAX_LAYERS; i++) {
        if (!layers[i].in_use) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        unlock_layers();
        return -3;
    }
    // Reset slot to a clean state (prevents stale timed/auto_destroy flags from previous use)
    memset(&layers[slot], 0, sizeof(layers[slot]));
    layers[slot].in_use       = true;
    layers[slot].active       = active;
    layers[slot].paused       = false;
    layers[slot].exclusive    = exclusive;
    layers[slot].clear_before = clear_before;
    layers[slot].priority     = priority;
    layers[slot].creation_seq = ++layer_creation_counter;
    layers[slot].render_cb    = render_cb;
    layers[slot].user_ctx     = user_ctx;
    layers[slot].type         = LED_LAYER_TYPE_DYNAMIC;
    unlock_layers();
    force_refresh = true;
    return slot;
}

int led_layer_create_timed(led_pattern_priority_t priority, led_pattern_render_cb render_cb, void *user_ctx, bool exclusive,
                           bool clear_before, uint32_t duration_ms, bool auto_destroy) {
    int h = led_layer_create(priority, render_cb, user_ctx, true, exclusive, clear_before);
    if (h < 0) {
        return h;
    }
    lock_layers();
    layers[h].timed         = true;
    layers[h].auto_destroy  = auto_destroy;
    layers[h].duration_ms   = duration_ms;
    layers[h].start_time_ms = esp_timer_get_time() / 1000ULL;
    unlock_layers();
    return h;
}

int led_layer_create_static_color_timed(led_pattern_priority_t priority, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness,
                                        bool exclusive, uint32_t duration_ms, bool auto_destroy) {
    int h = led_layer_create_static_color(priority, r, g, b, brightness, true, exclusive);
    if (h < 0) {
        return h;
    }
    lock_layers();
    layers[h].timed         = true;
    layers[h].auto_destroy  = auto_destroy;
    layers[h].duration_ms   = duration_ms;
    layers[h].start_time_ms = esp_timer_get_time() / 1000ULL;
    unlock_layers();
    return h;
}

esp_err_t led_layer_destroy(int handle) {
    if (handle < 0 || handle >= LED_MAX_LAYERS) {
        return ESP_ERR_INVALID_ARG;
    }
    // If we are currently inside the render callback of this layer (lock held by render task),
    // defer actual destruction to end-of-frame phase to avoid modifying layer array mid-render.
    if (frame_capture && current_render_layer == handle) {
        pending_destroy[handle] = true;
        force_refresh           = true;
        return ESP_OK;
    }
    lock_layers();
    memset(&layers[handle], 0, sizeof(layers[handle]));
    unlock_layers();
    force_refresh = true;
    return ESP_OK;
}

esp_err_t led_layer_set_active(int handle, bool active) {
    if (handle < 0 || handle >= LED_MAX_LAYERS) {
        return ESP_ERR_INVALID_ARG;
    }
    lock_layers();
    if (!layers[handle].in_use) {
        unlock_layers();
        return ESP_ERR_INVALID_STATE;
    }
    layers[handle].active = active;
    unlock_layers();
    force_refresh = true;
    return ESP_OK;
}

esp_err_t led_layer_set_paused(int handle, bool paused) {
    if (handle < 0 || handle >= LED_MAX_LAYERS) {
        return ESP_ERR_INVALID_ARG;
    }
    lock_layers();
    if (!layers[handle].in_use) {
        unlock_layers();
        return ESP_ERR_INVALID_STATE;
    }
    layers[handle].paused = paused;
    unlock_layers();
    force_refresh = true;
    return ESP_OK;
}

esp_err_t led_layer_set_exclusive(int handle, bool exclusive) {
    if (handle < 0 || handle >= LED_MAX_LAYERS) {
        return ESP_ERR_INVALID_ARG;
    }
    lock_layers();
    if (!layers[handle].in_use) {
        unlock_layers();
        return ESP_ERR_INVALID_STATE;
    }
    layers[handle].exclusive = exclusive;
    unlock_layers();
    force_refresh = true;
    return ESP_OK;
}

esp_err_t led_layer_set_priority(int handle, led_pattern_priority_t priority) {
    if (handle < 0 || handle >= LED_MAX_LAYERS) {
        return ESP_ERR_INVALID_ARG;
    }
    lock_layers();
    if (!layers[handle].in_use) {
        unlock_layers();
        return ESP_ERR_INVALID_STATE;
    }
    layers[handle].priority     = priority;
    layers[handle].creation_seq = ++layer_creation_counter;
    unlock_layers();
    force_refresh = true;
    return ESP_OK;
}

esp_err_t led_layer_bump(int handle) {
    if (handle < 0 || handle >= LED_MAX_LAYERS) {
        return ESP_ERR_INVALID_ARG;
    }
    lock_layers();
    if (!layers[handle].in_use) {
        unlock_layers();
        return ESP_ERR_INVALID_STATE;
    }
    layers[handle].creation_seq = ++layer_creation_counter;
    unlock_layers();
    force_refresh = true;
    return ESP_OK;
}

static void static_color_render_cb(uint32_t now_ms, void *user_ctx) {
    (void)now_ms;
    // Actual pixel writes are done in led_render_layer for static type; nothing here.
    (void)user_ctx;
}

int led_layer_create_static_color(led_pattern_priority_t priority, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness,
                                  bool active, bool exclusive) {
    int h = led_layer_create(priority, static_color_render_cb, NULL, active, exclusive, true);
    if (h >= 0) {
        lock_layers();
        layers[h].type         = LED_LAYER_TYPE_STATIC_COLOR;
        layers[h].r            = r;
        layers[h].g            = g;
        layers[h].b            = b;
        layers[h].b_brightness = brightness;
        unlock_layers();
        force_refresh = true;
    }
    return h;
}

esp_err_t led_layer_update_static_color(int handle, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    if (handle < 0 || handle >= LED_MAX_LAYERS) {
        return ESP_ERR_INVALID_ARG;
    }
    lock_layers();
    if (!layers[handle].in_use || layers[handle].type != LED_LAYER_TYPE_STATIC_COLOR) {
        unlock_layers();
        return ESP_ERR_INVALID_STATE;
    }
    layers[handle].r            = r;
    layers[handle].g            = g;
    layers[handle].b            = b;
    layers[handle].b_brightness = brightness;
    unlock_layers();
    force_refresh = true;
    return ESP_OK;
}

int led_layer_current() {
    return current_render_layer;
}

void led_layer_request_refresh() {
    force_refresh = true;
}

esp_err_t led_layers_pause_all_except(int keep_handle) {
    lock_layers();
    for (int i = 0; i < LED_MAX_LAYERS; i++) {
        if (!layers[i].in_use) {
            continue;
        }
        if (i == keep_handle) {
            continue;
        }
        layers[i].paused = true;
    }
    unlock_layers();
    force_refresh = true;
    return ESP_OK;
}

esp_err_t led_layers_resume_all() {
    lock_layers();
    for (int i = 0; i < LED_MAX_LAYERS; i++) {
        if (!layers[i].in_use) {
            continue;
        }
        layers[i].paused = false;
    }
    unlock_layers();
    force_refresh = true;
    return ESP_OK;
}
