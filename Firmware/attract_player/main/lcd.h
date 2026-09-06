#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include <stdint.h>

#define LCD_H_RES 240
#define LCD_V_RES 320

esp_err_t lcd_init(void);
esp_err_t lcd_draw_bitmap(int x1, int y1, int x2, int y2, const void *color_data);
void lcd_set_backlight(uint8_t level);
esp_lcd_panel_handle_t lcd_panel(void);
