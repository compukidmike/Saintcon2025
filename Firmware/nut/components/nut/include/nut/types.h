#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ------------------------------------------------------------------------------------------------
// Nut types and data
// ------------------------------------------------------------------------------------------------

/**
 * @brief Nut hardware types
 */
typedef enum {
    NUT_HW_TYPE_UNKNOWN = 0,
    NUT_HW_TYPE_TELECOM, // Has MCP23017 I/O expander + WizNet 5500 Lite - stationary terminal
    NUT_HW_TYPE_WIRELESS // No I/O expander - wireless mobile node
} nut_hw_type_t;

// clang-format off
/**
 * @brief Nut types
 */
#define NUT_TYPE_LIST \
    X(TELECOM,      TELECOM,            "Telecom")      \
    X(COMMUNITY,    COMMUNITY_HARDWARE, "Community")    \
    X(VENDOR,       VENDOR_HARDWARE,    "Vendor")       \
    X(CONTEST,      CONTEST_HARDWARE,   "Contest")      \
    X(EVENT,        EVENT_HARDWARE,     "Event")        \
    X(GARBAGE,      GARBAGE_HARDWARE,   "Garbage")      \
    X(SORTING_HAT,  SORTING_HAT,        "Sorting Hat")
#undef X
typedef enum {
    NUT_TYPE_UNKNOWN = -1,
#define X(short, hw, lbl) NUT_TYPE_##short,
    NUT_TYPE_LIST
#undef X
    NUT_TYPE_COUNT,
} nut_type_t;
// clang-format on
const char *get_nut_type_str(nut_type_t type);
const char *get_nut_type_short_str(nut_type_t type);
const char *get_nut_type_label(nut_type_t type);
nut_type_t get_nut_type_from_str(const char *str);

// ------------------------------------------------------------------------------------------------
// Node types and data
// ------------------------------------------------------------------------------------------------

// The total number of nodes we will have in the game
//   - 1 Core node
//   - 6 Ring 1 nodes
//   - 12 Ring 2 nodes
//   - 6 Faction HQ nodes
//   - 6 Tower nodes
#define NUM_NODES 31

// clang-format off
/**
 * @brief Node data types
 */
#define NODE_TYPE_LIST  \
    X(NONE,      "None") \
    X(CORE,      "Core") \
    X(RING1,     "Ring 1") \
    X(RING2,     "Ring 2") \
    X(HQ,        "HQ")   \
    X(TOWER,     "Tower")
#undef X
typedef enum {
    NODE_TYPE_UNKNOWN = -1,
#define X(val, lbl) NODE_TYPE_##val,
    NODE_TYPE_LIST
#undef X
    NODE_TYPE_MAX
} node_type_t;
// clang-format on
const char *get_node_type_str(node_type_t type);
const char *get_node_type_label(node_type_t type);
node_type_t get_node_type_from_str(const char *str);
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

// ------------------------------------------------------------------------------------------------
// Code types
// ------------------------------------------------------------------------------------------------
// clang-format off
#define CODE_TYPE_LIST              \
    X(COMMUNITY,    "Community")    \
    X(VENDOR,       "Vendor")       \
    X(CONTEST,      "Contest")      \
    X(EVENT,        "Event")        \
    X(GARBAGE,      "Garbage")      \
    X(LEVELUP,      "Level Up")     \
    X(CREDITSWAP,   "Credit Swap")  \
    X(UNLOCK,       "Unlock")       \
    X(IDENTITY,     "Identity")     \
    X(SORTING_HAT,  "Sorting Hat")
#undef X
typedef enum {
    CODE_TYPE_UNKNOWN = -1,
#define X(val, lbl) CODE_TYPE_##val,
    CODE_TYPE_LIST
#undef X
    CODE_TYPE_MAX
} code_type_t;
// clang-format on
const char *get_code_type_str(code_type_t type);
const char *get_code_type_label(code_type_t type);
code_type_t get_code_type_from_str(const char *str);

#ifdef __cplusplus
}
#endif