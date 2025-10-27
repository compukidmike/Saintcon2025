// Legacy timer callbacks removed after refactor – declarations kept out.
#include <math.h>
#include <memory.h>
#include "esp_log.h"
#include "esp_random.h"
#include "driver/gpio.h"

#include "hwtest.h"
#include "ui.h"
#include "theme.h"
#include "nav.h"

// Includes for things to test
#include "badge.h"
#include "i2c_manager.h"
#include "input.h"
#include "secure_element.h"
#include "nfc.h"
#include "led.h"
#include "battery.h"
#include "esp_timer.h"

static const char *TAG = "screens/hwtest";

// When hardware test has not yet been officially passed we show a static white pattern
#define LED_HWTEST_WHITE_BRIGHTNESS 10

// Types of tests
typedef enum {
    HWTEST_NONE,         // No test selected
    HWTEST_JOYSTICK,     // Input - joystick / d-pad
    HWTEST_CHIPS,        // Basic I2C comms (Crypto + NFC)
    HWTEST_CRYPTO_SETUP, // Secure element provisioning / lock state
    HWTEST_RGB_LEDS,     // RGB LED patterns
    HWTEST_AI_INHIBIT,   // AI Inhibitor
    HWTEST_AI_SECURITY,  // AI Security Strip
} hwtest_t;

// Strings for the hardware test names
static const char *hwtest_labels[] = {
    "None",         //
    "Joystick",     //
    "Chip Comms",   //
    "Crypto Setup", //
    "RGB LEDs",     //
    "AI Inhibitor", //
    "AI Security",  //
};
#define HWTEST_COUNT (sizeof(hwtest_labels) / sizeof(hwtest_labels[0]))

// Test result states
typedef enum {
    HWTEST_RESULT_NONE,
    HWTEST_RESULT_PENDING,
    HWTEST_RESULT_PASSED,
    HWTEST_RESULT_FAILED,
} hwtest_result_t;

// Track the state of each hardware test
static hwtest_result_t hwtest_results[HWTEST_COUNT] = {HWTEST_RESULT_NONE};

// Button types to handle
typedef enum {
    HWTEST_BTN_PASS,
    HWTEST_BTN_FAIL,
} hwtest_btn_t;

static uint32_t hwtest_loaded_ms = 0;

static lv_obj_t *scr_hwtest       = NULL;
static lv_obj_t *hwtest_container = NULL;
static lv_obj_t *hwtest_list      = NULL;
static lv_style_t container_style;
static lv_style_t test_button_style;
static lv_style_t pass_main;
static lv_style_t pass_light;
static lv_style_t pass_focused;
static lv_style_t fail_main;
static lv_style_t fail_light;
static lv_style_t fail_focused;
static lv_style_t pending_main;
static lv_style_t pending_light;
static lv_style_t pending_focused;
static lv_style_t button_row;

// Test overlay objects
static lv_obj_t *joystick_overlay     = NULL;
static lv_obj_t *rgb_led_overlay      = NULL;
static lv_obj_t *crypto_setup_overlay = NULL;
static lv_timer_t *crypto_bg_timer    = NULL;

static bool styles_initialized = false;

// Navigation
lv_indev_t *hwtest_indev  = NULL;
lv_group_t *hwtest_group  = NULL;
nav_ctx_t *hwtest_nav_ctx = NULL;

// Chip test objects
typedef enum {
    HWTEST_CHIP_CRYPTO,
    HWTEST_CHIP_NFC,
} chips_t;
static const char *chips[] = {"Crypto", "NFC"};
#define CHIP_TEST_COUNT (sizeof(chips) / sizeof(chips[0]))
static chips_t chip_test_current                          = HWTEST_CHIP_CRYPTO;
static hwtest_result_t chip_test_results[CHIP_TEST_COUNT] = {HWTEST_RESULT_NONE};

static nav_scope_guard_t chip_test_nav_guard;
static nav_ctx_t *chip_test_nav_ctx    = NULL;
static lv_group_t *chip_test_nav_group = NULL;
static lv_obj_t *chip_comms_overlay    = NULL;
static lv_obj_t *chip_test_list        = NULL;

// Joystick test objects
static nav_scope_guard_t joystick_nav_guard;
static nav_ctx_t *joystick_nav_ctx         = NULL;
static lv_group_t *joystick_nav_group      = NULL;
static lv_obj_t *joystick_up_indicator     = NULL;
static lv_obj_t *joystick_down_indicator   = NULL;
static lv_obj_t *joystick_left_indicator   = NULL;
static lv_obj_t *joystick_right_indicator  = NULL;
static lv_obj_t *joystick_center_indicator = NULL;
static bool joystick_test_complete[5]      = {false}; // up, down, left, right, center

// Crypto setup test objects
static lv_style_t crypto_btn;
static lv_style_t crypto_btn_focused;

// RGB LED test state
static nav_scope_guard_t rgb_led_nav_guard;
static int current_brightness          = 64; // 25% default brightness
static int last_applied_pattern        = -1;
static lv_timer_t *rgb_animation_timer = NULL;
// Layered LED system integration
static int hwtest_led_layer_handle = -1;

typedef enum {
    HWTEST_LED_MODE_NONE = 0,
    HWTEST_LED_MODE_STATIC_WHITE,
    HWTEST_LED_MODE_CHASE,
    HWTEST_LED_MODE_BREATH,
    HWTEST_LED_MODE_STATIC_FRAME, // generic static pattern stored in buffer
} hwtest_led_mode_t;

typedef struct {
    hwtest_led_mode_t mode;
    // Shared timing
    uint32_t last_update_ms;
    // Chase state
    int chase_pos;
    bool chase_forward;
    int color_offset;
    int last_chase_positions[5];
    int last_chase_count;
    // Breath state
    int breath_step;
    bool breathing_up;
    uint32_t frame_counter; // generic per-mode frame counter
    // Static frame buffer
    uint8_t static_frame[NUM_LEDS][3];
    int static_frame_pattern_id; // pattern id that produced this frame
} hwtest_led_state_t;

static hwtest_led_state_t hwtest_led_state = {.mode = HWTEST_LED_MODE_NONE};

static inline uint32_t hwtest_now_ms() {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void hwtest_led_render_cb(uint32_t now_ms, void *user_ctx);

// RGB LED test objects
static nav_ctx_t *rgb_led_nav_ctx         = NULL;
static lv_group_t *rgb_led_nav_group      = NULL;
static lv_obj_t *rgb_led_list             = NULL;
static lv_obj_t *brightness_slider        = NULL;
static lv_obj_t *brightness_percent_label = NULL;
static lv_style_t rgb_brightness_slider;
static lv_style_t rgb_brightness_slider_indicator;
static lv_style_t rgb_brightness_slider_knob;
static lv_style_t rgb_brightness_slider_focused;
static lv_style_t rgb_brightness_slider_editing;
static lv_style_t rgb_list;
static lv_style_t rgb_list_button;
static lv_style_t rgb_list_button_focused;
static lv_style_t rgb_back_button;
static lv_style_t rgb_back_button_focused;

// Set this to 1 to force API/WiFi stages to show pending for development/testing
#define HWTEST_FORCE_PENDING 0

// Main function prototypes
static void hwtest_initialize_styles();
static void hwtest_update_theme();
static void hwtest_render_list(lv_obj_t *parent);
static void hwtest_list_event_cb(lv_event_t *e);
static void hwtest_button_event_cb(lv_event_t *e);
static void hwtest_set_result(hwtest_t test, hwtest_result_t result);
static void hwtest_screen_delete_event_cb(lv_event_t *e);
static void hwtest_screen_loaded_event_cb(lv_event_t *e);

// Quick test functions
static void hwtest_quick_tests();

// Chip test functions
static void hwtest_start_chip_test(lv_obj_t *parent);
static void chip_test_run_next(lv_timer_t *timer);
static void chip_test_render_results();
static void chip_test_continue_event_cb(lv_event_t *e);
static bool test_crypto_comms();
static bool test_nfc_comms();

// Crypto setup (provisioning status) test functions
static void hwtest_start_crypto_setup_test(lv_obj_t *parent);
static void crypto_setup_poll_timer_cb(lv_timer_t *timer);
static void crypto_setup_close_event_cb(lv_event_t *e);
static void crypto_bg_poll_timer_cb(lv_timer_t *timer);

// Joystick test functions
static void hwtest_start_joystick_test(lv_obj_t *parent);
static void joystick_key_event_cb(lv_event_t *e);
static void joystick_test_end_timer_cb(lv_timer_t *timer);
static void joystick_test_end();

// RGB LED test functions
static void hwtest_start_rgb_led_test(lv_obj_t *parent);
static void rgb_led_pattern_event_cb(lv_event_t *e);
static void rgb_led_brightness_event_cb(lv_event_t *e);
static void rgb_led_apply_pattern(int pattern);
static void rgb_led_update_active_indicator(int active_pattern);
static void rgb_led_start_chase_pattern();
static void rgb_led_start_breathing_pattern();
static void rgb_led_test_end();
static void hwtest_apply_static_pattern(int pattern_id);

static void hwtest_initialize_styles() {
    if (styles_initialized) {
        return;
    }

    // Initialize the flex container base style
    lv_style_init(&container_style);
    lv_style_set_bg_color(&container_style, lv_color_hex(BLACK));
    lv_style_set_border_color(&container_style, lv_color_hex(GRAY_BASE));
    lv_style_set_text_color(&container_style, lv_color_hex(WHITE));
    lv_style_set_border_width(&container_style, 2);
    lv_style_set_radius(&container_style, 1);
    lv_style_set_size(&container_style, LV_HOR_RES, LV_VER_RES);

    // Test button style
    lv_style_init(&test_button_style);
    lv_style_set_border_width(&test_button_style, 2);
    lv_style_set_radius(&test_button_style, 3);
    lv_style_set_text_color(&test_button_style, lv_color_hex(WHITE));

    // Make the font look centered for hardware test buttons
    lv_style_set_text_font(&test_button_style, &clarity_16);
    lv_style_set_pad_left(&test_button_style, 12);
    lv_style_set_pad_right(&test_button_style, 8);
    lv_style_set_pad_top(&test_button_style, 7);
    lv_style_set_pad_bottom(&test_button_style, 10);

    // Initialize the main pass style
    lv_style_init(&pass_main);
    lv_style_set_bg_color(&pass_main, lv_color_hex(GREEN_DIMMEST));
    lv_style_set_border_color(&pass_main, lv_color_hex(GREEN_MAIN));

    // Initialize the light pass style
    lv_style_init(&pass_light);
    lv_style_set_bg_color(&pass_light, lv_color_hex(GREEN_DIM));
    lv_style_set_border_color(&pass_light, lv_color_hex(GREEN_LIGHT));

    // Initialize the focused pass style
    lv_style_init(&pass_focused);
    lv_style_set_bg_color(&pass_focused, lv_color_hex(GREEN_LIGHT));
    lv_style_set_border_color(&pass_focused, lv_color_hex(GREEN_LIGHTER));
    lv_style_set_outline_color(&pass_focused, lv_color_hex(GREEN_LIGHTER));

    // Initialize the main fail style
    lv_style_init(&fail_main);
    lv_style_set_bg_color(&fail_main, lv_color_hex(RED_DIMMEST));
    lv_style_set_border_color(&fail_main, lv_color_hex(RED_MAIN));

    // Initialize the light fail style
    lv_style_init(&fail_light);
    lv_style_set_bg_color(&fail_light, lv_color_hex(RED_DIM));
    lv_style_set_border_color(&fail_light, lv_color_hex(RED_LIGHT));

    // Initialize the focused fail style
    lv_style_init(&fail_focused);
    lv_style_set_bg_color(&fail_focused, lv_color_hex(RED_LIGHT));
    lv_style_set_border_color(&fail_focused, lv_color_hex(RED_LIGHTER));
    lv_style_set_outline_color(&fail_focused, lv_color_hex(RED_LIGHTER));

    // Initialize pending styles
    lv_style_init(&pending_main);
    lv_style_set_bg_color(&pending_main, lv_color_hex(YELLOW_DIMMEST));
    lv_style_set_border_color(&pending_main, lv_color_hex(YELLOW_MAIN));

    lv_style_init(&pending_light);
    lv_style_set_bg_color(&pending_light, lv_color_hex(YELLOW_DIM));
    lv_style_set_border_color(&pending_light, lv_color_hex(YELLOW_LIGHT));

    lv_style_init(&pending_focused);
    lv_style_set_bg_color(&pending_focused, lv_color_hex(YELLOW_LIGHT));
    lv_style_set_border_color(&pending_focused, lv_color_hex(YELLOW_LIGHTER));
    lv_style_set_outline_color(&pending_focused, lv_color_hex(YELLOW_LIGHTER));

    // Initialize the button row style
    lv_style_init(&button_row);
    lv_style_set_size(&button_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_style_set_bg_opa(&button_row, LV_OPA_TRANSP);
    lv_style_set_border_width(&button_row, 0);

    // Crypto button main style
    lv_style_init(&crypto_btn);
    lv_style_set_bg_color(&crypto_btn, lv_color_hex(BLUE_DIMMEST));
    lv_style_set_border_color(&crypto_btn, lv_color_hex(BLUE_MAIN));

    // Crypto button focused style
    lv_style_init(&crypto_btn_focused);
    lv_style_set_bg_color(&crypto_btn_focused, lv_color_hex(BLUE_LIGHT));
    lv_style_set_border_color(&crypto_btn_focused, lv_color_hex(BLUE_LIGHTER));
    lv_style_set_outline_color(&crypto_btn_focused, lv_color_hex(BLUE_LIGHTER));

    // Initialize slider main style
    lv_style_init(&rgb_brightness_slider);
    lv_style_set_bg_color(&rgb_brightness_slider, lv_color_hex(GRAY_SHADE_2));
    lv_style_set_border_width(&rgb_brightness_slider, 0);
    lv_style_set_radius(&rgb_brightness_slider, 0);
    lv_style_set_height(&rgb_brightness_slider, 4);
    lv_style_set_pad_all(&rgb_brightness_slider, 4);

    // Initialize slider indicator style
    lv_style_init(&rgb_brightness_slider_indicator);
    lv_style_set_bg_color(&rgb_brightness_slider_indicator, lv_color_hex(WHITE));
    lv_style_set_radius(&rgb_brightness_slider_indicator, 0);

    // Initialize slider knob style
    lv_style_init(&rgb_brightness_slider_knob);
    lv_style_set_bg_color(&rgb_brightness_slider_knob, lv_color_hex(GRAY_TINT_1));
    lv_style_set_border_color(&rgb_brightness_slider_knob, lv_color_hex(WHITE));
    lv_style_set_border_width(&rgb_brightness_slider_knob, 2);
    lv_style_set_radius(&rgb_brightness_slider_knob, 0);
    lv_style_set_width(&rgb_brightness_slider_knob, 12);
    lv_style_set_height(&rgb_brightness_slider_knob, 12);

    // Initialize slider focused style
    lv_style_init(&rgb_brightness_slider_focused);
    lv_style_set_outline_color(&rgb_brightness_slider_focused, lv_color_hex(GRAY_TINT_2));

    // Initialize slider editing style
    lv_style_init(&rgb_brightness_slider_editing);
    lv_style_set_outline_color(&rgb_brightness_slider_editing, lv_color_hex(YELLOW_LIGHTEST));
    lv_style_set_outline_width(&rgb_brightness_slider_editing, 4);
    lv_style_set_outline_opa(&rgb_brightness_slider_editing, LV_OPA_50);

    // List main style
    lv_style_init(&rgb_list);
    lv_style_set_bg_opa(&rgb_list, LV_OPA_TRANSP);
    lv_style_set_border_width(&rgb_list, 0);
    lv_style_set_radius(&rgb_list, 0);

    // List button style
    lv_style_init(&rgb_list_button);
    lv_style_set_bg_opa(&rgb_list_button, LV_OPA_TRANSP);
    lv_style_set_border_color(&rgb_list_button, lv_color_hex(GRAY_BASE));
    lv_style_set_text_color(&rgb_list_button, lv_color_hex(WHITE));
    lv_style_set_text_font(&rgb_list_button, &clarity_14);

    // List button focused style
    lv_style_init(&rgb_list_button_focused);
    lv_style_set_bg_color(&rgb_list_button_focused, lv_color_hex(GRAY_BASE));
    lv_style_set_border_color(&rgb_list_button_focused, lv_color_hex(GRAY_TINT_1));
    lv_style_set_outline_color(&rgb_list_button_focused, lv_color_hex(GRAY_TINT_2));
    lv_style_set_outline_opa(&rgb_list_button_focused, LV_OPA_COVER);

    // Initialize the back button style
    lv_style_init(&rgb_back_button);
    lv_style_set_bg_color(&rgb_back_button, lv_color_hex(GRAY_BASE));
    lv_style_set_border_color(&rgb_back_button, lv_color_hex(GRAY_TINT_1));

    // Initialize the back button focused style
    lv_style_init(&rgb_back_button_focused);
    lv_style_set_bg_color(&rgb_back_button_focused, lv_color_hex(GRAY_TINT_1));
    lv_style_set_border_color(&rgb_back_button_focused, lv_color_hex(GRAY_TINT_2));
    lv_style_set_outline_color(&rgb_back_button_focused, lv_color_hex(GRAY_TINT_2));

    styles_initialized = true;
}

lv_obj_t *create_hwtest_screen() {
    static ui_screen_t screen_type = SCREEN_HWTEST;
    hwtest_loaded_ms               = lv_tick_get();

    // Initialize the styles for the hardware test screen
    hwtest_initialize_styles();

    // Create a screen object for the hardware test screen
    scr_hwtest = lv_obj_create(NULL);
    lv_obj_set_user_data(scr_hwtest, &screen_type);
    lv_obj_set_size(scr_hwtest, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(scr_hwtest, lv_color_hex(BLACK), LV_PART_MAIN);
    lv_obj_remove_flag(scr_hwtest, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scr_hwtest, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr_hwtest, hwtest_screen_delete_event_cb, LV_EVENT_DELETE, NULL);
    lv_obj_add_event_cb(scr_hwtest, hwtest_screen_loaded_event_cb, LV_EVENT_SCREEN_LOADED, NULL);

    // Create a container to hold the list of tests and the bottom pass/fail buttons
    hwtest_container = lv_obj_create(scr_hwtest);
    lv_obj_add_style(hwtest_container, &container_style, LV_PART_MAIN);
    lv_obj_add_style(hwtest_container, &pass_main, LV_PART_MAIN);
    lv_obj_remove_flag(hwtest_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(hwtest_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(hwtest_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(hwtest_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    // Set up the navigation context
    hwtest_indev   = input_get_device();
    hwtest_group   = lv_group_create();
    hwtest_nav_ctx = nav_ctx_create(hwtest_indev, hwtest_group, hwtest_container);
    nav_ctx_set_wrap(hwtest_nav_ctx, false, true);

    // Create the list of hardware tests
    hwtest_render_list(hwtest_container);

    // Create the pass/fail button
    lv_obj_t *bottom_buttons = lv_obj_create(hwtest_container);
    lv_obj_add_style(bottom_buttons, &button_row, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(bottom_buttons, 5, LV_PART_MAIN);
    lv_obj_set_flex_flow(bottom_buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_grow(bottom_buttons, 0);

    lv_obj_t *pass_btn = lv_button_create(bottom_buttons);
    lv_obj_add_style(pass_btn, &test_button_style, LV_PART_MAIN);
    lv_obj_add_style(pass_btn, &pass_light, LV_PART_MAIN);
    lv_obj_t *pass_label = lv_label_create(pass_btn);
    lv_label_set_text(pass_label, "Pass");
    lv_obj_set_style_text_align(pass_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_add_event_cb(pass_btn, hwtest_button_event_cb, LV_EVENT_CLICKED, (void *)HWTEST_BTN_PASS);

    nav_register(hwtest_nav_ctx, pass_btn);

    lv_obj_t *first_btn = lv_obj_get_child(hwtest_list, 0);
    nav_autolink(hwtest_nav_ctx);
    nav_focus(hwtest_nav_ctx, first_btn);

    // Run the quick tests
    hwtest_quick_tests();

    if (hwtest_results[HWTEST_CRYPTO_SETUP] == HWTEST_RESULT_PENDING && crypto_bg_timer == NULL) {
        crypto_bg_timer = lv_timer_create(crypto_bg_poll_timer_cb, 1000, NULL);
        lv_timer_ready(crypto_bg_timer);
    }

    return scr_hwtest;
}

static void hwtest_update_theme() {
    bool any_failed  = false;
    bool any_pending = false;
    for (int i = 1; i < HWTEST_COUNT; i++) {
        if (hwtest_results[i] == HWTEST_RESULT_FAILED) {
            any_failed = true;
            break;
        }
        if (hwtest_results[i] == HWTEST_RESULT_PENDING) {
            any_pending = true;
        }
    }

    lv_style_t *set_style_main;
    lv_style_t *set_style_light;
    lv_style_t *set_style_focused;
    if (any_failed) {
        set_style_main    = &fail_main;
        set_style_light   = &fail_light;
        set_style_focused = &fail_focused;
    } else if (any_pending) {
        set_style_main    = &pending_main;
        set_style_light   = &pending_light;
        set_style_focused = &pending_focused;
    } else {
        set_style_main    = &pass_main;
        set_style_light   = &pass_light;
        set_style_focused = &pass_focused;
    }

    // Update the styles for the hardware test screen
    lv_obj_remove_style(hwtest_container, &pass_main, LV_PART_MAIN);
    lv_obj_remove_style(hwtest_container, &fail_main, LV_PART_MAIN);
    lv_obj_remove_style(hwtest_container, &pending_main, LV_PART_MAIN);
    lv_obj_add_style(hwtest_container, set_style_main, LV_PART_MAIN);

    // Update the list scrollbar style
    lv_obj_remove_style(hwtest_list, &pass_main, LV_PART_SCROLLBAR);
    lv_obj_remove_style(hwtest_list, &fail_main, LV_PART_SCROLLBAR);
    lv_obj_remove_style(hwtest_list, &pending_main, LV_PART_SCROLLBAR);
    lv_obj_add_style(hwtest_list, set_style_main, LV_PART_SCROLLBAR);

    // Update the list item border color for each list item
    lv_style_value_t border_color_main;
    lv_style_get_prop(set_style_main, LV_STYLE_BORDER_COLOR, &border_color_main);
    for (int i = 1; i < HWTEST_COUNT; i++) {
        lv_obj_t *item = lv_obj_get_child(hwtest_list, i - 1);
        if (item == NULL) {
            ESP_LOGW(TAG, "Failed to find hardware test item in list");
            continue;
        }
        lv_obj_set_style_border_color(item, border_color_main.color, LV_PART_MAIN);

        // Focus state
        lv_obj_remove_style(item, &pass_focused, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
        lv_obj_remove_style(item, &fail_focused, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
        lv_obj_remove_style(item, &pending_focused, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
        lv_obj_add_style(item, set_style_focused, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    }

    // Update the pass/fail button styles
    lv_obj_t *bottom_buttons = lv_obj_get_child(hwtest_container, -1);
    lv_obj_t *pass_button    = lv_obj_get_child(bottom_buttons, 0);
    lv_obj_remove_style(pass_button, &pass_light, LV_PART_MAIN);
    lv_obj_remove_style(pass_button, &fail_light, LV_PART_MAIN);
    lv_obj_remove_style(pass_button, &pending_light, LV_PART_MAIN);
    lv_obj_add_style(pass_button, set_style_light, LV_PART_MAIN);

    // Focus state
    lv_obj_remove_style(pass_button, &pass_focused, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_remove_style(pass_button, &fail_focused, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_remove_style(pass_button, &pending_focused, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_add_style(pass_button, set_style_focused, LV_PART_MAIN | LV_STATE_FOCUS_KEY);

    // Disable the pass button if any tests have failed
    lv_obj_set_state(pass_button, LV_STATE_DISABLED, any_failed);

    // If none are failed then focus the pass button
    if (!any_failed) {
        nav_focus(hwtest_nav_ctx, pass_button);
    }
}

static void hwtest_set_result(hwtest_t test, hwtest_result_t result) {
    if (test < 0 || test >= HWTEST_COUNT) {
        ESP_LOGW(TAG, "Invalid hardware test index");
        return;
    }
    hwtest_results[test] = result;

    // Find the item in the list and update the icon
    if (hwtest_list != NULL) {
        lv_obj_t *item = lv_obj_get_child(hwtest_list, test - 1);
        if (item == NULL) {
            return;
        }
        lv_obj_t *icon = lv_obj_get_child(item, -1);
        if (icon == NULL) {
            return;
        }
        const void *sym = LV_SYMBOL_MINUS;
        switch (result) {
            case HWTEST_RESULT_NONE: sym = LV_SYMBOL_MINUS; break;
            case HWTEST_RESULT_PENDING: sym = LV_SYMBOL_REFRESH; break;
            case HWTEST_RESULT_PASSED: sym = LV_SYMBOL_OK; break;
            case HWTEST_RESULT_FAILED: sym = LV_SYMBOL_CLOSE; break;
            default: break;
        }
        lv_image_set_src(icon, sym);
    } else {
        ESP_LOGW(TAG, "Hardware test list not created");
    }

    // Update the theme based on the test results
    hwtest_update_theme();
}

static void crypto_bg_poll_timer_cb(lv_timer_t *timer) {
    if (hwtest_results[HWTEST_CRYPTO_SETUP] != HWTEST_RESULT_PENDING) {
        lv_timer_delete(timer);
        crypto_bg_timer = NULL;
        return;
    }
    extern badge_state_t badge_state;
    bool provisioned = badge_state.provision_state.api_secrets && badge_state.provision_state.wifi_creds;
    if (se_ready() && provisioned) {
        hwtest_set_result(HWTEST_CRYPTO_SETUP, HWTEST_RESULT_PASSED);
        lv_timer_delete(timer);
        crypto_bg_timer = NULL;
    }
}

static void hwtest_screen_delete_event_cb(lv_event_t *e) {
    (void)e;
    if (crypto_bg_timer) {
        lv_timer_delete(crypto_bg_timer);
        crypto_bg_timer = NULL;
    }
    if (hwtest_led_layer_handle >= 0) {
        led_layer_destroy(hwtest_led_layer_handle);
        hwtest_led_layer_handle = -1;
    }
    led_layers_resume_all();
}

static void hwtest_screen_loaded_event_cb(lv_event_t *e) {
    (void)e;

    ESP_LOGI(TAG, "Screen loaded event fired");

    // Pause all external LED layers while hardware tests are active & set up initial pattern layer.
    if (hwtest_led_layer_handle < 0) {
        if (hwtest_led_state.mode == HWTEST_LED_MODE_NONE) {
            if (!badge_config.hw_pass) {
                hwtest_led_state.mode = HWTEST_LED_MODE_STATIC_WHITE;
            } else {
                // After initial pass, default to rainbow chase pattern when entering via Konami code, etc.
                hwtest_led_state.mode          = HWTEST_LED_MODE_CHASE;
                hwtest_led_state.chase_pos     = 0;
                hwtest_led_state.chase_forward = true;
                hwtest_led_state.color_offset  = 0;
            }
        }
        hwtest_led_state.last_update_ms = hwtest_now_ms();
        hwtest_led_state.breath_step    = 0;
        hwtest_led_state.breathing_up   = true;
        hwtest_led_state.frame_counter  = 0;
        hwtest_led_layer_handle = led_layer_create(LED_PRIORITY_HIGH, hwtest_led_render_cb, &hwtest_led_state, true, true, true);
        if (hwtest_led_layer_handle < 0) {
            ESP_LOGE(TAG, "Failed to create hwtest LED layer!");
        }
        led_layers_dump();
    }
    led_layers_pause_all_except(hwtest_led_layer_handle);
    led_layers_dump();
}

static void hwtest_render_list(lv_obj_t *parent) {
    // Delete the existing list if it exists so we can recreate it
    if (hwtest_list != NULL && lv_obj_is_valid(hwtest_list)) {
        lv_obj_delete(hwtest_list);
    }
    hwtest_list = NULL;

    // Create a list of the hardware tests that can be run
    hwtest_list = lv_list_create(parent);
    lv_obj_set_size(hwtest_list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(hwtest_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(hwtest_list, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(hwtest_list, 0, LV_PART_MAIN);
    lv_obj_set_flex_grow(hwtest_list, 1);

    // Set the scrollbar color and style
    lv_obj_add_style(hwtest_list, &pass_main, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(hwtest_list, 0, LV_PART_SCROLLBAR);

    // Add the hardware tests to the list
    for (int i = 1; i < HWTEST_COUNT; i++) {
        lv_obj_t *btn = lv_list_add_button(hwtest_list, NULL, hwtest_labels[i]);

        // List item button style and alignment
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, lv_color_hex(GREEN_MAIN), LV_PART_MAIN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

        // Style for focused state
        lv_obj_add_style(btn, &pass_focused, LV_PART_MAIN | LV_STATE_FOCUS_KEY);

        // Style the button label
        lv_obj_set_style_text_font(btn, &clarity_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(btn, lv_color_hex(WHITE), LV_PART_MAIN);

        // Add an icon on the right to show the test result
        hwtest_result_t result = hwtest_results[i];
        lv_obj_t *result_icon  = lv_image_create(btn);
        lv_obj_set_size(result_icon, 18, 18);
        lv_image_set_inner_align(result_icon, LV_IMAGE_ALIGN_CENTER);
        lv_obj_set_style_text_font(result_icon, &lv_font_montserrat_14, LV_PART_MAIN);
        const void *sym = LV_SYMBOL_MINUS;
        switch (result) {
            case HWTEST_RESULT_NONE: sym = LV_SYMBOL_MINUS; break;
            case HWTEST_RESULT_PENDING: sym = LV_SYMBOL_REFRESH; break;
            case HWTEST_RESULT_PASSED: sym = LV_SYMBOL_OK; break;
            case HWTEST_RESULT_FAILED: sym = LV_SYMBOL_CLOSE; break;
            default: break;
        }
        lv_image_set_src(result_icon, sym);

        // Add an event handler to the button
        lv_obj_add_event_cb(btn, hwtest_list_event_cb, LV_EVENT_CLICKED, (void *)i);
    }

    nav_register_children(hwtest_nav_ctx, hwtest_list);
}

static void hwtest_list_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        if (lv_tick_elaps(hwtest_loaded_ms) < 500) {
            // Ignore clicks within the first 500ms of loading the screen to avoid unintentional or
            // stale/buffered clicks
            return;
        }
        hwtest_t test = (hwtest_t)lv_event_get_user_data(e);
        if (test == HWTEST_NONE || test >= HWTEST_COUNT) {
            ESP_LOGW(TAG, "Invalid hardware test selected");
            return;
        }
        const char *label = hwtest_labels[test];
        ESP_LOGI(TAG, "Running hardware test: %s", label);

        // Run the selected test
        switch (test) {
            case HWTEST_CHIPS: //
                hwtest_start_chip_test(scr_hwtest);
                break;
            case HWTEST_JOYSTICK: //
                hwtest_start_joystick_test(scr_hwtest);
                break;
            case HWTEST_CRYPTO_SETUP: //
                hwtest_start_crypto_setup_test(scr_hwtest);
                break;
            case HWTEST_RGB_LEDS: //
                hwtest_start_rgb_led_test(scr_hwtest);
                break;
            case HWTEST_AI_INHIBIT: //
                // hwtest_start_ai_inhibit_test(scr_hwtest);
                break;
            case HWTEST_AI_SECURITY: //
                // hwtest_start_ai_security_test(scr_hwtest);
                break;
            // HWTEST_POWER case removed - not applicable for this badge
            default: break;
        }
    }
}

static void hwtest_button_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    hwtest_btn_t btn     = (hwtest_btn_t)(intptr_t)lv_event_get_user_data(e);

    if (code == LV_EVENT_CLICKED) {
        switch (btn) {
            case HWTEST_BTN_PASS: //
                // Save the flag in the badge configuration
                badge_config.hw_pass = true;
                save_badge_config();

                // Destroy hwtest LED layer and resume other layers
                if (hwtest_led_layer_handle >= 0) {
                    led_layer_destroy(hwtest_led_layer_handle);
                    hwtest_led_layer_handle = -1;
                }
                led_layers_resume_all();
                set_screen(SCREEN_MAIN); // Go to main screen
                break;
            default: break;
        }
    }
}

/*****************************************************
 * Quick Test Functions                              *
 *****************************************************/

static void hwtest_quick_tests() {
    // Run quick tests for chip communications (NFC and Secure Element)
    bool chip_pass = test_crypto_comms() && test_nfc_comms();
    hwtest_set_result(HWTEST_CHIPS, chip_pass ? HWTEST_RESULT_PASSED : HWTEST_RESULT_FAILED);

    // Quick test for RGB LEDs.
    // Before the hardware test has ever been marked passed, show static dim white but any other time show the animated chase
    // pattern.
    if (led_init() == ESP_OK && led_clear() == ESP_OK) {
        if (!badge_config.hw_pass) {
            for (int i = 0; i < NUM_LEDS; i++) {
                led_set_with_brightness(i, 255, 255, 255, LED_HWTEST_WHITE_BRIGHTNESS);
            }
            led_show();
            current_brightness   = LED_HWTEST_WHITE_BRIGHTNESS;
            last_applied_pattern = -1;
        } else {
            led_set_brightness(64);
            current_brightness   = 64;
            last_applied_pattern = 8;
            rgb_led_start_chase_pattern();
        }
        hwtest_set_result(HWTEST_RGB_LEDS, HWTEST_RESULT_PASSED);
    } else {
        hwtest_set_result(HWTEST_RGB_LEDS, HWTEST_RESULT_FAILED);
    }

    // Joystick test starts as none - requires user interaction
    hwtest_set_result(HWTEST_JOYSTICK, HWTEST_RESULT_NONE);

    gpio_set_direction(5, GPIO_MODE_INPUT);
    gpio_set_pull_mode(5, GPIO_PULLUP_ENABLE);
    gpio_set_direction(4, GPIO_MODE_INPUT);
    gpio_set_pull_mode(4, GPIO_PULLUP_ENABLE);
    if (gpio_get_level(5) == 0) {
        hwtest_set_result(HWTEST_AI_INHIBIT, HWTEST_RESULT_PASSED);
    } else {
        hwtest_set_result(HWTEST_AI_INHIBIT, HWTEST_RESULT_FAILED);
    }
    if (gpio_get_level(4) == 0) {
        hwtest_set_result(HWTEST_AI_SECURITY, HWTEST_RESULT_PASSED);
    } else {
        hwtest_set_result(HWTEST_AI_SECURITY, HWTEST_RESULT_FAILED);
    }

    // Crypto setup (provisioning) quick state
#if HWTEST_FORCE_PENDING
    hwtest_set_result(HWTEST_CRYPTO_SETUP, HWTEST_RESULT_PENDING);
#else
    bool provisioned = badge_state.provision_state.api_secrets && badge_state.provision_state.wifi_creds;
    hwtest_set_result(HWTEST_CRYPTO_SETUP, se_ready() && provisioned ? HWTEST_RESULT_PASSED : HWTEST_RESULT_PENDING);
#endif

    // Check if all other tests (excluding joystick) passed
    bool all_passed = true;
    for (int i = 1; i < HWTEST_COUNT; i++) {
        if (i == HWTEST_JOYSTICK || i == HWTEST_CRYPTO_SETUP) { // Joystick requires interaction; Crypto can be pending
            continue;
        }
        if (hwtest_results[i] != HWTEST_RESULT_PASSED) {
            all_passed = false;
            break;
        }
    }

    // If all other tests passed, auto-launch joystick test ONLY if hardware test not yet passed.
    if (all_passed && !badge_config.hw_pass) {
        hwtest_start_joystick_test(scr_hwtest);
    }
}

/*****************************************************
 * Chip Test Functions                               *
 *****************************************************/

static void hwtest_start_chip_test(lv_obj_t *parent) {
    chip_comms_overlay = lv_obj_create(parent);
    lv_obj_set_size(chip_comms_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(chip_comms_overlay, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(chip_comms_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(chip_comms_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(chip_comms_overlay, 0, LV_PART_MAIN);
    lv_obj_align(chip_comms_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(chip_comms_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(chip_comms_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(chip_comms_overlay, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    // Nav context for the overlay
    chip_test_nav_group = lv_group_create();
    chip_test_nav_guard = nav_scope_push(input_get_device(), chip_test_nav_group);
    chip_test_nav_ctx   = nav_ctx_create(input_get_device(), chip_test_nav_group, chip_comms_overlay);
    nav_ctx_set_wrap(chip_test_nav_ctx, false, false);

    // Create a list for the chip test results
    chip_test_list = lv_list_create(chip_comms_overlay);
    lv_obj_set_size(chip_test_list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(chip_test_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(chip_test_list, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(chip_test_list, 0, LV_PART_MAIN);
    lv_obj_set_style_text_color(chip_test_list, lv_color_hex(GRAY_SHADE_2), LV_PART_MAIN);
    lv_obj_align(chip_test_list, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_flex_grow(chip_test_list, 1);

    // Add devices to the list
    for (int i = 0; i < CHIP_TEST_COUNT; i++) {
        lv_obj_t *item = lv_list_add_text(chip_test_list, chips[i]);
        lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_text_font(item, &clarity_16, LV_PART_MAIN);

        // Add an icon on the right to show the test result
        lv_obj_t *result_icon = lv_image_create(item);
        lv_obj_set_size(result_icon, 18, 18);
        lv_image_set_inner_align(result_icon, LV_IMAGE_ALIGN_CENTER);
        lv_image_set_src(result_icon, LV_SYMBOL_MINUS);
        lv_obj_set_style_text_font(result_icon, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(result_icon, lv_color_hex(GRAY_TINT_1), LV_PART_MAIN);
        lv_obj_align(result_icon, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    // Create a "Continue" button
    lv_obj_t *continue_btn = lv_button_create(chip_comms_overlay);
    lv_obj_add_style(continue_btn, &test_button_style, LV_PART_MAIN);
    lv_obj_add_style(continue_btn, &pass_light, LV_PART_MAIN);
    lv_obj_set_style_outline_color(continue_btn, lv_color_hex(GREEN_MAIN), LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_t *continue_label = lv_label_create(continue_btn);
    lv_label_set_text(continue_label, "Continue");
    lv_obj_add_event_cb(continue_btn, chip_test_continue_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_state(continue_btn, LV_STATE_DISABLED, true);

    // Register navigable objects and push new scope
    nav_register(chip_test_nav_ctx, continue_btn);

    // Reset the state and results for the chip tests
    chip_test_current = 0;
    for (int i = 0; i < CHIP_TEST_COUNT; i++) {
        chip_test_results[i] = HWTEST_RESULT_NONE;
    }

    // Run the chip test
    lv_timer_t *timer = lv_timer_create(chip_test_run_next, 1000, NULL);
    lv_timer_set_repeat_count(timer, CHIP_TEST_COUNT);
    lv_timer_ready(timer);
}

static void chip_test_run_next(lv_timer_t *timer) {
    if (chip_test_current >= CHIP_TEST_COUNT) {
        lv_timer_delete(timer);
        chip_test_current = 0;
        chip_test_render_results();
        return;
    }

    hwtest_result_t result = HWTEST_RESULT_NONE;
    switch (chip_test_current) {
        case HWTEST_CHIP_CRYPTO: //
            result = test_crypto_comms() ? HWTEST_RESULT_PASSED : HWTEST_RESULT_FAILED;
            break;
        case HWTEST_CHIP_NFC: //
            result = test_nfc_comms() ? HWTEST_RESULT_PASSED : HWTEST_RESULT_FAILED;
            break;
        default: break;
    }

    // Update the test results list
    chip_test_results[chip_test_current] = result;
    chip_test_render_results();

    // Enable the "Continue" button if all tests have been run
    if (chip_test_current == CHIP_TEST_COUNT - 1) {
        lv_obj_t *continue_btn = lv_obj_get_child(chip_comms_overlay, -1);
        lv_obj_set_state(continue_btn, LV_STATE_DISABLED, false);
        nav_focus(chip_test_nav_ctx, continue_btn);
    }

    // Increment the current test index for the next run
    chip_test_current++;
}

static void chip_test_render_results() {
    for (int i = 0; i < CHIP_TEST_COUNT; i++) {
        lv_obj_t *item = lv_obj_get_child(chip_test_list, i);
        if (item == NULL) {
            ESP_LOGW(TAG, "Failed to find chip test item in list");
            return;
        }
        lv_obj_t *icon = lv_obj_get_child(item, -1);
        if (icon == NULL) {
            icon = lv_image_create(item);
            lv_obj_set_size(icon, 18, 18);
            lv_image_set_inner_align(icon, LV_IMAGE_ALIGN_CENTER);
        }
        lv_image_set_src(icon, chip_test_results[i] == HWTEST_RESULT_NONE     ? LV_SYMBOL_MINUS
                               : chip_test_results[i] == HWTEST_RESULT_PASSED ? LV_SYMBOL_OK
                                                                              : LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_color(icon,
                                    chip_test_results[i] == HWTEST_RESULT_NONE     ? lv_color_hex(GRAY_TINT_1)
                                    : chip_test_results[i] == HWTEST_RESULT_PASSED ? lv_color_hex(GREEN_MAIN)
                                                                                   : lv_color_hex(RED_MAIN),
                                    LV_PART_MAIN);
    }
}

static void chip_test_continue_event_cb(lv_event_t *e) {
    // Restore previous navigation context
    nav_scope_pop(input_get_device(), chip_test_nav_guard);
    if (chip_test_nav_ctx != NULL) {
        nav_ctx_destroy(chip_test_nav_ctx);
        chip_test_nav_ctx = NULL;
    }
    if (chip_test_nav_group != NULL) {
        lv_group_delete(chip_test_nav_group);
        chip_test_nav_group = NULL;
    }

    // Delete the overlay
    if (chip_comms_overlay != NULL && lv_obj_is_valid(chip_comms_overlay)) {
        lv_obj_delete(chip_comms_overlay);
        chip_comms_overlay = NULL;
    }

    // Determine the overall result based on individual tests
    bool overall_result = true;
    for (int i = 0; i < CHIP_TEST_COUNT; i++) {
        if (chip_test_results[i] != HWTEST_RESULT_PASSED) {
            overall_result = false;
            break;
        }
    }
    hwtest_set_result(HWTEST_CHIPS, overall_result ? HWTEST_RESULT_PASSED : HWTEST_RESULT_FAILED);
}

static bool test_crypto_comms() {
    // Test that we can initialize and communicate with the secure element
    esp_err_t ret = se_init();
    if (ret != ESP_OK) {
        return false;
    }

    // Check if the secure element is ready/provisioned
    return se_ready();
}

static bool test_nfc_comms() {
    // If already initialized, consider comms healthy
    if (nfc_ready()) {
        return true;
    }
    // Otherwise attempt init then deinit for a basic comms check
    nfc_err_t ret = nfc_init();
    if (ret != NFC_OK) {
        return false;
    }
    nfc_deinit();
    return true;
}

/*****************************************************
 * Crypto Setup (Provisioning Status) Test Functions *
 *****************************************************/

typedef enum {
    CRYPTO_STAGE_CONFIG_LOCK = 0,
    CRYPTO_STAGE_DATA_LOCK,
    CRYPTO_STAGE_API_SECRETS,
    CRYPTO_STAGE_WIFI_CREDS,
    CRYPTO_STAGE_COUNT,
} crypto_stage_t;

static lv_timer_t *crypto_poll_timer = NULL;
static lv_obj_t *crypto_stage_list   = NULL;

static void crypto_setup_update_icons() {
    if (!crypto_stage_list) {
        return;
    }
    bool config_locked = se_config_locked();
    bool data_locked   = se_data_locked();
    bool api_ok        = badge_state.provision_state.api_secrets;
    bool wifi_ok       = badge_state.provision_state.wifi_creds;
    if (HWTEST_FORCE_PENDING) {
        api_ok  = false;
        wifi_ok = false;
    }

    bool stages_done[CRYPTO_STAGE_COUNT] = {
        config_locked,
        data_locked,
        api_ok,
        wifi_ok,
    };

    for (int i = 0; i < CRYPTO_STAGE_COUNT; i++) {
        lv_obj_t *row = lv_obj_get_child(crypto_stage_list, i);
        if (!row) {
            continue;
        }
        lv_obj_t *icon = lv_obj_get_child(row, -1);
        if (!icon) {
            icon = lv_image_create(row);
            lv_obj_set_size(icon, 18, 18);
            lv_image_set_inner_align(icon, LV_IMAGE_ALIGN_CENTER);
        }
        const void *sym = LV_SYMBOL_MINUS;
        if (stages_done[i]) {
            sym = LV_SYMBOL_OK;
            lv_obj_set_style_text_color(icon, lv_color_hex(WHITE), LV_PART_MAIN);
        } else {
            sym = LV_SYMBOL_REFRESH;
            lv_obj_set_style_text_color(icon, lv_color_hex(WHITE), LV_PART_MAIN);
        }
        lv_image_set_src(icon, sym);
    }

    // If fully ready, mark test passed
    if (config_locked && data_locked && api_ok && wifi_ok) {
        if (crypto_poll_timer) {
            lv_timer_delete(crypto_poll_timer);
            crypto_poll_timer = NULL;
        }
        hwtest_set_result(HWTEST_CRYPTO_SETUP, HWTEST_RESULT_PASSED);
    }
}

static void crypto_setup_poll_timer_cb(lv_timer_t *timer) {
    (void)timer;
    crypto_setup_update_icons();
}

static nav_scope_guard_t crypto_nav_guard;
static nav_ctx_t *crypto_nav_ctx    = NULL;
static lv_group_t *crypto_nav_group = NULL;

static void crypto_setup_close_event_cb(lv_event_t *e) {
    (void)e;
    // Restore nav context
    nav_scope_pop(input_get_device(), crypto_nav_guard);
    if (crypto_nav_ctx) {
        nav_ctx_destroy(crypto_nav_ctx);
        crypto_nav_ctx = NULL;
    }
    if (crypto_nav_group) {
        lv_group_delete(crypto_nav_group);
        crypto_nav_group = NULL;
    }
    if (crypto_poll_timer) {
        lv_timer_delete(crypto_poll_timer);
        crypto_poll_timer = NULL;
    }
    if (crypto_setup_overlay && lv_obj_is_valid(crypto_setup_overlay)) {
        lv_obj_delete(crypto_setup_overlay);
        crypto_setup_overlay = NULL;
    }
}

static void hwtest_start_crypto_setup_test(lv_obj_t *parent) {
    crypto_setup_overlay = lv_obj_create(parent);
    lv_obj_set_size(crypto_setup_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(crypto_setup_overlay, lv_color_hex(BLUE_DIMMEST), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(crypto_setup_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(crypto_setup_overlay, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(crypto_setup_overlay, lv_color_hex(BLUE_MAIN), LV_PART_MAIN);
    lv_obj_set_style_radius(crypto_setup_overlay, 1, LV_PART_MAIN);
    lv_obj_align(crypto_setup_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(crypto_setup_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(crypto_setup_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(crypto_setup_overlay, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    // Navigation context
    crypto_nav_group = lv_group_create();
    crypto_nav_guard = nav_scope_push(input_get_device(), crypto_nav_group);
    crypto_nav_ctx   = nav_ctx_create(input_get_device(), crypto_nav_group, crypto_setup_overlay);
    nav_ctx_set_wrap(crypto_nav_ctx, false, false);

    // Title
    lv_obj_t *title = lv_label_create(crypto_setup_overlay);
    lv_label_set_text(title, "Crypto Provisioning Status");
    lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_color(title, lv_color_hex(BLUE_LIGHTEST), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &clarity_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_size(title, lv_pct(90), LV_SIZE_CONTENT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *list_container = lv_obj_create(crypto_setup_overlay);
    lv_obj_set_size(list_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(list_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(list_container, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(list_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list_container, 0, LV_PART_MAIN);
    lv_obj_set_flex_grow(list_container, 1);

    crypto_stage_list = lv_list_create(list_container);
    lv_obj_set_size(crypto_stage_list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(crypto_stage_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(crypto_stage_list, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(crypto_stage_list, lv_color_hex(BLUE_MAIN), LV_PART_MAIN);
    lv_obj_set_style_radius(crypto_stage_list, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(crypto_stage_list, 10, LV_PART_MAIN);
    lv_obj_set_style_margin_ver(crypto_stage_list, 10, LV_PART_MAIN);

    const char *stage_labels[CRYPTO_STAGE_COUNT] = {
        "Config Zone",
        "Data Zone",
        "API Secrets",
        "WiFi Creds",
    };
    for (int i = 0; i < CRYPTO_STAGE_COUNT; i++) {
        lv_obj_t *row = lv_list_add_text(crypto_stage_list, stage_labels[i]);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_text_font(row, &clarity_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(row, lv_color_hex(WHITE), LV_PART_MAIN);

        lv_obj_t *icon = lv_image_create(row);
        lv_obj_set_size(icon, 18, 18);
        lv_image_set_inner_align(icon, LV_IMAGE_ALIGN_CENTER);
        lv_image_set_src(icon, LV_SYMBOL_MINUS);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(icon, lv_color_hex(WHITE), LV_PART_MAIN);
        lv_obj_align(icon, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    lv_obj_t *close_btn = lv_button_create(crypto_setup_overlay);
    lv_obj_add_style(close_btn, &test_button_style, LV_PART_MAIN);
    lv_obj_add_style(close_btn, &crypto_btn, LV_PART_MAIN);
    lv_obj_add_style(close_btn, &crypto_btn_focused, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "Back");
    lv_obj_add_event_cb(close_btn, crypto_setup_close_event_cb, LV_EVENT_CLICKED, NULL);
    nav_register(crypto_nav_ctx, close_btn);
    nav_focus(crypto_nav_ctx, close_btn);

    // Initial icon state update
    crypto_setup_update_icons();

    // Poll timer only if not ready yet
    if (hwtest_results[HWTEST_CRYPTO_SETUP] == HWTEST_RESULT_PENDING) {
        crypto_poll_timer = lv_timer_create(crypto_setup_poll_timer_cb, 1000, NULL);
        lv_timer_ready(crypto_poll_timer);
    }
}

/*****************************************************
 * Joystick Test Functions                           *
 *****************************************************/

static void hwtest_start_joystick_test(lv_obj_t *parent) {
    joystick_overlay = lv_obj_create(parent);
    lv_obj_set_size(joystick_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(joystick_overlay, lv_color_hex(BLACK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(joystick_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(joystick_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(joystick_overlay, 0, LV_PART_MAIN);
    lv_obj_align(joystick_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(joystick_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Navigation context for the overlay
    joystick_nav_group = lv_group_create();
    joystick_nav_guard = nav_scope_push(input_get_device(), joystick_nav_group);
    joystick_nav_ctx   = nav_ctx_create(input_get_device(), joystick_nav_group, joystick_overlay);
    nav_ctx_set_wrap(joystick_nav_ctx, false, false);

    // Instructions
    lv_obj_t *instructions = lv_label_create(joystick_overlay);
    lv_label_set_text(instructions, "Press joystick in each direction");
    lv_label_set_long_mode(instructions, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_color(instructions, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(instructions, &clarity_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(instructions, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_size(instructions, lv_pct(90), LV_SIZE_CONTENT);
    lv_obj_align(instructions, LV_ALIGN_TOP_MID, 0, 10);

    /***** Visual indicators for joystick directions *****/

    // Center
    lv_obj_t *center_circle = lv_obj_create(joystick_overlay);
    lv_obj_set_size(center_circle, 60, 60);
    lv_obj_set_style_bg_color(center_circle, lv_color_hex(GRAY_SHADE_6), LV_PART_MAIN);
    lv_obj_set_style_border_width(center_circle, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(center_circle, lv_color_hex(GRAY_SHADE_1), LV_PART_MAIN);
    lv_obj_set_style_radius(center_circle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_align(center_circle, LV_ALIGN_CENTER, 0, 30);
    lv_obj_remove_flag(center_circle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(center_circle, LV_OBJ_FLAG_CLICKABLE);

    // Event handler
    lv_obj_add_event_cb(center_circle, joystick_key_event_cb, LV_EVENT_KEY, NULL);

    // Register the circle as a navigable object and set focus
    nav_register(joystick_nav_ctx, center_circle);
    nav_focus(joystick_nav_ctx, center_circle);

    joystick_center_indicator = lv_obj_create(center_circle);
    lv_obj_set_size(joystick_center_indicator, 40, 40);
    lv_obj_set_style_bg_color(joystick_center_indicator, lv_color_hex(GRAY_SHADE_3), LV_PART_MAIN);
    lv_obj_set_style_border_width(joystick_center_indicator, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(joystick_center_indicator, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_align(joystick_center_indicator, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(joystick_center_indicator, LV_OBJ_FLAG_SCROLLABLE);

    // Up
    joystick_up_indicator = lv_obj_create(joystick_overlay);
    lv_obj_set_size(joystick_up_indicator, 40, 40);
    lv_obj_set_style_bg_color(joystick_up_indicator, lv_color_hex(GRAY_SHADE_6), LV_PART_MAIN);
    lv_obj_set_style_border_width(joystick_up_indicator, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(joystick_up_indicator, lv_color_hex(GRAY_SHADE_1), LV_PART_MAIN);
    lv_obj_set_style_radius(joystick_up_indicator, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_align(joystick_up_indicator, LV_ALIGN_CENTER, 0, -50);
    lv_obj_remove_flag(joystick_up_indicator, LV_OBJ_FLAG_SCROLLABLE);

    // Down
    joystick_down_indicator = lv_obj_create(joystick_overlay);
    lv_obj_set_size(joystick_down_indicator, 40, 40);
    lv_obj_set_style_bg_color(joystick_down_indicator, lv_color_hex(GRAY_SHADE_6), LV_PART_MAIN);
    lv_obj_set_style_border_width(joystick_down_indicator, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(joystick_down_indicator, lv_color_hex(GRAY_SHADE_1), LV_PART_MAIN);
    lv_obj_set_style_radius(joystick_down_indicator, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_align(joystick_down_indicator, LV_ALIGN_CENTER, 0, 110);
    lv_obj_remove_flag(joystick_down_indicator, LV_OBJ_FLAG_SCROLLABLE);

    // Left
    joystick_left_indicator = lv_obj_create(joystick_overlay);
    lv_obj_set_size(joystick_left_indicator, 40, 40);
    lv_obj_set_style_bg_color(joystick_left_indicator, lv_color_hex(GRAY_SHADE_6), LV_PART_MAIN);
    lv_obj_set_style_border_width(joystick_left_indicator, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(joystick_left_indicator, lv_color_hex(GRAY_SHADE_1), LV_PART_MAIN);
    lv_obj_set_style_radius(joystick_left_indicator, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_align(joystick_left_indicator, LV_ALIGN_CENTER, -80, 30);
    lv_obj_remove_flag(joystick_left_indicator, LV_OBJ_FLAG_SCROLLABLE);

    // Right
    joystick_right_indicator = lv_obj_create(joystick_overlay);
    lv_obj_set_size(joystick_right_indicator, 40, 40);
    lv_obj_set_style_bg_color(joystick_right_indicator, lv_color_hex(GRAY_SHADE_6), LV_PART_MAIN);
    lv_obj_set_style_border_width(joystick_right_indicator, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(joystick_right_indicator, lv_color_hex(GRAY_SHADE_1), LV_PART_MAIN);
    lv_obj_set_style_radius(joystick_right_indicator, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_align(joystick_right_indicator, LV_ALIGN_CENTER, 80, 30);
    lv_obj_remove_flag(joystick_right_indicator, LV_OBJ_FLAG_SCROLLABLE);

    // Reset test state
    for (int i = 0; i < 5; i++) {
        joystick_test_complete[i] = false;
    }
}

static void joystick_key_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) {
        return;
    }

    uint32_t key = lv_event_get_key(e);
    ESP_LOGI(TAG, "Joystick key pressed: %d", key);

    // Map keys to test completion flags and indicators
    int test_index      = -1;
    lv_obj_t *indicator = NULL;

    switch (key) {
        case LV_KEY_UP:
            test_index = 0;
            indicator  = joystick_up_indicator;
            break;
        case LV_KEY_DOWN:
            test_index = 1;
            indicator  = joystick_down_indicator;
            break;
        case LV_KEY_LEFT:
            test_index = 2;
            indicator  = joystick_left_indicator;
            break;
        case LV_KEY_RIGHT:
            test_index = 3;
            indicator  = joystick_right_indicator;
            break;
        case LV_KEY_ENTER:
            test_index = 4;
            indicator  = joystick_center_indicator;
            break;
        default: return;
    }

    // Mark test as complete and update visual indicator
    if (test_index >= 0 && !joystick_test_complete[test_index]) {
        joystick_test_complete[test_index] = true;
        lv_obj_set_style_bg_color(indicator, lv_color_hex(GREEN_DIM), LV_PART_MAIN);

        // Check if all tests are complete
        bool all_complete = true;
        for (int i = 0; i < 5; i++) {
            if (!joystick_test_complete[i]) {
                all_complete = false;
                break;
            }
        }

        if (all_complete) {
            // Use a timer to delay the end so user can see all green indicators
            lv_timer_t *end_timer = lv_timer_create(joystick_test_end_timer_cb, 800, NULL);
            lv_timer_set_repeat_count(end_timer, 1);
        }
    }
}

static void joystick_test_end_timer_cb(lv_timer_t *timer) {
    joystick_test_end();
    lv_timer_delete(timer);
}

static void joystick_test_end() {
    // Restore the previous navigation context
    nav_scope_pop(input_get_device(), joystick_nav_guard);
    if (joystick_nav_ctx != NULL) {
        nav_ctx_destroy(joystick_nav_ctx);
        joystick_nav_ctx = NULL;
    }
    if (joystick_nav_group != NULL) {
        lv_group_delete(joystick_nav_group);
        joystick_nav_group = NULL;
    }

    // Delete the overlay
    if (joystick_overlay != NULL && lv_obj_is_valid(joystick_overlay)) {
        lv_obj_delete(joystick_overlay);
        joystick_overlay = NULL;
    }

    // Determine the overall result based on the whether all buttons were pressed
    bool overall_result = true;
    for (int i = 0; i < 5; i++) {
        if (!joystick_test_complete[i]) {
            overall_result = false;
            break;
        }
    }
    hwtest_set_result(HWTEST_JOYSTICK, overall_result ? HWTEST_RESULT_PASSED : HWTEST_RESULT_FAILED);
}

/*****************************************************
 * RGB LED Test Functions                            *
 *****************************************************/

static void hwtest_start_rgb_led_test(lv_obj_t *parent) {
    // Stop any existing animation timer from quick tests
    if (rgb_animation_timer != NULL) {
        lv_timer_delete(rgb_animation_timer);
        rgb_animation_timer = NULL;
    }

    // Initialize state variables
    last_applied_pattern = -1;

    // Navigation context for the overlay
    rgb_led_nav_group = lv_group_create();
    rgb_led_nav_guard = nav_scope_push(input_get_device(), rgb_led_nav_group);
    rgb_led_nav_ctx   = nav_ctx_create(input_get_device(), rgb_led_nav_group, parent);
    nav_ctx_set_wrap(rgb_led_nav_ctx, false, true);

    rgb_led_overlay = lv_obj_create(parent);
    lv_obj_set_size(rgb_led_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(rgb_led_overlay, lv_color_hex(BLACK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(rgb_led_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(rgb_led_overlay, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(rgb_led_overlay, lv_color_hex(GRAY_SHADE_1), LV_PART_MAIN);
    lv_obj_set_style_radius(rgb_led_overlay, 1, LV_PART_MAIN);
    lv_obj_set_flex_flow(rgb_led_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rgb_led_overlay, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    // Initialize LED system
    led_init();
    led_set_brightness(current_brightness);
    led_clear();

    // Brightness container
    lv_obj_t *brightness_container = lv_obj_create(rgb_led_overlay);
    lv_obj_set_size(brightness_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(brightness_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(brightness_container, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(brightness_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(brightness_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(brightness_container, 0);

    // Brightness slider
    brightness_slider = lv_slider_create(brightness_container);
    lv_obj_set_size(brightness_slider, lv_pct(100), 24);
    lv_slider_set_range(brightness_slider, 10, 255);
    lv_slider_set_value(brightness_slider, current_brightness, LV_ANIM_OFF);
    lv_obj_remove_flag(brightness_slider, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(brightness_slider, LV_OBJ_FLAG_CLICKABLE);

    // Apply custom slider styles
    lv_obj_add_style(brightness_slider, &rgb_brightness_slider, LV_PART_MAIN);
    lv_obj_add_style(brightness_slider, &rgb_brightness_slider_indicator, LV_PART_INDICATOR);
    lv_obj_add_style(brightness_slider, &rgb_brightness_slider_knob, LV_PART_KNOB);

    // Add focus and editing outline style for slider
    lv_obj_add_style(brightness_slider, &rgb_brightness_slider_focused, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_add_style(brightness_slider, &rgb_brightness_slider_editing, LV_PART_MAIN | LV_STATE_EDITED);

    lv_obj_add_event_cb(brightness_slider, rgb_led_brightness_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(brightness_slider, rgb_led_brightness_event_cb, LV_EVENT_KEY, NULL);
    nav_register(rgb_led_nav_ctx, brightness_slider);

    // Percentage label below slider
    brightness_percent_label = lv_label_create(brightness_container);
    int percentage           = (current_brightness * 100) / 255;
    lv_label_set_text_fmt(brightness_percent_label, "%d%%", percentage);
    lv_obj_set_style_text_color(brightness_percent_label, lv_color_hex(WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(brightness_percent_label, &clarity_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(brightness_percent_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_top(brightness_percent_label, 4, LV_PART_MAIN);

    // Create list for colors and patterns
    rgb_led_list = lv_list_create(rgb_led_overlay);
    lv_obj_set_size(rgb_led_list, lv_pct(100), lv_pct(100));
    lv_obj_add_style(rgb_led_list, &rgb_list, LV_PART_MAIN);
    lv_obj_set_style_radius(rgb_led_list, 0, LV_PART_SCROLLBAR);
    lv_obj_set_flex_grow(rgb_led_list, 1);

    // Add list items for colors and patterns
    const char *pattern_names[] = {
        // Individual colors/patterns
        "White",
        "Red",
        "Green",
        "Blue",
        "Yellow",
        "Cyan",
        "Magenta",
        "Rainbow",

        // Animated patterns
        "Chase",
        "Breathing",
    };
    int num_patterns = sizeof(pattern_names) / sizeof(pattern_names[0]);

    for (int i = 0; i < num_patterns; i++) {
        lv_obj_t *btn = lv_list_add_button(rgb_led_list, NULL, pattern_names[i]);
        lv_obj_add_style(btn, &rgb_list_button, LV_PART_MAIN);
        lv_obj_add_style(btn, &rgb_list_button_focused, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_add_event_cb(btn, rgb_led_pattern_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        // Add an icon on the right to show if this pattern is currently active
        lv_obj_t *active_icon = lv_image_create(btn);
        lv_obj_set_size(active_icon, 16, 16);
        lv_image_set_inner_align(active_icon, LV_IMAGE_ALIGN_CENTER);
        lv_image_set_src(active_icon, LV_SYMBOL_PLAY);
        lv_obj_set_style_text_font(active_icon, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(active_icon, lv_color_hex(WHITE), LV_PART_MAIN);
        lv_obj_add_flag(active_icon, LV_OBJ_FLAG_HIDDEN); // Initially hidden
        lv_obj_align(active_icon, LV_ALIGN_RIGHT_MID, -8, 0);
    }
    nav_register_children(rgb_led_nav_ctx, rgb_led_list);
    lv_obj_scroll_to_y(rgb_led_list, 0, LV_ANIM_OFF);

    lv_obj_t *bottom_buttons = lv_obj_create(rgb_led_overlay);
    lv_obj_add_style(bottom_buttons, &button_row, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(bottom_buttons, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(bottom_buttons, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(bottom_buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_grow(bottom_buttons, 0);

    // Back button
    lv_obj_t *back_btn = lv_button_create(rgb_led_overlay);
    lv_obj_add_style(back_btn, &test_button_style, LV_PART_MAIN);
    lv_obj_add_style(back_btn, &rgb_back_button, LV_PART_MAIN);
    lv_obj_add_style(back_btn, &rgb_back_button_focused, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_add_event_cb(back_btn, rgb_led_pattern_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
    nav_register(rgb_led_nav_ctx, back_btn);

    // Focus the first list item and apply the first pattern (White)
    nav_focus(rgb_led_nav_ctx, rgb_led_list);
    rgb_led_apply_pattern(0);
}

static void rgb_led_brightness_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *slider     = (lv_obj_t *)lv_event_get_target(e);

    ESP_LOGI(TAG, "Slider event: %d, current value: %d", code, lv_slider_get_value(slider));

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        ESP_LOGI(TAG, "Slider key pressed: %d", key);

        // Handle left/right arrow keys for slider adjustment
        if (key == LV_KEY_LEFT) {
            int new_value = lv_slider_get_value(slider) - 10;
            if (new_value < 10) {
                new_value = 10;
            }
            lv_slider_set_value(slider, new_value, LV_ANIM_OFF);
        } else if (key == LV_KEY_RIGHT) {
            int new_value = lv_slider_get_value(slider) + 10;
            if (new_value > 255) {
                new_value = 255;
            }
            lv_slider_set_value(slider, new_value, LV_ANIM_OFF);
        }
    }

    if (code == LV_EVENT_VALUE_CHANGED || code == LV_EVENT_KEY) {
        current_brightness = lv_slider_get_value(slider);

        // Round to increments of 10
        current_brightness = ((current_brightness + 5) / 10) * 10;
        if (current_brightness < 10) {
            current_brightness = 10;
        }
        if (current_brightness > 255) {
            current_brightness = 255;
        }

        // Update slider if we rounded
        if (lv_slider_get_value(slider) != current_brightness) {
            lv_slider_set_value(slider, current_brightness, LV_ANIM_OFF);
        }

        // Update the LED component's global brightness
        led_set_brightness(current_brightness);

        // Update percentage label
        int percentage = (current_brightness * 100) / 255;
        lv_label_set_text_fmt(brightness_percent_label, "%d%%", percentage);

        // Request a refresh so current layer picks up brightness change without resetting mode
        led_layer_request_refresh();

        ESP_LOGI(TAG, "RGB LED brightness changed to: %d (%d%%)", current_brightness, percentage);
    }
}

static void rgb_led_pattern_event_cb(lv_event_t *e) {
    int pattern = (int)(intptr_t)lv_event_get_user_data(e);

    ESP_LOGI(TAG, "RGB LED pattern selected: %d", pattern);

    // Handle back button
    if (pattern == -1) {
        rgb_led_test_end();
        return;
    }

    rgb_led_apply_pattern(pattern);
}

// Build/buffer a static pattern frame and switch mode (placed before rgb_led_apply_pattern to avoid implicit warnings)
static void hwtest_apply_static_pattern(int pattern_id) {
    hwtest_led_state.static_frame_pattern_id = pattern_id;
    switch (pattern_id) {
        case 0:
            for (int i = 0; i < NUM_LEDS; i++) {
                hwtest_led_state.static_frame[i][0] = 255;
                hwtest_led_state.static_frame[i][1] = 255;
                hwtest_led_state.static_frame[i][2] = 255;
            }
            break; // White
        case 1:
            for (int i = 0; i < NUM_LEDS; i++) {
                hwtest_led_state.static_frame[i][0] = 255;
                hwtest_led_state.static_frame[i][1] = 0;
                hwtest_led_state.static_frame[i][2] = 0;
            }
            break; // Red
        case 2:
            for (int i = 0; i < NUM_LEDS; i++) {
                hwtest_led_state.static_frame[i][0] = 0;
                hwtest_led_state.static_frame[i][1] = 255;
                hwtest_led_state.static_frame[i][2] = 0;
            }
            break; // Green
        case 3:
            for (int i = 0; i < NUM_LEDS; i++) {
                hwtest_led_state.static_frame[i][0] = 0;
                hwtest_led_state.static_frame[i][1] = 0;
                hwtest_led_state.static_frame[i][2] = 255;
            }
            break; // Blue
        case 4:
            for (int i = 0; i < NUM_LEDS; i++) {
                hwtest_led_state.static_frame[i][0] = 255;
                hwtest_led_state.static_frame[i][1] = 255;
                hwtest_led_state.static_frame[i][2] = 0;
            }
            break; // Yellow
        case 5:
            for (int i = 0; i < NUM_LEDS; i++) {
                hwtest_led_state.static_frame[i][0] = 0;
                hwtest_led_state.static_frame[i][1] = 255;
                hwtest_led_state.static_frame[i][2] = 255;
            }
            break; // Cyan
        case 6:
            for (int i = 0; i < NUM_LEDS; i++) {
                hwtest_led_state.static_frame[i][0] = 255;
                hwtest_led_state.static_frame[i][1] = 0;
                hwtest_led_state.static_frame[i][2] = 255;
            }
            break; // Magenta
        case 7:
        default: {
            for (int i = 0; i < NUM_LEDS; i++) {
                int hue = (i * 360) / NUM_LEDS;
                uint8_t r, g, b;
                if (hue < 60) {
                    r = 255;
                    g = (hue * 255) / 60;
                    b = 0;
                } else if (hue < 120) {
                    r = ((120 - hue) * 255) / 60;
                    g = 255;
                    b = 0;
                } else if (hue < 180) {
                    r = 0;
                    g = 255;
                    b = ((hue - 120) * 255) / 60;
                } else if (hue < 240) {
                    r = 0;
                    g = ((240 - hue) * 255) / 60;
                    b = 255;
                } else if (hue < 300) {
                    r = ((hue - 240) * 255) / 60;
                    g = 0;
                    b = 255;
                } else {
                    r = 255;
                    g = 0;
                    b = ((360 - hue) * 255) / 60;
                }
                hwtest_led_state.static_frame[i][0] = r;
                hwtest_led_state.static_frame[i][1] = g;
                hwtest_led_state.static_frame[i][2] = b;
            }
        } break;
    }
    hwtest_led_state.mode = HWTEST_LED_MODE_STATIC_FRAME;
}

static void rgb_led_apply_pattern(int pattern) {
    // Stop any existing animation timer before starting a new pattern
    if (rgb_animation_timer != NULL) {
        lv_timer_delete(rgb_animation_timer);
        rgb_animation_timer = NULL;
        ESP_LOGI(TAG, "Stopped previous RGB animation timer");
    }

    last_applied_pattern = pattern;

    // Update the visual indicator to show which pattern is active
    rgb_led_update_active_indicator(pattern);

    // Update global LED brightness (render callback will pick this up next frame)
    led_set_brightness(current_brightness);

    switch (pattern) {
        case 8: // Chase pattern (animated)
            rgb_led_start_chase_pattern();
            break;
        case 9: // Breathing pattern (animated)
            rgb_led_start_breathing_pattern();
            break;
        default: // All static patterns 0-7
            hwtest_apply_static_pattern(pattern);
            break;
    }
}

static void rgb_led_update_active_indicator(int active_pattern) {
    if (rgb_led_list == NULL) {
        return;
    }

    // Show the icon only for the active pattern
    uint32_t child_count = lv_obj_get_child_count(rgb_led_list);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *item = lv_obj_get_child(rgb_led_list, i);
        if (item != NULL) {
            lv_obj_t *icon = lv_obj_get_child(item, -1);
            if (icon != NULL) {
                if (i == active_pattern) {
                    lv_obj_remove_flag(icon, LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(icon, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
    }
}

static void rgb_led_start_chase_pattern() {
    hwtest_led_state.mode           = HWTEST_LED_MODE_CHASE;
    hwtest_led_state.last_update_ms = hwtest_now_ms();
    // reset chase parameters
    hwtest_led_state.chase_pos     = 0;
    hwtest_led_state.chase_forward = true;
    hwtest_led_state.color_offset  = 0;
    hwtest_led_state.frame_counter = 0;
}

static void rgb_led_start_breathing_pattern() {
    hwtest_led_state.mode           = HWTEST_LED_MODE_BREATH;
    hwtest_led_state.last_update_ms = hwtest_now_ms();
    hwtest_led_state.breath_step    = 0;
    hwtest_led_state.breathing_up   = true;
    hwtest_led_state.frame_counter  = 0;
}

static void rgb_led_test_end() {
    // Switch back to static white (or off) within layer
    hwtest_led_state.mode = HWTEST_LED_MODE_STATIC_WHITE;

    // Set test as passed
    hwtest_set_result(HWTEST_RGB_LEDS, HWTEST_RESULT_PASSED);

    // Pop navigation scope before cleanup
    nav_scope_pop(input_get_device(), rgb_led_nav_guard);

    // Safely destroy the context now that the scope is popped
    if (rgb_led_nav_ctx != NULL) {
        nav_ctx_destroy(rgb_led_nav_ctx);
        rgb_led_nav_ctx = NULL;
    }

    // Safely delete the group
    if (rgb_led_nav_group != NULL) {
        lv_group_delete(rgb_led_nav_group);
        rgb_led_nav_group = NULL;
    }

    if (rgb_led_overlay != NULL) {
        lv_obj_delete(rgb_led_overlay);
        rgb_led_overlay = NULL;
    }

    // Reset references
    rgb_led_list             = NULL;
    brightness_slider        = NULL;
    brightness_percent_label = NULL;
    last_applied_pattern     = -1;
}

// ---------------------------------------------------------------------------------
// Layer render callback for hardware test LED patterns
// ---------------------------------------------------------------------------------
static void hwtest_led_render_cb(uint32_t now_ms, void *user_ctx) {
    hwtest_led_state_t *st = (hwtest_led_state_t *)user_ctx;
    // static uint32_t dbg_frames = 0;
    // if ((dbg_frames++ % 50) == 0) {
    //     ESP_LOGI(TAG, "LED render layer=%d my_handle=%d mode=%d chase_pos=%d breath_step=%d", led_layer_current(),
    //              hwtest_led_layer_handle, st->mode, st->chase_pos, st->breath_step);
    // }
    switch (st->mode) {
        case HWTEST_LED_MODE_STATIC_WHITE: {
            for (int i = 0; i < NUM_LEDS; i++) {
                led_set_with_brightness(i, 255, 255, 255, LED_HWTEST_WHITE_BRIGHTNESS);
            }
            break;
        }
        case HWTEST_LED_MODE_CHASE: {
// Deterministic frame-based stepping to guarantee motion even if timing logic fails.
// Advance every CHASE_STEP_FRAMES frames.
#ifndef CHASE_STEP_FRAMES
    #define CHASE_STEP_FRAMES 3
#endif
            bool do_step = true; // force step every frame for diagnostics
            // Clear previously lit positions only (reduces flicker)
            for (int i = 0; i < st->last_chase_count; i++) {
                int p = st->last_chase_positions[i];
                if (p >= 0 && p < NUM_LEDS) {
                    led_set_with_brightness(p, 0, 0, 0, 0);
                }
            }
            st->last_chase_count = 0;
            for (int trail = 0; trail < 5; trail++) {
                int pos = st->chase_forward ? (st->chase_pos - trail) : (st->chase_pos + trail);
                if (pos < 0 || pos >= NUM_LEDS) {
                    continue;
                }
                uint8_t brightness = ((255 - (trail * 50)) * current_brightness) / 255;
                int hue            = (st->color_offset + (pos * 30)) % 360;
                uint8_t r, g, b;
                if (hue < 60) {
                    r = 255;
                    g = (hue * 255) / 60;
                    b = 0;
                } else if (hue < 120) {
                    r = ((120 - hue) * 255) / 60;
                    g = 255;
                    b = 0;
                } else if (hue < 180) {
                    r = 0;
                    g = 255;
                    b = ((hue - 120) * 255) / 60;
                } else if (hue < 240) {
                    r = 0;
                    g = ((240 - hue) * 255) / 60;
                    b = 255;
                } else if (hue < 300) {
                    r = ((hue - 240) * 255) / 60;
                    g = 0;
                    b = 255;
                } else {
                    r = 255;
                    g = 0;
                    b = ((360 - hue) * 255) / 60;
                }
                led_set_with_brightness(pos, r, g, b, brightness);
                if (st->last_chase_count < 5) {
                    st->last_chase_positions[st->last_chase_count++] = pos;
                }
            }
            if (do_step) {
                if (st->chase_forward) {
                    st->chase_pos++;
                    if (st->chase_pos >= NUM_LEDS - 1) {
                        st->chase_forward = false;
                    }
                } else {
                    st->chase_pos--;
                    if (st->chase_pos <= 0) {
                        st->chase_forward = true;
                    }
                }
                st->color_offset = (st->color_offset + 8) % 360; // slightly faster color cycle
            }
            break;
        }
        case HWTEST_LED_MODE_STATIC_FRAME: {
            // Draw buffered static frame (apply current_brightness scaling)
            for (int i = 0; i < NUM_LEDS; i++) {
                uint8_t r = (hwtest_led_state.static_frame[i][0] * current_brightness) / 255;
                uint8_t g = (hwtest_led_state.static_frame[i][1] * current_brightness) / 255;
                uint8_t b = (hwtest_led_state.static_frame[i][2] * current_brightness) / 255;
                led_set(i, r, g, b);
            }
            break;
        }
        case HWTEST_LED_MODE_BREATH: {
            bool do_step     = false;
            uint32_t elapsed = now_ms - st->last_update_ms;
            if (elapsed >= 30) {
                do_step            = true;
                st->last_update_ms = now_ms;
                st->frame_counter  = 0;
            } else if (++st->frame_counter >= 2) { // fallback every ~80ms
                do_step            = true;
                st->frame_counter  = 0;
                st->last_update_ms = now_ms;
            }
            if (do_step) {
                if (st->breathing_up) {
                    if (st->breath_step >= 50) {
                        st->breathing_up = false;
                    }
                } else {
                    if (st->breath_step >= 100) {
                        st->breath_step  = -1;
                        st->breathing_up = true;
                    }
                }
                st->breath_step++;
            }
            int local_step = st->breath_step;
            if (!st->breathing_up) {
                if (local_step > 50) {
                    local_step = 100 - local_step;
                }
            }
            if (local_step < 0) {
                local_step = 0;
            }
            if (local_step > 50) {
                local_step = 50;
            }
            uint8_t breath_brightness = (local_step * 255) / 50;
            uint8_t brightness        = (breath_brightness * current_brightness) / 255;
            for (int i = 0; i < NUM_LEDS; i++) {
                led_set_with_brightness(i, 255, 255, 255, brightness);
            }
            break;
        }
        default: break;
    }
}
