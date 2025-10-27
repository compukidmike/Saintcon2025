idf_component_register(SRCS "input.c" "touch.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "driver" "esp_lcd" "i2c_manager")