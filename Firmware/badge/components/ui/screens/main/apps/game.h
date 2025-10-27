#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * @brief Create the badge game UI
 * @param parent The parent LVGL object
 */
void game_app_create(lv_obj_t *parent);

/**
 * @brief Request a refresh of the game data from the API
 * This will update the display with the latest topology data
 */
void game_app_refresh(void);

#ifdef __cplusplus
}
#endif