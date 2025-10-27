#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "../factions.h"
#include "../nodes.h"
#include "../nuts.h"
#include "api.h"

/**
 * @brief Enhanced game node structure that combines internal and external (API) data
 */
typedef struct {
    // Static data
    node_type_t type;     // Type of the node
    uint8_t id;           // Unique ID for the node
    node_port_t ports[6]; // Array of ports for the node (connections)

    // Dynamic data
    float x_coord;    // X coordinate from API
    float y_coord;    // Y coordinate from API
    int port_count;   // Number of ports from API
    int team_id;      // Team that controls this node (-1 if none)
    char *color;      // Color string from API (e.g., "blue", "red")
    bool is_online;   // Online status from API
    char *halo_color; // Halo color for towers
    int is_unlocked;  // Unlock status (-1 for null, 0/1 for false/true)

    // Additional computed data
    faction_id_t faction; // Mapped faction ID based on team_id from the API
} game_node_info_t;

/**
 * @brief Connection types
 */
typedef enum {
    CONNECTION_TYPE_UNKNOWN,
    CONNECTION_TYPE_WIRED,
    CONNECTION_TYPE_WIRELESS,
} game_connection_type_t;

/**
 * @brief Game connection structure
 */
typedef struct {
    uint8_t node_a;
    uint8_t node_b;
    int node_a_port;
    int node_b_port;
    game_connection_type_t connection_type;
} game_connection_info_t;

/**
 * @brief Link statuses
 */
typedef enum {
    LINK_STATUS_UNKNOWN,
    LINK_STATUS_NONE,
    LINK_STATUS_PARTIAL,
    LINK_STATUS_FULL,
} game_link_status_t;

/**
 * @brief Port status information
 */
typedef struct {
    uint8_t node_id;
    int port_number;
    bool connected;
    game_link_status_t link_status;
    char *updated_at;
} game_port_status_t;

/**
 * @brief Game state type enum
 */
typedef enum {
    GAME_STATE_TYPE_FALLBACK,
    GAME_STATE_TYPE_LIVE,
} game_state_type_t;

/**
 * @brief Game state structure
 */
typedef struct {
    game_node_info_t *nodes;
    int node_count;

    game_connection_info_t *connections;
    int connection_count;

    game_port_status_t *port_status;
    int port_status_count;

    char *last_updated;
    game_state_type_t state_type;
    bool is_valid;
} game_state_t;

/**
 * @brief Initialize the badge game system
 */
void badge_game_init();

/**
 * @brief Update game state from API
 * @return true if update was successful, false otherwise
 */
bool badge_game_update_state();

/**
 * @brief Get the current game state
 * @return pointer to current game state, or NULL if not initialized
 */
const game_state_t *badge_game_get_state();

/**
 * @brief Get node info by node ID
 * @param node_id The ID of the node to find
 * @return pointer to the node info, or NULL if not found
 */
const game_node_info_t *badge_game_get_node(uint8_t node_id);

/**
 * @brief Check if game state is valid and up to date
 * @return true if valid, false if needs update
 */
bool badge_game_state_ok();

/**
 * @brief Free and cleanup game state
 */
void badge_game_cleanup();

#ifdef __cplusplus
}
#endif
