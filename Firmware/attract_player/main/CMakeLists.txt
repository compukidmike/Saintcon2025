idf_component_register(
    SRCS "main.c" "lcd.c" "leds.c" "player.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_lcd
        esp_timer
        spiffs
        nvs_flash
        led_strip
        esp_jpeg
)

# Pack SPIFFS (attract.scmj) into the storage partition and flash with the app
spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
