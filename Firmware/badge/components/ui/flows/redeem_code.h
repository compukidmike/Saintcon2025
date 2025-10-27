#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "lvgl.h"
#include "api.h"

// Callback signature for redeem code flow completion
typedef void (*redeem_code_cb_t)(int button_idx, const api_result_t *result, void *user_data);

/** Optional UI text overrides for the redeem flow. Provide NULL to use defaults. */
typedef struct {
    const char *spinner_text;    // e.g., "Redeeming code..." or "Attempting node unlock..."
    const char *success_title;   // default: "Success"
    const char *success_message; // default: "Code redeemed successfully!"
    const char *failure_title;   // default: "Failed"
} redeem_ui_texts_t;

/**
 * @brief Initiates the redeem code flow: shows spinner, runs API, shows result modal, invokes callback on button press.
 *
 * @param code The code to redeem. Needs to be exactly 5 characters long and use TI_BASE32_NC characters.
 * @param code_len Length of the code string (should be 5).
 * @param callback The callback to invoke on button press.
 * @param user_data User data to pass to the callback.
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t try_redeem_code(const char *code, size_t code_len, redeem_code_cb_t callback, void *user_data);

/** Variant that accepts optional UI text overrides. */
esp_err_t try_redeem_code_ctx(const char *code, size_t code_len, const redeem_ui_texts_t *ui, redeem_code_cb_t callback,
                              void *user_data);

#ifdef __cplusplus
}
#endif
