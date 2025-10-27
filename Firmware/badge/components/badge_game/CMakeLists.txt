set(component_sources)
file(GLOB_RECURSE component_sources ${CMAKE_CURRENT_SOURCE_DIR}/ *.c)

idf_component_register(SRCS ${component_sources}
                       INCLUDE_DIRS "include"
                       REQUIRES "badge" "display" "lvgl" "ui")