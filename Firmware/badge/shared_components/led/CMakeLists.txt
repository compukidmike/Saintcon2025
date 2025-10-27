idf_component_register(SRCS "led.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "driver" "esp_timer" "led_strip")