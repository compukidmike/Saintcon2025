#include "player.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jpeg_decoder.h"

#include "lcd.h"
#include "leds.h"

static const char *TAG = "player";

#define SCMJ_PATH "/spiffs/attract.scmj"
#define MAX_JPEG  (80 * 1024)

typedef struct __attribute__((packed)) {
    uint16_t w;
    uint16_t h;
    uint16_t fps;
    uint32_t nframes;
} scmj_header_t;

esp_err_t player_run_loop(void) {
    FILE *f = fopen(SCMJ_PATH, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open %s", SCMJ_PATH);
        return ESP_ERR_NOT_FOUND;
    }

    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "SCMJ", 4) != 0) {
        ESP_LOGE(TAG, "Bad SCMJ magic");
        fclose(f);
        return ESP_ERR_INVALID_RESPONSE;
    }

    scmj_header_t hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fclose(f);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "SCMJ %ux%u @ %u fps, %lu frames (hdr=%u)", hdr.w, hdr.h, hdr.fps, (unsigned long)hdr.nframes,
             (unsigned)sizeof(hdr));

    if (hdr.w != LCD_H_RES || hdr.h != LCD_V_RES) {
        ESP_LOGW(TAG, "Resolution mismatch (got %ux%u)", hdr.w, hdr.h);
    }
    if (hdr.nframes == 0 || hdr.nframes > 100000) {
        ESP_LOGE(TAG, "Implausible frame count %lu — abort", (unsigned long)hdr.nframes);
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }

    const size_t frame_bytes = (size_t)LCD_H_RES * LCD_V_RES * sizeof(uint16_t);
    uint8_t *jpeg_buf = heap_caps_malloc(MAX_JPEG, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!jpeg_buf) {
        jpeg_buf = malloc(MAX_JPEG);
    }
    uint16_t *fb = heap_caps_malloc(frame_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!fb) {
        fb = heap_caps_malloc(frame_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!fb) {
        fb = malloc(frame_bytes);
    }
    if (!jpeg_buf || !fb) {
        ESP_LOGE(TAG, "OOM for decode buffers");
        free(jpeg_buf);
        free(fb);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    const uint32_t frame_period_us = 1000000UL / (hdr.fps ? hdr.fps : 12);
    long data_start                = ftell(f);

    while (1) {
        fseek(f, data_start, SEEK_SET);
        int64_t loop_t0 = esp_timer_get_time();

        for (uint32_t i = 0; i < hdr.nframes; i++) {
            int64_t frame_deadline = loop_t0 + (int64_t)(i + 1) * frame_period_us;

            uint32_t jsz = 0;
            if (fread(&jsz, sizeof(jsz), 1, f) != 1 || jsz == 0 || jsz > MAX_JPEG) {
                ESP_LOGE(TAG, "Bad jpeg size at frame %lu (sz=%lu)", (unsigned long)i, (unsigned long)jsz);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
            }
            if (fread(jpeg_buf, 1, jsz, f) != jsz) {
                ESP_LOGE(TAG, "Short jpeg read at frame %lu", (unsigned long)i);
                break;
            }

            esp_jpeg_image_cfg_t jpeg_cfg = {
                .indata      = jpeg_buf,
                .indata_size = jsz,
                .outbuf      = (uint8_t *)fb,
                .outbuf_size = frame_bytes,
                .out_format  = JPEG_IMAGE_FORMAT_RGB565,
                .out_scale   = JPEG_IMAGE_SCALE_0,
                .flags =
                    {
                        .swap_color_bytes = 0,
                    },
            };
            esp_jpeg_image_output_t outimg;
            esp_err_t err = esp_jpeg_decode(&jpeg_cfg, &outimg);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "decode fail frame %lu: %s", (unsigned long)i, esp_err_to_name(err));
            } else {
                lcd_draw_bitmap(0, 0, LCD_H_RES, LCD_V_RES, fb);
            }

            uint32_t local_ms = (uint32_t)((esp_timer_get_time() - loop_t0) / 1000);
            leds_sync_attract(local_ms);

            int64_t now = esp_timer_get_time();
            if (now < frame_deadline) {
                int64_t delay_us = frame_deadline - now;
                if (delay_us > 1000) {
                    vTaskDelay(pdMS_TO_TICKS((uint32_t)(delay_us / 1000)));
                }
            }
        }
        ESP_LOGI(TAG, "loop restart");
    }

    /* unreachable */
    free(jpeg_buf);
    free(fb);
    fclose(f);
    return ESP_OK;
}
