#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

// Expose functions from utils.h
#include "../utils.h"

typedef enum {
    SE_KEY_IO_KEY        = 0,
    SE_KEY_IO_SEED       = 1,
    SE_KEY_HMAC          = 2,
    SE_KEY_HASH_PREFIX   = 3,
    SE_KEY_ECC256_PRIV   = 4,
    SE_KEY_AES128_SYMM   = 5,
    SE_KEY_REGISTRY      = 6,
    SE_KEY_DEBUG_SHA     = 7,
    SE_KEY_BIG_DATA      = 8,
    SE_KEY_ECC256_PUB    = 9,
    SE_KEY_WIFI_SSID     = 10,
    SE_KEY_WIFI_PASSWORD = 11,
    SE_KEY_HMAC_CTF      = 12,
    SE_KEY_HASH_CTF      = 13,
    SE_KEY_CTF_PASSWORD  = 14,
    SE_KEY_MISC          = 15,
    SE_KEY_MAX           = 15
} se_slot_id_t;

// Slot utility macros
#define SLOT_SIZE(slot)   (((slot) <= 7) ? 36 : ((slot) == 8) ? 416 : ((slot) <= 15) ? 72 : 0)
#define SLOT_CAP(slot)    (((slot) <= 7) ? 32 : ((slot) == 8) ? 416 : ((slot) <= 15) ? 64 : 0)
#define SLOT_BLOCKS(slot) ((SLOT_SIZE(slot) + ATCA_BLOCK_SIZE - 1) / ATCA_BLOCK_SIZE)

// List of the API secrets that will be locked after provisioning
#ifdef CONFIG_DEVICE_TYPE_BADGE /* clang-format off */
#define API_SLOTS_LIST                        \
    X(SE_KEY_HMAC,        "HMAC Secret")      \
    X(SE_KEY_HASH_PREFIX, "Hash Prefix")      \
    X(SE_KEY_HMAC_CTF,    "CTF HMAC Secret")  \
    X(SE_KEY_HASH_CTF,    "CTF Hash Prefix")
#else
#define API_SLOTS_LIST                        \
    X(SE_KEY_HMAC,        "HMAC Secret")      \
    X(SE_KEY_HASH_PREFIX, "Hash Prefix")
#endif
#undef X /* clang-format on */
/* Count how many entries are in API_SLOTS_LIST */
#define X(key, name) +1
enum { NUM_API_SLOTS = 0 API_SLOTS_LIST };
#undef X
typedef struct {
    se_slot_id_t key;
    const char *name;
} se_secret_title_t;
extern const se_secret_title_t api_secrets[NUM_API_SLOTS];

/**
 * @brief Initialize the secure element (ATECC608B).
 *        This will set up the I2C interface and check if the chip is provisioned.
 *        If the chip is not provisioned, it will remain unlocked.
 *
 * @return
 *     - ESP_OK if the secure element is initialized successfully
 *     - ESP_FAIL if the secure element initialization fails
 */
esp_err_t se_init();

/**
 * @brief Check if the secure element is provisioned.
 *        A provisioned chip has at minimum the config zone locked.
 *
 * @return
 *     - true if the secure element is provisioned and locked, false otherwise
 */
bool se_ready();

/** Return true if config zone is locked */
bool se_config_locked();
/** Return true if data zone is locked */
bool se_data_locked();

/**
 * @brief Write a value to the given slot
 *
 * @param slot The slot to write to.
 * @param value The value to write.
 * @param len The length of the value to write.
 * @return
 *     - ESP_OK if the write is successful
 *     - ESP_ERR_INVALID_ARG if the slot is invalid or the value is NULL
 *     - ESP_ERR_INVALID_STATE if the secure element is not initialized or provisioned
 */
esp_err_t se_write_slot(se_slot_id_t slot, const uint8_t *value, size_t len);

/**
 * @brief Read a value from the given slot
 *
 * @param slot The slot to read from.
 * @param value The buffer to read the value into.
 * @param len The length of the buffer.
 * @return
 *     - ESP_OK if the read is successful
 *     - ESP_ERR_INVALID_ARG if the slot is invalid or the buffer is NULL
 *     - ESP_ERR_INVALID_STATE if the secure element is not initialized or provisioned
 */
esp_err_t se_read_slot(se_slot_id_t slot, uint8_t *value, size_t len);

/**
 * @brief Clear the contents of a given slot
 *
 * @param slot The slot to clear.
 * @return
 *     - ESP_OK if the clear is successful
 *     - ESP_ERR_INVALID_ARG if the slot is invalid
 *     - ESP_ERR_INVALID_STATE if the secure element is not initialized or provisioned
 */
esp_err_t se_clear_slot(se_slot_id_t slot);

/**
 * @brief Check if a given slot has been written to (touched).
 *        This is tracked via a registry stored in a dedicated slot.
 *
 * @param slot The slot to check.
 * @return
 *     - true if the slot has been written to
 *     - false if the slot is unwritten or invalid
 */
bool se_slot_touched(se_slot_id_t slot);

/**
 * @brief Validate slot against known value that should have been written. Useful for non-readable slots
 *        that can be used with MAC and/or HMAC.
 *
 * @param slot The slot to validate.
 * @param expected The expected key/value that should have been written.
 * @param len The length of the expected value.
 * @return
 *     - ESP_OK if the slot is valid and the expected value matches the slot contents
 *     - ESP_ERR_INVALID_ARG if the slot is invalid or the expected value is NULL
 *     - ESP_ERR_INVALID_STATE if the secure element is not initialized or provisioned
 */
esp_err_t se_validate_slot(se_slot_id_t slot, const uint8_t *expected, size_t len);

/**
 * @brief Lock a given slot if it hasn't already been locked.
 *
 * @param slot The slot to lock.
 * @return
 *     - ESP_OK if the slot is locked successfully
 *     - ESP_ERR_INVALID_ARG if the slot is invalid
 *     - ESP_ERR_INVALID_STATE if the secure element is not initialized or provisioned
 */
esp_err_t se_lock_slot(se_slot_id_t slot);

/**
 * @brief Check if a given slot is locked.
 *
 * @param slot The slot to check.
 * @return
 *     - true if the slot is locked, false otherwise
 */
bool se_slot_locked(se_slot_id_t slot);

/**
 * @brief Get an HMAC signature for a message with the private key in the HMAC slot.
 *
 * @param msg The message to sign (must not be NULL).
 * @param msg_len The length of the message to sign.
 * @param digest The buffer to store the HMAC digest (must be 64 bytes).
 * @return
 *     - ESP_OK if the message is signed successfully
 *     - ESP_ERR_INVALID_ARG if the message is NULL or the digest buffer is not 64 bytes
 *     - ESP_ERR_INVALID_STATE if the secure element is not initialized or provisioned
 */
esp_err_t se_hmac(const uint8_t *msg, size_t msg_len, uint8_t *digest);

#ifdef __cplusplus
}
#endif