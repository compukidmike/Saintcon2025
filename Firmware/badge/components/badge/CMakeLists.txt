# Operations/tasks related to the badge
set(ops_sources)
file(GLOB_RECURSE ops_sources ops/ *.c)

idf_component_register(SRCS "badge.c" "config.c" "migrate.c" "ota.c" "version.c" ${ops_sources}
                       INCLUDE_DIRS "include" "include/badge"
                       REQUIRES "api" "badge_game" "dawn_accord" "display" "led" "nfc" "nvs" "secure_element" "spiffs" "ui" "wifi_manager")
