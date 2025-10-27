#include "esp_err.h"
#include "esp_log.h"

#include "config.h"
#include "migrate.h"
#include "nvs.h"

static const char *TAG = "nut/config";

#define NUT_NVS_NAMESPACE "nut"

nut_config_t nut_config = NUT_DEFAULTS;

esp_err_t load_nut_config() {
    if (!nvs_ready()) {
        ESP_LOGE(TAG, "NVS not ready");
        return ESP_FAIL;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NUT_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No NVS data found, using defaults");
        nut_config = NUT_DEFAULTS;
        return ESP_OK;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle", esp_err_to_name(err));
        return err;
    }

    uint32_t stored_version = 0;
    err                     = nvs_get_u32(nvs_handle, "nut_version", &stored_version);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No version found or version size mismatch, initializing defaults");
        nut_config = NUT_DEFAULTS;
    } else {
        if (stored_version != NUT_CONFIG_VERSION) {
            ESP_LOGI(TAG, "Configuration version mismatch, migrating if possible (stored: %u, current: %u)", stored_version,
                     NUT_CONFIG_VERSION);
            migrate_nut_config(nvs_handle, stored_version);
        } else {
            size_t required_size = sizeof(nut_config_t);
            err                  = nvs_get_blob(nvs_handle, "nut_config", &nut_config, &required_size);
            if (err != ESP_OK) {
                if (required_size != sizeof(nut_config_t)) {
                    ESP_LOGW(TAG, "Size mismatch loading nut config (expected %d, got %d), using defaults", sizeof(nut_config_t),
                             required_size);
                    nut_config = NUT_DEFAULTS;
                    save_nut_config();
                } else {
                    ESP_LOGW(TAG, "Error (%s) reading nut config, using defaults for now", esp_err_to_name(err));
                    nut_config = NUT_DEFAULTS;
                }
            } else {
                ESP_LOGD(TAG, "Loaded nut config from NVS (%d bytes):", required_size);
                ESP_LOG_BUFFER_HEXDUMP(TAG, &nut_config, sizeof(nut_config), ESP_LOG_DEBUG);
            }
        }
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t save_nut_config() {
    if (!nvs_ready()) {
        ESP_LOGE(TAG, "NVS not ready");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Saving nut config to NVS:");
    ESP_LOG_BUFFER_HEXDUMP(TAG, &nut_config, sizeof(nut_config), ESP_LOG_DEBUG);

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NUT_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u32(nvs_handle, "nut_version", nut_config.version);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) saving nut version", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_blob(nvs_handle, "nut_config", &nut_config, sizeof(nut_config_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) saving nut config", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) committing nut config", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Verify the write by reading it back
    nut_config_t verify_config = {0};
    size_t verify_size         = sizeof(nut_config_t);
    err                        = nvs_get_blob(nvs_handle, "nut_config", &verify_config, &verify_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "NVS write verification: version=%u, type=%d, nut_id=%s", verify_config.version, verify_config.type,
                 verify_config.nut_id);
        if (memcmp(&nut_config, &verify_config, sizeof(nut_config_t)) != 0) {
            ESP_LOGE(TAG, "NVS WRITE VERIFICATION FAILED - data mismatch!");
            ESP_LOG_BUFFER_HEXDUMP(TAG, &verify_config, sizeof(verify_config), ESP_LOG_ERROR);
        } else {
            ESP_LOGI(TAG, "NVS write verified successfully");
        }
    } else {
        ESP_LOGW(TAG, "Failed to verify NVS write: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}