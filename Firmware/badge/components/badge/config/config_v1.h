#pragma once

typedef struct {
    uint32_t version;
    bool hw_pass;                         // Initial hardware tests passed
    bool registered;                      // User has registered
    uint8_t screen_brightness;            // Display brightness preference
    uint32_t screen_timeout;              // Screen timeout in seconds
    bool faction_leds;                    // Whether to show faction LED pattern
    char player_name[PLAYER_NAME_LENGTH]; // Player name
    uint8_t team_id;                      // Team ID
    char team_name[TEAM_NAME_LENGTH];     // Team name
    int credits;                          // User credits
    int level;                            // User level
    bool sorting_hat;                     // Whether the player has done the sorting hat
} badge_config_v1_t;
// clang-format off
#define BADGE_DEFAULTS_V1                      \
    (badge_config_v1_t){                       \
        .version          = 1,                 \
        .hw_pass          = false,             \
        .registered       = false,             \
        .screen_brightness = LCD_BACKLIGHT_ON, \
        .screen_timeout   = 30,                \
        .faction_leds     = true,              \
        .player_name      = "",                \
        .team_id          = 0,                 \
        .team_name        = "",                \
        .credits          = 0,                 \
        .level            = 0,                 \
        .sorting_hat      = false,             \
    }
// clang-format on
