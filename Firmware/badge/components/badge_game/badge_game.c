#include "badge_game.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "badge_game";

// Global game state
static game_state_t game_state     = {0};
static game_state_t fallback_state = {0};
static bool game_initialized       = false;

/**
 * @brief Map API node type string to internal node_type_t enum
 */
static node_type_t map_node_type(const char *api_type) {
    if (api_type == NULL) {
        return NODE_TYPE_NONE;
    }

    if (strcmp(api_type, "CORE") == 0) {
        return NODE_TYPE_CORE;
    }
    if (strcmp(api_type, "RING1") == 0) {
        return NODE_TYPE_RING;
    }
    if (strcmp(api_type, "RING2") == 0) {
        return NODE_TYPE_RING;
    }
    if (strcmp(api_type, "HQ") == 0) {
        return NODE_TYPE_HQ;
    }
    if (strcmp(api_type, "TOWER") == 0) {
        return NODE_TYPE_TOWER;
    }

    return NODE_TYPE_NONE;
}

/**
 * @brief Free game state memory
 */
static void free_game_state(game_state_t *state) {
    if (state == NULL) {
        return;
    }

    if (state->nodes) {
        for (int i = 0; i < state->node_count; i++) {
            if (state->nodes[i].color) {
                free(state->nodes[i].color);
            }
            if (state->nodes[i].halo_color) {
                free(state->nodes[i].halo_color);
            }
        }
        free(state->nodes);
        state->nodes = NULL;
    }

    if (state->port_status) {
        for (int i = 0; i < state->port_status_count; i++) {
            free(state->port_status[i].updated_at);
        }
        free(state->port_status);
        state->port_status = NULL;
    }

    if (state->last_updated) {
        free(state->last_updated);
    }
    state->last_updated = NULL;

    state->node_count        = 0;
    state->connection_count  = 0;
    state->port_status_count = 0;
    state->is_valid          = false;
}

/**
 * @brief Create fallback state from static node definitions
 */
static bool create_fallback_state() {
    // Free any existing fallback state
    free_game_state(&fallback_state);

    // Copy nodes from static data
    fallback_state.node_count = NUM_NODES;
    fallback_state.nodes      = malloc(sizeof(game_node_info_t) * NUM_NODES);
    if (fallback_state.nodes == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for fallback nodes");
        return false;
    }

    for (int i = 0; i < NUM_NODES; i++) {
        const node_t *static_node   = &nodes[i];
        game_node_info_t *game_node = &fallback_state.nodes[i];

        // Copy basic data
        game_node->id          = static_node->id;
        game_node->type        = static_node->type;
        game_node->x_coord     = 0;
        game_node->y_coord     = 0;
        game_node->port_count  = 6;
        game_node->team_id     = -1;
        game_node->is_online   = false;
        game_node->is_unlocked = false;
        game_node->faction     = FACTION_NONE;
        game_node->color       = NULL;
        game_node->halo_color  = NULL;

        // Copy port connections from static data
        for (int port = 0; port < 6; port++) {
            game_node->ports[port] = static_node->ports[port];
        }
    }

    // Create connections from static node data
    int connection_count = 0;
    game_connection_info_t connections[256];

    // Build connections from port data
    for (int i = 0; i < NUM_NODES; i++) {
        const node_t *node = &nodes[i];
        for (int port = 0; port < 6; port++) {
            if (node->ports[port].state == NODE_PORT_CONNECTED) {
                uint8_t target_id = node->ports[port].uplinks_to;
                // Only add connection once (from lower ID to higher ID to avoid duplicates)
                if (node->id < target_id && connection_count < 256) {
                    connections[connection_count].node_a          = node->id;
                    connections[connection_count].node_b          = target_id;
                    connections[connection_count].node_a_port     = port;
                    connections[connection_count].node_b_port     = -1; // We don't track reverse ports in this simple version
                    connections[connection_count].connection_type = CONNECTION_TYPE_WIRED;
                    connection_count++;
                }
            }
        }
    }

    // Allocate and copy connections
    if (connection_count > 0) {
        fallback_state.connections = malloc(sizeof(game_connection_info_t) * connection_count);
        if (fallback_state.connections != NULL) {
            memcpy(fallback_state.connections, connections, sizeof(game_connection_info_t) * connection_count);
            fallback_state.connection_count = connection_count;
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for fallback connections");
            free_game_state(&fallback_state);
            return false;
        }
    }

    fallback_state.last_updated = NULL;
    fallback_state.state_type   = GAME_STATE_TYPE_FALLBACK;
    fallback_state.is_valid     = true;
    ESP_LOGI(TAG, "Created fallback state with %d nodes and %d connections", fallback_state.node_count,
             fallback_state.connection_count);
    return true;
}

/**
 * @brief Convert API topology data to internal game state
 */
static bool set_state_from_api(const api_topology_data_t *api_data) {
    int64_t start_time = esp_timer_get_time();
    if (api_data == NULL) {
        ESP_LOGE(TAG, "API data is NULL");
        return false;
    }

    // Free existing state
    free_game_state(&game_state);

    // Copy timestamp
    if (api_data->timestamp) {
        game_state.last_updated = strdup(api_data->timestamp);
    }

    // Convert nodes
    if (api_data->node_count > 0 && api_data->nodes) {
        game_state.node_count = api_data->node_count;
        game_state.nodes      = calloc(game_state.node_count, sizeof(game_node_info_t));

        if (!game_state.nodes) {
            ESP_LOGE(TAG, "Failed to allocate memory for nodes");
            return false;
        }

        for (int i = 0; i < game_state.node_count; i++) {
            const api_topology_node_t *api_node = &api_data->nodes[i];
            game_node_info_t *game_node         = &game_state.nodes[i];

            // Copy basic data
            game_node->id          = api_node->id;
            game_node->type        = map_node_type(api_node->node_type);
            game_node->x_coord     = api_node->x_coord;
            game_node->y_coord     = api_node->y_coord;
            game_node->port_count  = api_node->port_count;
            game_node->team_id     = api_node->team_id;
            game_node->is_online   = api_node->is_online;
            game_node->is_unlocked = api_node->is_unlocked;

            // Copy strings
            if (api_node->color) {
                game_node->color = strdup(api_node->color);
            }
            if (api_node->halo_color) {
                game_node->halo_color = strdup(api_node->halo_color);
            }

            // Map faction
            if (api_node->team_id != -1) {
                game_node->faction = faction_from_team_id(api_node->team_id);
            } else {
                game_node->faction = FACTION_NONE;
            }

            // Initialize ports to empty - we'll fill these from connections
            for (int p = 0; p < 6; p++) {
                game_node->ports[p].state      = NODE_PORT_UNAVAILABLE;
                game_node->ports[p].uplinks_to = 0;
            }
        }
    }

    // Convert connections
    if (api_data->connection_count > 0 && api_data->connections) {
        game_state.connection_count = api_data->connection_count;
        game_state.connections      = calloc(game_state.connection_count, sizeof(game_connection_info_t));

        if (!game_state.connections) {
            ESP_LOGE(TAG, "Failed to allocate memory for connections");
            return false;
        }

        for (int i = 0; i < game_state.connection_count; i++) {
            const api_topology_connection_t *api_conn = &api_data->connections[i];
            game_connection_info_t *game_conn         = &game_state.connections[i];

            game_conn->node_a          = api_conn->node_a_id;
            game_conn->node_b          = api_conn->node_b_id;
            game_conn->node_a_port     = api_conn->node_a_port;
            game_conn->node_b_port     = api_conn->node_b_port;
            game_conn->connection_type = (api_conn->connection_type == NULL)                    ? CONNECTION_TYPE_UNKNOWN
                                         : (strcmp(api_conn->connection_type, "WIRED") == 0)    ? CONNECTION_TYPE_WIRED
                                         : (strcmp(api_conn->connection_type, "WIRELESS") == 0) ? CONNECTION_TYPE_WIRELESS
                                                                                                : CONNECTION_TYPE_UNKNOWN;

            // Update node port information
            for (int n = 0; n < game_state.node_count; n++) {
                game_node_info_t *node = &game_state.nodes[n];

                if (node->id == api_conn->node_a_id && api_conn->node_a_port >= 1 && api_conn->node_a_port <= 6) {
                    node->ports[api_conn->node_a_port - 1].state      = NODE_PORT_CONNECTED;
                    node->ports[api_conn->node_a_port - 1].uplinks_to = api_conn->node_b_id;
                }

                if (node->id == api_conn->node_b_id && api_conn->node_b_port >= 1 && api_conn->node_b_port <= 6) {
                    node->ports[api_conn->node_b_port - 1].state      = NODE_PORT_CONNECTED;
                    node->ports[api_conn->node_b_port - 1].uplinks_to = api_conn->node_a_id;
                }
            }
        }
    }

    // Convert port status
    if (api_data->port_status_count > 0 && api_data->port_status) {
        game_state.port_status_count = api_data->port_status_count;
        game_state.port_status       = calloc(game_state.port_status_count, sizeof(game_port_status_t));

        if (!game_state.port_status) {
            ESP_LOGE(TAG, "Failed to allocate memory for port status");
            return false;
        }

        for (int i = 0; i < game_state.port_status_count; i++) {
            const api_topology_port_status_t *api_port = &api_data->port_status[i];
            game_port_status_t *game_port              = &game_state.port_status[i];

            game_port->node_id     = api_port->node_id;
            game_port->port_number = api_port->port_number;
            game_port->connected   = api_port->connected == 1;
            game_port->link_status = (api_port->link_status == NULL)                   ? LINK_STATUS_NONE
                                     : (strcmp(api_port->link_status, "PARTIAL") == 0) ? LINK_STATUS_PARTIAL
                                     : (strcmp(api_port->link_status, "FULL") == 0)    ? LINK_STATUS_FULL
                                                                                       : LINK_STATUS_NONE;

            if (api_port->updated_at) {
                game_port->updated_at = strdup(api_port->updated_at);
            }
        }
    }

    game_state.state_type = GAME_STATE_TYPE_LIVE;
    game_state.is_valid   = true;
    int64_t end_time      = esp_timer_get_time();
    ESP_LOGI(TAG, "Game state updated from API: %d nodes, %d connections, %d port statuses", game_state.node_count,
             game_state.connection_count, game_state.port_status_count);
    ESP_LOGD(TAG, "Time taken to update game state: %lld ms", (end_time - start_time) / 1000);

    return true;
}

void badge_game_init() {
    if (game_initialized) {
        ESP_LOGW(TAG, "Badge game already initialized");
        return;
    }

    memset(&game_state, 0, sizeof(game_state_t));
    memset(&fallback_state, 0, sizeof(game_state_t));

    // Create the persistent fallback state
    if (!create_fallback_state()) {
        ESP_LOGE(TAG, "Failed to create fallback state");
    }

    game_initialized = true;

    ESP_LOGI(TAG, "Badge game initialized");
}

bool badge_game_update_state() {
    if (!game_initialized) {
        ESP_LOGE(TAG, "Badge game not initialized");
        return false;
    }

    ESP_LOGI(TAG, "Updating game state from API...");

    // Call API to get topology data
    api_result_t *result = api_topology_data();
    if (result == NULL) {
        ESP_LOGE(TAG, "Failed to get topology data from API");
        return false;
    }

    if (!result->ok || result->data == NULL) {
        ESP_LOGE(TAG, "API returned error or no data");
        api_free_result(result, true);
        return false;
    }

    const api_topology_data_t *api_data = (const api_topology_data_t *)result->data;
    bool success                        = set_state_from_api(api_data);

    api_free_result(result, true);

    if (success) {
        ESP_LOGI(TAG, "Game state updated successfully");
    } else {
        ESP_LOGE(TAG, "Failed to update game state");
    }

    return success;
}

const game_state_t *badge_game_get_state() {
    if (!game_initialized || !game_state.is_valid) {
        if (!fallback_state.is_valid) {
            return NULL;
        }
        return &fallback_state;
    }
    return &game_state;
}

const game_node_info_t *badge_game_get_node(uint8_t node_id) {
    if (!game_initialized) {
        return NULL;
    }

    // First try the main game state
    if (game_state.is_valid) {
        for (int i = 0; i < game_state.node_count; i++) {
            if (game_state.nodes[i].id == node_id) {
                return &game_state.nodes[i];
            }
        }
    }

    // If not found in main state, try fallback state
    if (fallback_state.is_valid) {
        for (int i = 0; i < fallback_state.node_count; i++) {
            if (fallback_state.nodes[i].id == node_id) {
                return &fallback_state.nodes[i];
            }
        }
    }

    return NULL;
}

bool badge_game_state_ok() {
    return game_initialized && game_state.is_valid;
}

void badge_game_cleanup() {
    if (game_initialized) {
        free_game_state(&game_state);
        free_game_state(&fallback_state);
        game_initialized = false;
        ESP_LOGI(TAG, "Badge game cleaned up");
    }
}
