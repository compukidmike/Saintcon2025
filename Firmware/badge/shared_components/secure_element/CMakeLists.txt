idf_component_register(SRCS "secure_element.c" "se_debug.c" "utils.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "esp-cryptoauthlib" "i2c_manager" "mbedtls" "nvs")