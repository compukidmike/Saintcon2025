#include <ctype.h>
#include <string.h>

#include "./factions.h"

const faction_t faction_info[] = {
#define X(id, name, screen_color, led_color, logo_path, hq_node)                                  \
    {FACTION_##id,                                                                                \
     name,                                                                                        \
     LV_COLOR_MAKE((screen_color >> 16) & 0xFF, (screen_color >> 8) & 0xFF, screen_color & 0xFF), \
     LV_COLOR_MAKE((led_color >> 16) & 0xFF, (led_color >> 8) & 0xFF, led_color & 0xFF),          \
     logo_path,                                                                                   \
     hq_node},
    FACTION_LIST
#undef X
};
faction_t get_faction(faction_id_t faction) {
    if (faction < 0 || faction >= sizeof(faction_info) / sizeof(faction_info[0])) {
        return (faction_t){FACTION_NONE, "Unknown", LV_COLOR_MAKE(0, 0, 0), LV_COLOR_MAKE(0, 0, 0), NULL, 0};
    }
    return faction_info[faction];
}
/**
 * Normalize a faction name for comparison:
 *  - Converts to lowercase
 *  - Strips whitespace
 *  - Expands Æ/æ ligature to "ae"
 */
static void normalize_name(const char *in, char *out, size_t out_len) {
    size_t dst = 0;
    for (size_t src = 0; in[src] != '\0' && dst + 2 < out_len; src++) {
        unsigned char c = (unsigned char)in[src];
        if (isspace(c)) {
            continue;
        }
        if (c == 0xC6 /* 'Æ' */ || c == 0xE6 /* 'æ' */) {
            // Expand ligature into "ae"
            out[dst++] = 'a';
            out[dst++] = 'e';
        } else {
            out[dst++] = (char)tolower(c);
        }
    }
    out[dst] = '\0';
}
faction_id_t get_faction_id(const char *str) {
    // Buffer size should cover worst-case expansion (ligatures → 2 chars)
    char norm_input[64];
    normalize_name(str, norm_input, sizeof(norm_input));

    for (size_t i = 0; i < sizeof(faction_info) / sizeof(faction_info[0]); i++) {
        char norm_name[64];
        normalize_name(faction_info[i].name, norm_name, sizeof(norm_name));
        if (strcmp(norm_name, norm_input) == 0) {
            return faction_info[i].id;
        }
    }
    return FACTION_NONE;
}
/**
 * Map team_id to faction_id_t since our ordering/indexing doesn't match the API
 */
faction_id_t faction_from_team_id(int team_id) {
    if (team_id < 1 || team_id > NUM_FACTIONS) {
        return FACTION_NONE;
    }

    // Manual mapping of team_id to faction_id_t
    switch (team_id) {
        case 1: return FACTION_AETHERWATCH;
        case 2: return FACTION_VERDANT_PACT;
        case 3: return FACTION_DAWN_ACCORD;
        case 4: return FACTION_IRON_HOWL;
        case 5: return FACTION_EMBERCLAW;
        case 6: return FACTION_DREAM_COIL;
        default: return FACTION_NONE;
    }
}
const char *get_faction_name(faction_id_t faction) {
    if (faction < 0 || faction >= sizeof(faction_info) / sizeof(faction_info[0])) {
        return "Unknown";
    }
    return faction_info[faction].name;
}
