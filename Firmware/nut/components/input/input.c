#include "input.h"
#include "esp_log.h"
#include "driver/gpio.h"

//#include "display.h"
#ifdef CONFIG_INPUT_TOUCH_ENABLED
    #include "esp_lcd_touch.h"
    #include "i2c_manager.h"
#endif

static const char *TAG __attribute__((unused)) = "input";

// static lv_indev_t *g_indev = NULL;

#ifdef CONFIG_INPUT_TOUCH_ENABLED
// Touch handle
static esp_lcd_touch_handle_t tp = NULL;

// copy+adapt your init_lcd_touch() here, minus the lv_indev bits…
static void init_touch_panel() {
    i2c_manager_bus_t *bus = i2c_manager_get_bus(I2C_BUS_TOUCH);
    assert(bus);
    display_orientation_params_t params = get_display_orientation_params();
    esp_lcd_touch_config_t cfg          = {
                 .x_max        = params.v_res,
                 .y_max        = params.h_res,
                 .sda_gpio_num = CONFIG_INPUT_TOUCH_SDA_GPIO,
                 .scl_gpio_num = CONFIG_INPUT_TOUCH_SCL_GPIO,
                 .int_gpio_num = (CONFIG_INPUT_TOUCH_INT_GPIO >= 0) ? CONFIG_INPUT_TOUCH_INT_GPIO : GPIO_NUM_NC,
                 .rst_gpio_num = (CONFIG_INPUT_TOUCH_RST_GPIO >= 0) ? CONFIG_INPUT_TOUCH_RST_GPIO : GPIO_NUM_NC,
                 .flags =
            {
                         .swap_xy  = params.swap_xy,
                         .mirror_x = params.mirror_x,
                         .mirror_y = params.mirror_y,
            },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(bus->handle, &cfg, &tp));
    ESP_LOGI(TAG, "Touch panel initialized");
}

// copy+adapt your lvgl_touch_cb() here:
static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    bool pressed = false;
    uint16_t x = 0, y = 0;
    esp_err_t err = esp_lcd_touch_read_data(tp);
    if (err == ESP_OK) {
        pressed = esp_lcd_touch_get_coordinates(tp, &x, &y, NULL, NULL, 1);
    }
    data->state   = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->point.x = x;
    data->point.y = y;
}
#endif // CONFIG_INPUT_TOUCH_ENABLED

#ifdef CONFIG_INPUT_JOYSTICK_ENABLED
// static void joystick_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
//     static uint32_t last_key = 0;
//     bool up                  = (gpio_get_level(CONFIG_INPUT_JOY_UP_GPIO) == 0);
//     bool down                = (gpio_get_level(CONFIG_INPUT_JOY_DOWN_GPIO) == 0);
//     bool left                = (gpio_get_level(CONFIG_INPUT_JOY_LEFT_GPIO) == 0);
//     bool right               = (gpio_get_level(CONFIG_INPUT_JOY_RIGHT_GPIO) == 0);
//     bool center              = (gpio_get_level(CONFIG_INPUT_JOY_CENTER_GPIO) == 0);

//     data->state = LV_INDEV_STATE_RELEASED;
//     data->key   = last_key;
//     if (up || down || left || right || center) {
//         data->state = LV_INDEV_STATE_PRESSED;
//         if (up)
//             last_key = data->key = LV_KEY_UP;
//         else if (down)
//             last_key = data->key = LV_KEY_DOWN;
//         else if (left)
//             last_key = data->key = LV_KEY_LEFT;
//         else if (right)
//             last_key = data->key = LV_KEY_RIGHT;
//         else if (center)
//             last_key = data->key = LV_KEY_ENTER;

//         screen_timeout_reset();
//     } else if (last_key != 0) {
//         screen_timeout_reset();
//         last_key = 0; // Reset last key when no button is pressed
//     }
// }
#endif // CONFIG_INPUT_JOYSTICK_ENABLED

// void input_init(lv_display_t *display) {
// #ifdef CONFIG_INPUT_TOUCH_ENABLED
//     init_touch_panel();
//     lv_indev_t *touch_indev = lv_indev_create();
//     lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
//     lv_indev_set_display(touch_indev, display);
//     lv_indev_set_read_cb(touch_indev, lvgl_touch_cb);
//     lv_indev_set_mode(touch_indev, LV_INDEV_MODE_TIMER);
//     g_indev = touch_indev;
// #endif

// #ifdef CONFIG_INPUT_JOYSTICK_ENABLED
//     // configure GPIOs
//     gpio_config_t io = {.mode         = GPIO_MODE_INPUT,
//                         .pull_up_en   = GPIO_PULLUP_ENABLE,
//                         .pull_down_en = GPIO_PULLDOWN_DISABLE,
//                         .intr_type    = GPIO_INTR_DISABLE};
//     io.pin_bit_mask  = (1ULL << CONFIG_INPUT_JOY_UP_GPIO) | (1ULL << CONFIG_INPUT_JOY_DOWN_GPIO) |
//                       (1ULL << CONFIG_INPUT_JOY_LEFT_GPIO) | (1ULL << CONFIG_INPUT_JOY_RIGHT_GPIO) |
//                       (1ULL << CONFIG_INPUT_JOY_CENTER_GPIO);
//     gpio_config(&io);

//     lv_group_t *g = lv_group_create();
//     lv_group_set_default(g);

//     lv_indev_t *joy_indev = lv_indev_create();
//     lv_indev_set_type(joy_indev, LV_INDEV_TYPE_KEYPAD);
//     lv_indev_set_group(joy_indev, g);
//     lv_indev_set_read_cb(joy_indev, joystick_read_cb);
//     g_indev = joy_indev;
// #endif
// }

// lv_indev_t *input_get_device() {
//     return g_indev;
// }