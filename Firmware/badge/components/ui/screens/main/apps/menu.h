#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void menu_app_create(lv_obj_t *parent);
void menu_update_faction_label();

#ifdef __cplusplus
}
#endif