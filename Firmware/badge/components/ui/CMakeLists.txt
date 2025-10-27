# Images
set(embedded_images)
file(GLOB_RECURSE embedded_images images/ *.c)

# Fonts
set(embedded_fonts)
file(GLOB_RECURSE embedded_fonts fonts/ *.c)

# Flows
set(flow_sources)
file(GLOB_RECURSE flow_sources flows/ *.c)

# Screens
set(screen_sources)
file(GLOB_RECURSE screen_sources screens/ *.c)

# Apps
set(app_sources)
file(GLOB_RECURSE app_sources apps/ *.c)

# Widgets and other UI components
set(widget_sources)
file(GLOB_RECURSE widget_sources widgets/ *.c)

idf_component_register(SRCS "loadanim.c" "nav.c" "theme.c" "ui_events.c" "ui.c" "screenshot.c" ${embedded_images} ${embedded_fonts} ${flow_sources} ${screen_sources} ${app_sources} ${widget_sources}
                       INCLUDE_DIRS "include" "widgets" "flows"
                       REQUIRES "api" "badge" "badge_game" "battery" "dawn_accord" "display" "i2c_manager" "input" "json" "led" "lvgl" "nfc" "secure_element" "wifi_manager")

include_directories(${CMAKE_BINARY_DIR}/include)
target_include_directories(${COMPONENT_LIB} PRIVATE ".")
