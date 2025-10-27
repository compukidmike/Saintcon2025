idf_component_register(SRCS "input.c" "touch.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "display" "driver" "esp_lcd" "i2c_manager" "lvgl" "minibadge")