#include "theme.h"

#define CORNER_TL_WIDTH    20
#define CORNER_TL_HEIGHT   15
#define CORNER_TR_WIDTH    10
#define CORNER_TR_HEIGHT   14
#define CORNER_BL_WIDTH    4
#define CORNER_BL_HEIGHT   4
#define CORNER_BR_WIDTH    32
#define CORNER_BR_HEIGHT   5
#define EDGE_TOP_HEIGHT    15
#define EDGE_BOTTOM_HEIGHT 5
#define EDGE_LEFT_WIDTH    4
#define EDGE_RIGHT_WIDTH   5

// Button styles
lv_style_t button_style;

lv_style_t button_red;
lv_style_t button_orange;
lv_style_t button_yellow;
lv_style_t button_green;
lv_style_t button_blue;
lv_style_t button_purple;
lv_style_t button_bluegrey;
lv_style_t button_grey;

// Flag to track if the styles have been initialized - we don't want to re-initialize them because it can cause memory leaks
static bool styles_initialized = false;

static lv_theme_t *theme __attribute__((unused));

// Function prototypes for styles
static void style_button_init();

// Initialize the LVGL style
void style_init() {
    if (styles_initialized) {
        return;
    }

    style_button_init();
    styles_initialized = true;
}

// Initialize the LVGL button styles
static void style_button_init() {
    // Base button style
    lv_style_init(&button_style);
    lv_style_set_border_width(&button_style, 2);
    lv_style_set_radius(&button_style, 3);
    lv_style_set_text_color(&button_style, lv_color_hex(WHITE));

    // Normal button style
    // lv_style_set_text_font(&button_style, &open_dyslexic_reg_16);
    // lv_style_set_pad_left(&button_style, 8);
    // lv_style_set_pad_right(&button_style, 8);
    // lv_style_set_pad_top(&button_style, 4);
    // lv_style_set_pad_bottom(&button_style, 4);
    lv_style_set_text_font(&button_style, &white_rabbit_18);
    lv_style_set_pad_hor(&button_style, 10);
    lv_style_set_pad_ver(&button_style, 8);

    // Red button style
    lv_style_init(&button_red);
    lv_style_set_bg_color(&button_red, lv_color_hex(RED_MAIN));
    lv_style_set_border_color(&button_red, lv_color_hex(RED_DIMMER));

    // Orange button style
    lv_style_init(&button_orange);
    lv_style_set_bg_color(&button_orange, lv_color_hex(ORANGE_MAIN));
    lv_style_set_border_color(&button_orange, lv_color_hex(ORANGE_DIMMER));

    // Yellow button style
    lv_style_init(&button_yellow);
    lv_style_set_bg_color(&button_yellow, lv_color_hex(YELLOW_MAIN));
    lv_style_set_border_color(&button_yellow, lv_color_hex(YELLOW_DIMMER));

    // Green button style
    lv_style_init(&button_green);
    lv_style_set_bg_color(&button_green, lv_color_hex(GREEN_MAIN));
    lv_style_set_border_color(&button_green, lv_color_hex(GREEN_DIMMER));

    // Blue button style
    lv_style_init(&button_blue);
    lv_style_set_bg_color(&button_blue, lv_color_hex(BLUE_MAIN));
    lv_style_set_border_color(&button_blue, lv_color_hex(BLUE_DIMMER));

    // Purple button style
    lv_style_init(&button_purple);
    lv_style_set_bg_color(&button_purple, lv_color_hex(PURPLE_MAIN));
    lv_style_set_border_color(&button_purple, lv_color_hex(PURPLE_DIMMER));

    // Blue-grey button style
    lv_style_init(&button_bluegrey);
    lv_style_set_bg_color(&button_bluegrey, lv_color_hex(BLUEGRAY_MAIN));
    lv_style_set_border_color(&button_bluegrey, lv_color_hex(BLUEGRAY_DIMMER));

    // Grey button style
    lv_style_init(&button_grey);
    lv_style_set_bg_color(&button_grey, lv_color_hex(GRAY_BASE));
    lv_style_set_border_color(&button_grey, lv_color_hex(GRAY_SHADE_2));
}
