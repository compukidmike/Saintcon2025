#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_err.h>
#include <stdint.h>
#include <stddef.h>

/**
 * @brief Encode data to Base64 format.
 *
 * @param input Pointer to the input data.
 * @param input_len Length of the input data.
 * @param output Pointer to the output buffer (must be large enough to hold the Base64-encoded data).
 * @param output_len As input - size of the output buffer. Actual bytes written are returned in this variable on success.
 * @return
 *     - ESP_OK if the encoding is successful
 *     - ESP_INVALID_ARG if the input parameters are invalid
 *     - ESP_ERR_INVALID_SIZE if the output buffer is too small. Needed buffer size is returned in *output_len.
 */
esp_err_t base64_encode(const uint8_t *input, size_t input_len, char *output, size_t *output_len);

/**
 * @brief Decode data from Base64 format.
 *
 * @param input Pointer to the Base64-encoded input data.
 * @param input_len Length of the input data.
 * @param output Pointer to the output buffer (must be large enough to hold the decoded data).
 * @param output_len As input - size of the output buffer. Actual bytes written are returned in this variable.
 * @return
 *     - ESP_OK if the decoding is successful
 *     - ESP_INVALID_ARG if the input parameters are invalid
 *     - ESP_ERR_INVALID_SIZE if the output buffer is too small. Needed buffer size is returned in *output_len.
 */
esp_err_t base64_decode(const char *input, size_t input_len, uint8_t *output, size_t *output_len);

/**
 * @brief Compute the SHA-256 hash of the input data.
 *
 * @param data Pointer to the input data.
 * @param data_len Length of the input data.
 * @param out Pointer to the output buffer (must be at least 32 bytes).
 * @return
 *     - ESP_OK if the hash is computed successfully
 *     - ESP_FAIL if the hash computation fails
 */
esp_err_t sha256(const uint8_t *data, size_t data_len, uint8_t *out);

#ifdef __cplusplus
}
#endif
