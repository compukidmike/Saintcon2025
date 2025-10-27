#include "nut/types.h"
#include <string.h>

// ------------------------------------------------------------------------------------------------
// Nut Type Helpers
// ------------------------------------------------------------------------------------------------

const char *get_nut_type_str(nut_type_t type) {
    // clang-format off
    switch (type) {
#define X(short, hw, lbl) \
    case NUT_TYPE_##short: \
        return ((NUT_TYPE_##short) == NUT_TYPE_TELECOM || (NUT_TYPE_##short) == NUT_TYPE_SORTING_HAT) ? #short : #short "_HARDWARE";
        NUT_TYPE_LIST
#undef X
        default: return "NUT_TYPE_UNKNOWN";
    }
    // clang-format on
}
const char *get_nut_type_short_str(nut_type_t type) {
    switch (type) {
#define X(short, hw, lbl) \
    case NUT_TYPE_##short: return #short;
        NUT_TYPE_LIST
#undef X
        default: return "UNKNOWN";
    }
}
const char *get_nut_type_label(nut_type_t type) {
    switch (type) {
#define X(short, hw, lbl) \
    case NUT_TYPE_##short: return lbl;
        NUT_TYPE_LIST
#undef X
        default: return "Unknown";
    }
}
nut_type_t get_nut_type_from_str(const char *str) {
    if (!str) {
        return NUT_TYPE_UNKNOWN;
    }

    // Try exact match with full enum name, _HARDWARE suffix, or without prefix
#define X(short, hw, lbl)                                                   \
    if (strcmp(str, #short) == 0 || strcmp(str, #short "_HARDWARE") == 0 || \
        (strncmp(str, "NUT_TYPE_", sizeof("NUT_TYPE_") - 1) == 0 &&         \
         (strcmp(str + sizeof("NUT_TYPE_") - 1, #short) == 0 ||             \
          strcmp(str + sizeof("NUT_TYPE_") - 1, #short "_HARDWARE") == 0))) \
        return NUT_TYPE_##short;
    NUT_TYPE_LIST
#undef X

    return NUT_TYPE_UNKNOWN;
}

// ------------------------------------------------------------------------------------------------
// Node Type Helpers
// ------------------------------------------------------------------------------------------------

const char *get_node_type_str(node_type_t type) {
    switch (type) {
#define X(val, lbl) \
    case NODE_TYPE_##val: return #val;
        NODE_TYPE_LIST
#undef X
        default: return "NODE_TYPE_UNKNOWN";
    }
}

const char *get_node_type_label(node_type_t type) {
    switch (type) {
#define X(val, lbl) \
    case NODE_TYPE_##val: return lbl;
        NODE_TYPE_LIST
#undef X
        default: return "Unknown";
    }
}

node_type_t get_node_type_from_str(const char *str) {
    if (!str) {
        return NODE_TYPE_UNKNOWN;
    }

    // Try exact match with full enum name or without prefix
#define X(val, lbl)             \
    if (strcmp(str, #val) == 0) \
        return NODE_TYPE_##val;
    NODE_TYPE_LIST
#undef X

    return NODE_TYPE_UNKNOWN;
}

// ------------------------------------------------------------------------------------------------
// Code Type Helpers
// ------------------------------------------------------------------------------------------------

const char *get_code_type_str(code_type_t type) {
    // clang-format off
    switch (type) {
#define X(val, lbl) \
    case CODE_TYPE_##val: \
        return #val;
    CODE_TYPE_LIST
#undef X
        default: return "CODE_TYPE_UNKNOWN";
    }
    // clang-format on
}

const char *get_code_type_label(code_type_t type) {
    switch (type) {
#define X(val, lbl) \
    case CODE_TYPE_##val: return lbl;
        CODE_TYPE_LIST
#undef X
        default: return "Unknown";
    }
}

code_type_t get_code_type_from_str(const char *str) {
    if (!str) {
        return CODE_TYPE_UNKNOWN;
    }

    // Try exact match with full enum name or without prefix
#define X(val, lbl)             \
    if (strcmp(str, #val) == 0) \
        return CODE_TYPE_##val;
    CODE_TYPE_LIST
#undef X

    return CODE_TYPE_UNKNOWN;
}