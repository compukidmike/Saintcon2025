idf_component_register(SRCS "i2c_manager.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "driver" "esp_event" "esp_timer" "helpers")

target_include_directories(${COMPONENT_LIB} PRIVATE
    ${IDF_PATH}/components/esp_driver_i2c
)