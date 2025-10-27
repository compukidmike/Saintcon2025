# Operations/tasks related to the badge
set(ops_sources)
file(GLOB_RECURSE ops_sources ops/ *.c)

idf_component_register(SRCS "nut.c" "config.c" "migrate.c" "ota.c" "types.c" "version.c" "code_manager.c" ${ops_sources}
                       INCLUDE_DIRS "include" "include/nut"
                       REQUIRES "api" "led" "nvs" "secure_element" "wifi_manager" "network_manager")
