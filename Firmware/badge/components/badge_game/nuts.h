#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// clang-format off
/**
 * @brief Nut types
 */
#define NUT_TYPE_LIST \
    X(NUT_TYPE_TELECOM,     "Telecom")      \
    X(NUT_TYPE_COMMUNITY,   "Community")    \
    X(NUT_TYPE_CONTEST,     "Contest")      \
    X(NUT_TYPE_EVENT,       "Event")        \
    X(NUT_TYPE_VENDOR,      "Vendor")       \
    X(NUT_TYPE_MISC,        "Misc")         \
    X(NUT_TYPE_SORTING_HAT, "Sorting Hat")
#undef X
typedef enum {
    NUT_TYPE_UNKNOWN = -1,
#define X(val, lbl) val,
    NUT_TYPE_LIST
#undef X
    NUM_NUT_TYPES
} nut_type_t;
// clang-format on
const char *get_nut_type_str(nut_type_t type);
const char *get_nut_type_label(nut_type_t type);
nut_type_t get_nut_type_from_str(const char *str);

#ifdef __cplusplus
}
#endif
