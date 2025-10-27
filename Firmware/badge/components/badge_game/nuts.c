#include <string.h>
#include "./nuts.h"

const char *get_nut_type_str(nut_type_t type) {
    // clang-format off
    switch (type) {
#define X(val, lbl) \
    case val: \
        return ((val) == NUT_TYPE_TELECOM) ? #val : #val "_HARDWARE";
        NUT_TYPE_LIST
#undef X
        default: return "NUT_TYPE_UNKNOWN";
    }
    // clang-format on
}

const char *get_nut_type_label(nut_type_t type) {
    switch (type) {
#define X(val, lbl) \
    case val: return lbl;
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
#define X(val, lbl)                                                                                 \
    if (strcmp(str, #val) == 0 || strcmp(str, #val "_HARDWARE") == 0 || strcmp(str, #val + 9) == 0) \
        return val;
    NUT_TYPE_LIST
#undef X

    return NUT_TYPE_UNKNOWN;
}