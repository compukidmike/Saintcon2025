idf_component_register(SRCS "nfc.cpp" "ndef.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "i2c_manager" "st25dv")