#include "esp_err.h"
#include "esp_log.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha256.h"

#include "utils.h"

static const char *TAG = "secure_element";

esp_err_t base64_encode(const uint8_t *input, size_t input_len, char *output, size_t *output_len) {
    if (input == NULL || output == NULL || output_len == NULL) {
        ESP_LOGE(TAG, "Base64 Encode: Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    size_t required_len = 4 * ((input_len + 2) / 3);
    if (*output_len < required_len) {
        ESP_LOGE(TAG, "Base64 Encode: Output buffer too small, required: %zu", required_len);
        *output_len = required_len; // Inform the caller of the required size
        return ESP_ERR_INVALID_SIZE;
    }

    int ret = mbedtls_base64_encode((unsigned char *)output, *output_len, output_len, input, input_len);
    if (ret != 0) {
        ESP_LOGE(TAG, "Base64 Encode: Failed to encode data: %d", ret);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t base64_decode(const char *input, size_t input_len, uint8_t *output, size_t *output_len) {
    if (input == NULL || output == NULL || output_len == NULL) {
        ESP_LOGE(TAG, "Base64 Decode: Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    size_t required_len = (input_len / 4) * 3;
    if (input_len % 4 != 0) {
        required_len += 3; // Round up to nearest multiple of 3
    }
    if (*output_len < required_len) {
        ESP_LOGE(TAG, "Base64 Decode: Output buffer too small, required: %zu", required_len);
        *output_len = required_len; // Inform the caller of the required size
        return ESP_ERR_INVALID_SIZE;
    }

    int ret = mbedtls_base64_decode(output, *output_len, output_len, (const unsigned char *)input, input_len);
    if (ret != 0) {
        ESP_LOGE(TAG, "Base64 Decode: Failed to decode data: %d", ret);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t sha256(const uint8_t *data, size_t data_len, uint8_t *out) {
    if (data == NULL || out == NULL) {
        ESP_LOGE(TAG, "SHA-256: Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);

    int ret = mbedtls_sha256_starts(&ctx, 0); // 0 for SHA-256 (not SHA-224)
    if (ret != 0) {
        ESP_LOGE(TAG, "SHA-256: Failed to start context: %d", ret);
        mbedtls_sha256_free(&ctx);
        return ESP_FAIL;
    }

    ret = mbedtls_sha256_update(&ctx, data, data_len);
    if (ret != 0) {
        ESP_LOGE(TAG, "SHA-256: Failed to update context: %d", ret);
        mbedtls_sha256_free(&ctx);
        return ESP_FAIL;
    }

    ret = mbedtls_sha256_finish(&ctx, out);
    if (ret != 0) {
        ESP_LOGE(TAG, "SHA-256: Failed to finish context: %d", ret);
        mbedtls_sha256_free(&ctx);
        return ESP_FAIL;
    }

    mbedtls_sha256_free(&ctx);
    return ESP_OK;
}
