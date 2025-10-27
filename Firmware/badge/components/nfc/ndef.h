#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Status / errors for NDEF operations
 */
typedef enum {
    NDEF_OK            = 0,
    NDEF_ERR_NO_NDEF   = 1,
    NDEF_ERR_MALFORMED = 2,
    NDEF_ERR_NOMEM     = 3,
} ndef_status_t;

/**
 * @brief TNF values (3-bit)
 */
typedef enum {
    NDEF_TNF_EMPTY        = 0x00,
    NDEF_TNF_WELL_KNOWN   = 0x01,
    NDEF_TNF_MIME_MEDIA   = 0x02,
    NDEF_TNF_ABSOLUTE_URI = 0x03,
    NDEF_TNF_EXTERNAL     = 0x04,
    NDEF_TNF_UNKNOWN      = 0x05,
    NDEF_TNF_UNCHANGED    = 0x06,
    NDEF_TNF_RESERVED     = 0x07,
} ndef_tnf_t;

/**
 * @brief One NDEF record (owns all pointers it sets)
 */
typedef struct {
    ndef_tnf_t tnf;
    uint8_t *type;
    size_t type_len;
    uint8_t *id;
    size_t id_len;
    uint8_t *payload;
    size_t payload_len;
} ndef_record_t;

/**
 * @brief NDEF message (vector of records)
 */
typedef struct {
    ndef_record_t *records;
    size_t count;
} ndef_message_t;

/**
 * @brief Find NDEF TLV (Type 0x03) inside a Type-5 area (starts at offset 0 with CC E1 xx xx xx).
 *        On success, *ndef points into 'buf' at the NDEF bytes, *ndef_len set.
 *
 * @param buf      Buffer containing Type-5 data (including CC)
 * @param len      Length of buffer
 * @param ndef     Output pointer to NDEF data inside buf
 * @param ndef_len Length of NDEF data
 *
 * @return NDEF_OK on success, NDEF_ERR_NO_NDEF if no NDEF TLV found, NDEF_ERR_MALFORMED if input is malformed
 */
ndef_status_t ndef_type5_find_ndef(const uint8_t *buf, size_t len, const uint8_t **ndef, size_t *ndef_len);

/**
 * @brief Parse an NDEF message (bytes == exactly the message payload from the TLV), producing a heap-owned message.
 *
 * @param bytes Pointer to the NDEF message bytes
 * @param len   Length of the NDEF message
 * @param out   Output pointer to the parsed NDEF message
 *
 * @return NDEF_OK on success, NDEF_ERR_MALFORMED if input is malformed, NDEF_ERR_NOMEM if out of memory
 */
ndef_status_t ndef_parse_message(const uint8_t *bytes, size_t len, ndef_message_t *out);

/**
 * @brief Free a message (records + their inner buffers). Safe on partials.
 *
 * @param msg Pointer to the NDEF message to free
 */
void ndef_free_message(ndef_message_t *msg);

/* Convenience decoders (return 1 on success, 0 if record is not that kind or malformed). */

/**
 * @brief Decode an NDEF text record (TNF=Well Known, Type="T"). Writes a NUL-terminated UTF-8 string into *out (malloc).
 *        Caller must free(*out). Language code is skipped. UTF-16 not supported (function returns 0).
 *
 * @param rec NDEF record to decode
 * @param out Output pointer to the decoded string (malloc)
 *
 * @return 1 on success, 0 if not a text record or malformed
 */
int ndef_decode_text(const ndef_record_t *rec, char **out);

/**
 * @brief Decode an NDEF URI record (TNF=Well Known, Type="U"). Expands the UIC prefix. Writes NUL-terminated string into *out
 * (malloc). Caller must free(*out).
 *
 * @param rec NDEF record to decode
 * @param out Output pointer to the decoded string (malloc)
 *
 * @return 1 on success, 0 if not a URI record or malformed
 */
int ndef_decode_uri(const ndef_record_t *rec, char **out);

/**
 * @brief Build a minimal “empty” Type-5 buffer (CC + empty NDEF TLV + terminator).
 *        Writes up to dst_len bytes; returns number of bytes required (7). If return > dst_len, nothing is written.
 *
 * @param dst     Destination buffer
 * @param dst_len Length of destination buffer
 *
 * @return Number of bytes required (7)
 */
size_t ndef_type5_build_empty(uint8_t *dst, size_t dst_len);

#ifdef __cplusplus
}
#endif
