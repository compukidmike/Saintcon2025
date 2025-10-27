#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// NFC return codes
typedef enum {
    NFC_OK = 0,      // Operation successful
    NFC_ERR_INIT,    // Initialization error
    NFC_ERR_READ,    // Read error
    NFC_ERR_WRITE,   // Write error
    NFC_ERR_TIMEOUT, // Operation timed out
} nfc_err_t;

// NFC events
typedef enum {
    NFC_EVENT_FIELD_DETECTED, // RF field detected
    NFC_EVENT_FIELD_LOST,     // RF field lost
    NFC_EVENT_TAG_READ,       // NFC tag read
    NFC_EVENT_TAG_WRITE,      // NFC tag write
    NFC_EVENT_WRITE_BLOCKED,  // Write blocked due to rate limiting
    NFC_EVENT_ERROR           // NFC error
} nfc_event_t;

// NDEF record data structure
typedef struct {
    uint8_t *payload;
    size_t payload_len;
    char *type;
    size_t type_len;
} nfc_ndef_record_t;

// -------------------------------------------------------------------------------------------------
// Default Record Support (optional single record provisioned at initialization)
// -------------------------------------------------------------------------------------------------

typedef enum {
    NFC_DEFAULT_NONE = 0,
    NFC_DEFAULT_TEXT,
    NFC_DEFAULT_URL,
    NFC_DEFAULT_WIFI,
} nfc_default_type_t;

typedef struct {
    nfc_default_type_t type;
    const char *text;      // for TEXT
    const char *url;       // for URL
    const char *wifi_ssid; // for WIFI
    const char *wifi_key;  // for WIFI (password)
} nfc_default_record_t;

// NFC event callback type
typedef void (*nfc_event_cb_t)(nfc_event_t event, const uint8_t *data, size_t len, void *arg);

/**
 * @brief Initialize the NFC subsystem.
 *
 * @return NFC_OK on success.
 */
nfc_err_t nfc_init();

/**
 * @brief Initialize NFC with an optional default NDEF record written after tag formatting.
 *        Falls back to basic init if def is NULL or type is NFC_DEFAULT_NONE.
 */
nfc_err_t nfc_init_with_default(const nfc_default_record_t *def);

/**
 * @brief Deinitialize the NFC subsystem.
 */
void nfc_deinit();

/**
 * @brief Check if NFC subsystem is initialized and RF operations enabled.
 *
 * @return true if initialized, false otherwise.
 */
bool nfc_ready();

/**
 * @brief Convert NFC error code to string.
 *
 * @param err NFC error code.
 * @return Pointer to error string.
 */
const char *nfc_err_to_string(nfc_err_t err);

/**
 * @brief Register an application callback to receive NFC events.
 */
void nfc_set_event_callback(nfc_event_cb_t cb, void *arg);

/**
 * @brief Start the NFC polling task.
 *
 * @return NFC_OK on success.
 */
nfc_err_t nfc_start();

/**
 * @brief Stop the NFC polling task.
 */
void nfc_stop();

/**
 * @brief Read raw tag memory into buffer.
 *
 * @param buffer destination buffer.
 * @param len    number of bytes to read.
 * @return NFC_OK on success.
 */
nfc_err_t nfc_read(uint8_t *buffer, size_t len);

/**
 * @brief Write raw tag memory from buffer.
 *
 * @param buffer source buffer.
 * @param len    number of bytes to write.
 * @return NFC_OK on success.
 */
nfc_err_t nfc_write(const uint8_t *buffer, size_t len);

/**
 * @brief Read NDEF records from the tag.
 *
 * @param records Array to store NDEF record pointers (allocated by this function)
 * @param count   Number of records found
 * @return NFC_OK on success.
 */
nfc_err_t nfc_get_records(nfc_ndef_record_t **records, size_t *count);

/**
 * @brief Decode a text NDEF record into a UTF-8 string.
 *
 * @param record    NDEF record to decode (must be of type "text")
 * @param out_text  Pointer to output string (allocated by this function, must be freed by caller)
 * @param out_lang  Pointer to output language code string (allocated by this function, must be freed by caller)
 * @return NFC_OK on success, or NFC_ERR_READ if the record is not a valid text record.
 */
nfc_err_t nfc_decode_text_record(const nfc_ndef_record_t *record, char **out_text, char **out_lang);

/**
 * @brief Free NDEF records allocated by nfc_get_records.
 *
 * @param records Array of NDEF record pointers
 * @param count   Number of records
 */
void nfc_free_records(nfc_ndef_record_t *records, size_t count);

/**
 * @brief Clear/reset the NDEF data on the tag.
 *
 * @return NFC_OK on success.
 */
nfc_err_t nfc_clear();

/**
 * @brief Write a simple text record to the tag.
 *
 * @param text Text content to write
 * @return NFC_OK on success.
 */
nfc_err_t nfc_write_text(const char *text);

/** Write a WiFi configuration NDEF record (SSID + password). */
nfc_err_t nfc_write_wifi_config(const char *ssid, const char *password);

/** Write a URL (URI) NDEF record. Full URL including scheme (http/https). */
nfc_err_t nfc_write_url(const char *url);

#ifdef __cplusplus
}
#endif