#include <stdio.h>

#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"

#include "lcd.h"
#include "leds.h"
#include "player.h"

static const char *TAG = "main";

static void mount_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path              = "/spiffs",
        .partition_label        = "storage",
        .max_files              = 4,
        .format_if_mount_failed = false,
    };
    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(err));
        return;
    }
    size_t total = 0, used = 0;
    if (esp_spiffs_info("storage", &total, &used) == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS: %u / %u bytes", (unsigned)used, (unsigned)total);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "SAINTCON 2025 attract player — MJPEG + LED ring");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    mount_spiffs();
    ESP_ERROR_CHECK(lcd_init());
    ESP_ERROR_CHECK(leds_init());

    /* Never dim — backlight stays full for the attract cabinet */
    lcd_set_backlight(255);

    ESP_ERROR_CHECK(player_run_loop());
}
