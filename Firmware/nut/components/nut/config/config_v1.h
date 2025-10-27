#pragma once

typedef struct {
    uint32_t version;
    nut_hw_type_t hw_type;
    nut_type_t type;
    char nut_id[64];
    bool enabled;
    int16_t node_id;
} nut_config_v1_t;
// clang-format off
#define NUT_DEFAULTS_V1                             \
    (nut_config_v1_t){                              \
        .version          = 1,                      \
        .hw_type          = NUT_HW_TYPE_UNKNOWN,    \
        .type             = NUT_TYPE_UNKNOWN,       \
        .nut_id           = "",                     \
        .enabled          = true,                   \
        .node_id         = -1,                      \
    }
// clang-format on
