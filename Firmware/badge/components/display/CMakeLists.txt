idf_component_register(SRCS "display.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "badge" "driver" "input" "lvgl" "esp_lcd" "i2c_manager")
