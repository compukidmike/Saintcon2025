#include "esp_log.h"

#include "main.h"
#include "theme.h"
#include "ui.h"

#include "main/content.h"
#include "main/status.h"

#include "flows/registration.h"

static const char *TAG = "screens/main";

LV_IMG_DECLARE(sc25_bg);

static lv_obj_t *scr_main  = NULL;
static lv_obj_t *scr_inner = NULL;

lv_obj_t *create_main_screen() {
    static ui_screen_t screen_type = SCREEN_MAIN;

    // Create a root screen object for the main screen
    scr_main = lv_obj_create(NULL);
    lv_obj_set_user_data(scr_main, &screen_type);
    lv_obj_set_size(scr_main, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(scr_main, lv_color_hex(BLACK), LV_PART_MAIN);

    // Add a background image
    lv_obj_t *bg = lv_img_create(scr_main);
    lv_img_set_src(bg, &sc25_bg);
    lv_obj_set_size(bg, LV_HOR_RES, LV_VER_RES);
    lv_obj_center(bg);
    lv_obj_move_background(bg);
    lv_obj_set_style_radius(bg, 7, LV_PART_MAIN);

    // Create an inner screen container on top of the background
    scr_inner = lv_obj_create(scr_main);
    lv_obj_set_size(scr_inner, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(scr_inner, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_scrollbar_mode(scr_inner, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(scr_inner, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr_inner, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr_inner, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(scr_inner, 7, LV_PART_MAIN);

    // Create the status bar
    lv_obj_t *status_bar = create_status_bar(scr_inner);

    // Create the content area
    lv_obj_t *content_area __attribute__((unused)) = create_content_area(scr_inner, status_bar);
    content_mark_layout_dirty();

    return scr_main;
}

static void check_registration_deferred(void *user_data) {
    (void)user_data;
    check_registration();
}

void render_main() {
    ui_state_t state = get_ui_state();

    if (state.screen == SCREEN_MAIN) {
        if (lvgl_lock(portMAX_DELAY, __FILE__, __LINE__)) {
            ESP_LOGD(TAG, "Updating status bar");
            update_status_bar(&state);
            ESP_LOGD(TAG, "Updating content area");
            update_content_area();
            lvgl_unlock(__FILE__, __LINE__);
        }

        // Defer registration check until after this render cycle completes
        lv_async_call(check_registration_deferred, NULL);
    }
}
