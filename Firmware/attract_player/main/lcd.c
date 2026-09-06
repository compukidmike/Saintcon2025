#include "lcd.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

static const char *TAG = "lcd";

/* Saintcon 2025 badge ST7789 I80 pinout */
#define PIN_DATA0 35
#define PIN_DATA1 48
#define PIN_DATA2 47
#define PIN_DATA3 21
#define PIN_DATA4 14
#define PIN_DATA5 13
#define PIN_DATA6 12
#define PIN_DATA7 11
#define PIN_DC    38
#define PIN_CS    39
#define PIN_RD    36
#define PIN_WR    37
#define PIN_RST   40
#define PIN_BL    18

#define LCD_PCLK_HZ (10 * 1000 * 1000)

static esp_lcd_panel_handle_t s_panel = NULL;

esp_err_t lcd_init(void) {
    /* Backlight off until panel is ready */
    ledc_timer_config_t bl_timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&bl_timer));
    ledc_channel_config_t bl_ch = {
        .gpio_num   = PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&bl_ch));

    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_cfg = {
        .clk_src        = LCD_CLK_SRC_PLL240M,
        .dc_gpio_num    = PIN_DC,
        .wr_gpio_num    = PIN_WR,
        .data_gpio_nums = {PIN_DATA0, PIN_DATA1, PIN_DATA2, PIN_DATA3, PIN_DATA4, PIN_DATA5, PIN_DATA6, PIN_DATA7},
        .bus_width      = 8,
        .max_transfer_bytes = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
        .dma_burst_size = 64,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_cfg, &i80_bus));

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_i80_config_t io_cfg = {
        .cs_gpio_num = PIN_CS,
        .pclk_hz     = LCD_PCLK_HZ,
        .trans_queue_depth = 10,
        .dc_levels =
            {
                .dc_idle_level  = 0,
                .dc_cmd_level   = 0,
                .dc_dummy_level = 0,
                .dc_data_level  = 1,
            },
        .flags =
            {
                .swap_color_bytes = 1,
            },
        .lcd_cmd_bits   = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_cfg, &io));

    gpio_set_direction(PIN_RD, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_RD, 1);

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &panel_cfg, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    /* ST7789 on this badge needs invert for true blacks (else white bg / dead purples) */
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_panel, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    lcd_set_backlight(255);
    ESP_LOGI(TAG, "ST7789 ready %dx%d", LCD_H_RES, LCD_V_RES);
    return ESP_OK;
}

esp_err_t lcd_draw_bitmap(int x1, int y1, int x2, int y2, const void *color_data) {
    return esp_lcd_panel_draw_bitmap(s_panel, x1, y1, x2, y2, color_data);
}

void lcd_set_backlight(uint8_t level) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, level);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

esp_lcd_panel_handle_t lcd_panel(void) {
    return s_panel;
}
