#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#include "lvgl.h"

// clang-format off
/**
 * @brief Factions
 */
#define FACTION_LIST \
    X(EMBERCLAW,    "Ember Claw",   0xDA3832 /* #DA3832 */, 0xFF0000 /* #FF0000 */, "S:/emberclaw.png",    24) \
    X(AETHERWATCH,  "Aether Watch", 0x00AAE9 /* #00AAE9 */, 0x0080FF /* #0080FF */, "S:/aetherwatch.png",  20) \
    X(VERDANT_PACT, "Verdant Pact", 0x00A359 /* #00A359 */, 0x00C000 /* #00C000 */, "S:/verdant_pact.png", 21) \
    X(IRON_HOWL,    "Iron Howl",    0xEA983E /* #EA983E */, 0xFF4500 /* #FF4500 */, "S:/iron_howl.png",    23) \
    X(DREAM_COIL,   "Dream Coil",   0x862F8B /* #862F8B */, 0x8000C0 /* #8000C0 */, "S:/dream_coil.png",   25) \
    X(DAWN_ACCORD,  "Dawn Accord",  0xFFF34A /* #FFF34A */, 0xFFD700 /* #FFD700 */, "S:/dawn_accord.png",  22)
#undef X
typedef enum {
    FACTION_NONE = -1,
#define X(val, lbl, screen_color, led_color, logo, hq_node) FACTION_##val,
    FACTION_LIST
#undef X
    NUM_FACTIONS
} faction_id_t;
// clang-format on
typedef struct {
    faction_id_t id;         // Faction ID from the enum
    const char *name;        // Faction name
    lv_color_t screen_color; // Color for screen/UI display
    lv_color_t led_color;    // Color optimized for LED display
    const char *logo_path;   // Faction logo path
    uint8_t hq_node;         // ID of the HQ node for this faction
} faction_t;
extern const faction_t faction_info[NUM_FACTIONS];
faction_t get_faction(faction_id_t faction);
faction_id_t get_faction_id(const char *str);
faction_id_t faction_from_team_id(int team_id); // API team_id to faction_id_t
const char *get_faction_name(faction_id_t faction);

#ifdef __cplusplus
}
#endif
