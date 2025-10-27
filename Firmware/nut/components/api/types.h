#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <time.h>
#include "nut/types.h"

// Special value to indicate "no timestamp" / NULL timestamp
#define API_TIMESTAMP_NULL ((time_t)-1)

#ifdef __cplusplus
enum class api_err_t {
    API_OK   = 0, // API call was successful
    API_FAIL = -1 // API call failed - generic error
};
#else
typedef enum {
    API_OK   = 0, // API call was successful
    API_FAIL = -1 // API call failed - generic error
} api_err_t;
#endif

// -----------------------------------------------------------------------------
// API response data
// -----------------------------------------------------------------------------

typedef enum {
    API_BASE, // Base API endpoint that just has an empty result field
    API_PROVISION,
    API_PROVISION_PSK,
    API_FIRMWARE_HEAD,
    API_FIRMWARE_DATA,
    API_STATUS,
    API_REQUEST_CODE,
    API_CODE_STATUS,
} api_result_type_t;

typedef struct {
    api_result_type_t type;
    bool ok;
    char *detail;
    void *data;
} api_result_t;

/**
 * @brief Provision response
 */
typedef struct {
    uint8_t hmac_secret[32];
    uint8_t hash_prefix[16];
    uint8_t ctf_hmac_secret[32];
    uint8_t ctf_hash_prefix[16];
} api_provision_response_t;

/**
 * @brief Wireless PSK provision response
 */
typedef struct {
    char psk[64]; // The PSK to write to the secure element
} api_provision_psk_response_t;

/**
 * @brief Badge firmware result for the /badge/firmware_version endpoint
 */
typedef enum {
    API_DEVICE_TYPE_UNKNOWN = 0, // The API doesn't have a string for this but this can be a default value
    API_DEVICE_TYPE_BADGE,
    API_DEVICE_TYPE_NUT,
} api_device_type_t;

typedef struct {
    api_device_type_t device_type;
    char *setting; // Setting name (e.g. "firmware_version_badge")
    char *value;   // The latest firmware version (should be a semver string)
} api_firmware_data_t;

/**
 * @brief Firmware information from headers in HEAD requests
 */
typedef struct {
    char *content_type;     // MIME type of the firmware file
    uint64_t firmware_size; // Size of the firmware file in bytes
    char *firmware_version; // Current firmware version
    api_device_type_t device_type;
    char *last_modified;
} api_firmware_head_t;

/**
 * @brief Status firmware information
 */
typedef struct {
    char *current_version;
    char *latest_version;
    bool update_available;
} api_firmware_info_t;

/**
 * @brief Status response
 */
typedef struct {
    char *nut_id;
    nut_type_t nut_type;
    bool enabled;
    bool provisioned;
    time_t provisioned_at; // Unix timestamp, or API_TIMESTAMP_NULL if not set
    time_t last_seen;      // Unix timestamp, or API_TIMESTAMP_NULL if not set
    api_firmware_info_t firmware_info;
    int16_t vendor_id;
    int16_t community_id;
    int16_t contest_id;
    int16_t event_id;
    int16_t garbage_id;
    int16_t system_id;
    int16_t node_id;
} api_status_response_t;

/**
 * @brief Request code response
 */
typedef struct {
    char *code;            // The code that was generated
    code_type_t code_type; // The type of code (e.g. "COMMUNITY", "CONTEST")
    time_t expires_at;     // Expiration timestamp (Unix time), or API_TIMESTAMP_NULL if not set
} api_request_code_response_t;

/**
 * @brief Code status response
 */
typedef enum {
    CODE_STATUS_UNKNOWN = 0,
    CODE_STATUS_PENDING,
    CODE_STATUS_VALID,
    CODE_STATUS_FAILED,
    CODE_STATUS_EXPIRED,
    CODE_STATUS_INVALIDATED,
} code_status_t;

// Allowed syslog levels via the API
#define SYSLOG_LEVEL_LIST \
    X(INFO)               \
    X(WARN)               \
    X(ERROR)              \
    X(DEBUG)
typedef enum {
    SYSLOG_LEVEL_NONE = -1,
#define X(name) SYSLOG_LEVEL_##name,
    SYSLOG_LEVEL_LIST
#undef X
        SYSLOG_LEVEL_MAX
} syslog_level_t;
extern const char *const syslog_level_strs[];
static inline const char *syslog_level_to_string(syslog_level_t level) {
    return syslog_level_strs[level];
}

typedef struct {
    char *code;                    // The code that was generated
    code_type_t code_type;         // The type of code (e.g. "COMMUNITY", "CONTEST")
    code_status_t status;          // Current status of the code - comes in as string (e.g. "pending", "valid", "expired")
    time_t issued_at;              // Issuance timestamp (Unix time), or API_TIMESTAMP_NULL if not set
    time_t expires_at;             // Expiration timestamp (Unix time), or API_TIMESTAMP_NULL if not set
    bool redeemed;                 // True if the code has been redeemed
    time_t redeemed_at;            // Redemption timestamp (Unix time), or API_TIMESTAMP_NULL if not redeemed
    char *redeemed_by_badge_id;    // Player badge ID who redeemed the code, or NULL if not redeemed
    bool invalidated;              // True if the code was invalidated
    char *invalidated_reason;      // Reason the code was invalidated, or NULL if not invalidated
    char *nut_id;                  // Nut ID that issued the code
    char *issued_by_nut_id;        // Nut ID that issued the code
    nut_type_t issued_by_nut_type; // Nut type that issued the code
    int node_id;                   // Node ID that issued the code
    node_type_t node_type;         // Node type that issued the code
    time_t unlocked_at;            // Unlock timestamp (Unix time), or API_TIMESTAMP_NULL if not unlocked
    char *unlocked_by_badge_id;    // Badge ID of the player who unlocked the node
    int team_id;                   // Team ID of the player who unlocked the node
    int credits_deducted;          // Number of credits deducted when the code was issued
    int remaining_credits;         // Remaining credits
    int node_team_id_at_unlock;    // Team ID that controlled the node at the time of unlock
    char *failure_reason;          // Reason the code failed, or NULL if it didn't fail
    time_t failed_at;              // Timestamp when the code failed (Unix time), or API_TIMESTAMP_NULL if didn't fail
    char *attempted_by_badge_id;   // Badge ID of the player who attempted to use the code, or NULL if it didn't fail
} api_code_status_response_t;

/**
 * Telecom API types
 */

typedef struct {
    uint8_t port_number;
    bool connected;
} api_telecom_port_status_t;

#ifdef __cplusplus
}
#endif
