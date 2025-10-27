#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// In case we need to expose types from the content or status headers
#include "main/content.h"
#include "main/status.h"

lv_obj_t *create_main_screen();

void render_main();

#ifdef __cplusplus
}
#endif
