#include "input.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

#include "display.h"
#include "minibadge.h"
#include "i2c_manager.h"
#ifdef CONFIG_INPUT_TOUCH_ENABLED
    #include "esp_lcd_touch.h"
#endif

static const char *TAG __attribute__((unused)) = "input";

static lv_indev_t *g_indev = NULL;

#ifdef CONFIG_INPUT_JOYSTICK_ENABLED
// Minibadge D-pad state tracking
static uint32_t minibadge_last_key                                 = 0;
static bool minibadge_key_pressed                                  = false;
static minibadge_slot_t minibadge_dpad_slots[MINIBADGE_SLOT_COUNT] = {0};
static int minibadge_dpad_count                                    = 0;
static minibadge_slot_t minibadge_dpad_active_slot                 = (minibadge_slot_t)(-1); // only one logical D-pad supported
#endif

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
// Map minibadge D-pad state to LVGL key codes
static uint32_t minibadge_dpad_state_to_key(minibadge_dpad_state_t state) {
    switch (state) {
        case MINIBADGE_DPAD_UP: return LV_KEY_UP;
        case MINIBADGE_DPAD_DOWN: return LV_KEY_DOWN;
        case MINIBADGE_DPAD_LEFT: return LV_KEY_LEFT;
        case MINIBADGE_DPAD_RIGHT: return LV_KEY_RIGHT;
        case MINIBADGE_DPAD_PRESS: return LV_KEY_ENTER;
        default: return 0;
    }
}

// Event handler for minibadge D-pad events
static void minibadge_dpad_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data) {
    if (id == MINIBADGE_DPAD_EVENT_PRESS) {
        minibadge_dpad_event_t *event = (minibadge_dpad_event_t *)event_data;

        if (event->state == MINIBADGE_DPAD_NONE) {
            // Button released
            minibadge_key_pressed = false;
            minibadge_last_key    = 0;
        } else {
            // Button pressed
            minibadge_last_key    = minibadge_dpad_state_to_key(event->state);
            minibadge_key_pressed = (minibadge_last_key != 0);

            if (minibadge_key_pressed) {
                screen_timeout_reset();
                ESP_LOGD(TAG, "Minibadge D-pad [slot %d]: key=%lu", event->slot + 1, minibadge_last_key);
            }
        }
    }
}

// Callback for minibadge insertion/removal events
static void minibadge_insertion_callback(minibadge_event_t event) {
    // Treat the bus as hosting at most one logical D-pad. Any INSERT/REPLACED event triggers a probe if inactive.
    if (event.type == MINIBADGE_EVENT_INSERTED || event.type == MINIBADGE_EVENT_REPLACED) {
        if (minibadge_dpad_count == 0) {
            i2c_manager_device_config_t probe_cfg = {
                .bus_index = I2C_BUS_B,
                .config =
                    {
                        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                        .device_address  = MINIBADGE_I2C_ADDR_DPAD,
                        .scl_speed_hz    = 100000,
                    },
            };
            if (i2c_manager_ping(&probe_cfg) == ESP_OK) {
                minibadge_dpad_slots[0]    = event.slot;
                minibadge_dpad_active_slot = event.slot;
                minibadge_dpad_count       = 1;
                minibadge_dpad_poll(true, event.slot);
                ESP_LOGI(TAG, "D-pad polling enabled");
            }
        }
    } else if (event.type == MINIBADGE_EVENT_REMOVED) {
        // On any removal, if active, stop. We rely on future INSERT to restart.
        if (minibadge_dpad_count > 0) {
            minibadge_dpad_poll(false, minibadge_dpad_active_slot);
            minibadge_dpad_count       = 0;
            minibadge_dpad_active_slot = (minibadge_slot_t)(-1);
            ESP_LOGI(TAG, "D-pad polling disabled (removal)");
        }
    }
}

static void joystick_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    static uint32_t last_key = 0;
    bool up                  = (gpio_get_level(CONFIG_INPUT_JOY_UP_GPIO) == 0);
    bool down                = (gpio_get_level(CONFIG_INPUT_JOY_DOWN_GPIO) == 0);
    bool left                = (gpio_get_level(CONFIG_INPUT_JOY_LEFT_GPIO) == 0);
    bool right               = (gpio_get_level(CONFIG_INPUT_JOY_RIGHT_GPIO) == 0);
    bool center              = (gpio_get_level(CONFIG_INPUT_JOY_CENTER_GPIO) == 0);

    // Merge hardware joystick and minibadge D-pad inputs
    bool any_pressed = up || down || left || right || center || minibadge_key_pressed;

    data->state = LV_INDEV_STATE_RELEASED;
    data->key   = last_key;

    if (any_pressed) {
        data->state = LV_INDEV_STATE_PRESSED;

        // Prioritize hardware joystick, but allow minibadge if hardware is idle
        if (up) {
            last_key = data->key = LV_KEY_UP;
        } else if (down) {
            last_key = data->key = LV_KEY_DOWN;
        } else if (left) {
            last_key = data->key = LV_KEY_LEFT;
        } else if (right) {
            last_key = data->key = LV_KEY_RIGHT;
        } else if (center) {
            last_key = data->key = LV_KEY_ENTER;
        } else if (minibadge_key_pressed) {
            // Use minibadge input when hardware joystick is idle
            last_key = data->key = minibadge_last_key;
        }

        screen_timeout_reset();
    } else if (last_key != 0) {
        screen_timeout_reset();
        last_key = 0; // Reset last key when no button is pressed
    }
}
#endif // CONFIG_INPUT_JOYSTICK_ENABLED

void input_init(lv_display_t *display) {
#ifdef CONFIG_INPUT_TOUCH_ENABLED
    init_touch_panel();
    lv_indev_t *touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(touch_indev, display);
    lv_indev_set_read_cb(touch_indev, lvgl_touch_cb);
    lv_indev_set_mode(touch_indev, LV_INDEV_MODE_TIMER);
    g_indev = touch_indev;
#endif

#ifdef CONFIG_INPUT_JOYSTICK_ENABLED
    static const gpio_num_t gpio_list[] = {
        CONFIG_INPUT_JOY_UP_GPIO,     //
        CONFIG_INPUT_JOY_DOWN_GPIO,   //
        CONFIG_INPUT_JOY_LEFT_GPIO,   //
        CONFIG_INPUT_JOY_RIGHT_GPIO,  //
        CONFIG_INPUT_JOY_CENTER_GPIO, //
    };

    // configure GPIOs
    gpio_config_t io = {
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    uint64_t pin_mask = 0;
    for (size_t i = 0; i < sizeof(gpio_list) / sizeof(gpio_list[0]); i++) {
        pin_mask |= (1ULL << gpio_list[i]);
    }
    io.pin_bit_mask = pin_mask;
    gpio_config(&io);

    // Enable pressing to wake from light sleep
    for (size_t i = 0; i < sizeof(gpio_list) / sizeof(gpio_list[0]); i++) {
        gpio_wakeup_enable(gpio_list[i], GPIO_INTR_LOW_LEVEL);
    }
    // Arm GPIO wakeup
    if (esp_sleep_enable_gpio_wakeup() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable GPIO wakeup - wakeup triggers conflict");
    }

    lv_group_t *g = lv_group_create();
    lv_group_set_default(g);

    lv_indev_t *joy_indev = lv_indev_create();
    lv_indev_set_type(joy_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_group(joy_indev, g);
    lv_indev_set_read_cb(joy_indev, joystick_read_cb);
    g_indev = joy_indev;

    // Register minibadge callbacks for automatic D-pad detection
    minibadge_add_event_callback(minibadge_insertion_callback);
    // Register for D-pad events (deferred until minibadge_init if not ready yet)
    if (minibadge_register_dpad_handler(minibadge_dpad_event_handler, NULL) == ESP_OK) {
        ESP_LOGI(TAG, "Minibadge D-pad event handler registration requested");
    } else {
        ESP_LOGW(TAG, "Failed to request minibadge D-pad handler registration");
    }
#endif
}

lv_indev_t *input_get_device() {
    return g_indev;
}