#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

// The total number of nodes we will have in the game
//   - 1 Core node
//   - 6 Ring 1 nodes
//   - 12 Ring 2 nodes
//   - 6 Faction HQ nodes
//   - 6 Tower nodes
#define NUM_NODES 31

/**
 * @brief Node data types
 */

typedef enum {
    NODE_TYPE_NONE,  // No type - any real/active node should not have this set
    NODE_TYPE_CORE,  // The one and only center core node
    NODE_TYPE_RING,  // The bulk of the standard ring nodes
    NODE_TYPE_HQ,    // Faction HQ nodes - each team/faction has to establish a link from here through other nodes to the core
    NODE_TYPE_TOWER, // Special nodes representing the towers onsite that will have challenges to unlock access to them
} node_type_t;
typedef enum {
    NODE_STATUS_NONE,
    NODE_STATUS_ONLINE,
    NODE_STATUS_OFFLINE,
} node_status_t;
typedef enum {
    NODE_LOCK_NONE,     // No lock - node is free to use
    NODE_LOCK_LOCKED,   // Node is locked
    NODE_LOCK_UNLOCKED, // Node is unlocked
} node_lock_t;
typedef enum {
    NODE_PORT_UNAVAILABLE, // Port is unavailable or non-existent (e.g., no connection like nodes in the outer ring)
    NODE_PORT_CONNECTED,   // Port is connected to another node
} node_port_state_t;
typedef struct {
    node_port_state_t state; // State of the port (available/unavailable)
    uint8_t uplinks_to;      // ID of the node this port connects to
} node_port_t;
typedef struct {
    node_type_t type;     // Type of the node
    uint8_t id;           // Unique ID for the node
    node_status_t status; // Status of the node
    node_lock_t lock;     // Lock status of the node
    node_port_t ports[6]; // Array of ports for the node
} node_t;

extern node_t nodes[NUM_NODES];

#ifdef __cplusplus
}
#endif
