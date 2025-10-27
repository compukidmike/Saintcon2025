#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

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
    API_REDEEM_CODE,
    API_REGISTER,
    API_STATUS,
    API_MESSAGES,
    API_MARK_MESSAGE_READ,
    API_LIST_FILES,
    API_ACHIEVEMENTS,
    API_CTF_HINTS_PASSWORD,
    API_CREDIT_SWAP,
    API_CREDIT_SWAP_STATUS,
    API_TOPOLOGY_DATA,
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
    char *player_name;
    uint8_t team_id;
    char *team_name;
    uint32_t credits;
    uint32_t level;
    char *registered_at;
    bool sorting_hat;
    bool has_messages;
    api_firmware_info_t firmware_info;
} api_status_response_t;

/**
 * @brief Message data
 */
typedef struct {
    int id;
    char *title;
    char *content;
    int priority;
    char *created_at;
} api_message_t;

/**
 * @brief Messages response
 */
typedef struct {
    char *badge_id;
    uint32_t message_count;
    api_message_t *messages;
    char *timestamp;
} api_messages_response_t;

typedef struct {
    char *badge_id;
    int message_id;
    bool success;
    char *timestamp;
} api_mark_message_read_response_t;

/**
 * @brief File information for files available for download
 */
typedef struct {
    char *filename;
    char *sha256;
    char *url;
} api_file_info_t;

typedef struct {
    api_file_info_t *files;
    uint32_t file_count;
} api_list_files_response_t;

/**
 * @brief Konami achievement response
 */
typedef struct {
    bool first_time; // true if this is the first time the achievement has been earned
} api_konami_response_t;

/**
 * @brief Achievements information
 */
typedef enum {
    ACHIEVEMENT_UNKNOWN = -1,
    ACHIEVEMENT_NOT_STARTED,
    ACHIEVEMENT_IN_PROGRESS,
    ACHIEVEMENT_COMPLETED,
    ACHIEVEMENT_FAILED,
} api_achievement_status_t;
typedef struct {
    int id;
    char *short_name;
    char *description;
    bool is_public;
    api_achievement_status_t status;
    char *completed_at;
    // progress_data could be added later if needed and schema is defined
} api_achievement_t;
typedef struct {
    api_achievement_t *achievements;
    uint32_t achievement_count;
} api_achievements_response_t;

/**
 * @brief CTF Hints Password response
 */
typedef struct {
    char *password;
} api_ctf_hints_password_response_t;

/**
 * @brief Credit swap response
 */
typedef struct {
    char *code; // The unique code for the credit swap process
    uint32_t credits;
    char *expires_at;
    char *status;
} api_credit_swap_response_t;

/**
 * @brief Credit swap status response
 */
typedef struct {
    char *code; // The unique code for the credit swap process
    uint32_t credits;
    char *status;
    bool redeemed;
    char *redeemed_at;
    char *redeemed_by;
    char *expires_at;
    char *issued_at;
} api_credit_swap_status_response_t;

/**
 * @brief Topology data structures for the badge game
 */
typedef struct {
    int id;
    char *node_type; // "CORE", "RING1", "RING2", "HQ", "TOWER"
    float x_coord;
    float y_coord;
    int port_count;
    int team_id; // nullable - use -1 for null
    char *color; // nullable - team color if applicable
    bool is_online;
    char *halo_color; // nullable - halo color for towers
    int is_unlocked;  // nullable - use -1 for null, 0/1 for false/true
} api_topology_node_t;

typedef struct {
    int id;
    int node_a_id;
    int node_a_port;
    int node_b_id;
    int node_b_port;
    char *connection_type; // "WIRED"
} api_topology_connection_t;

typedef struct {
    int node_id;
    int port_number;
    int connected;     // 0 or 1
    char *link_status; // "NONE", "PARTIAL", "FULL"
    char *updated_at;
} api_topology_port_status_t;

typedef struct {
    int id;
    int team_id;
    int has_path_to_core; // 0 or 1
    int path_via_tower;   // 0 or 1
    char *path_nodes;     // JSON array string, nullable
    char *updated_at;
    int is_scored_path; // 0 or 1
} api_topology_path_status_t;

typedef struct {
    int team_id;
    int first_node;
    int last_node;
    char *created_at;
} api_topology_team_path_score_t;

typedef struct {
    api_topology_node_t *nodes;
    int node_count;
    api_topology_connection_t *connections;
    int connection_count;
    api_topology_port_status_t *port_status;
    int port_status_count;
    api_topology_path_status_t *path_status;
    int path_status_count;
    api_topology_team_path_score_t *team_path_scores;
    int team_path_score_count;
    char *timestamp;
} api_topology_data_t;

#ifdef __cplusplus
}
#endif
