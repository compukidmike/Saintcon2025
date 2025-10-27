# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Add shared_components to EXTRA_COMPONENT_DIRS
set(EXTRA_COMPONENT_DIRS ${CMAKE_SOURCE_DIR}/shared_components)

# Read the version from version.txt
file(READ "${CMAKE_SOURCE_DIR}/version.txt" VERSION_CONTENTS)
string(STRIP "${VERSION_CONTENTS}" VERSION_STRING)

# Split the version string into its components
string(REPLACE "." ";" VERSION_LIST ${VERSION_STRING})
list(GET VERSION_LIST 0 VERSION_MAJOR)
list(GET VERSION_LIST 1 VERSION_MINOR)
list(GET VERSION_LIST 2 VERSION_PATCH)

# Get the short git hash
execute_process(
    COMMAND git rev-parse --short=8 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE VERSION_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if (NOT VERSION_GIT_HASH)
    set(VERSION_GIT_HASH "unknown")
endif()

# Pass version variables to the configuration system
set(VERSION_MAJOR ${VERSION_MAJOR})
set(VERSION_MINOR ${VERSION_MINOR})
set(VERSION_PATCH ${VERSION_PATCH})
set(VERSION_GIT_HASH ${VERSION_GIT_HASH})
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}+${VERSION_GIT_HASH}")

# Set PROJECT_VER for ESP-IDF to incorporate into the esp_app_desc_t struct
set(PROJECT_VER ${VERSION_STRING})

# Configure the version.h file
configure_file(
    "${CMAKE_SOURCE_DIR}/components/badge/include/version.h.in"
    "${CMAKE_BINARY_DIR}/include/version.h"
    @ONLY
)

# Output the version information
message(STATUS "Firmware version: ${VERSION_STRING}")

# Include the build output include directory
include_directories(${CMAKE_BINARY_DIR}/include)

# Pull in the ESP-IDF build system and define the project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(badge)

# Function to silence warnings for a given component
function(silence_warnings_for component)
  idf_component_get_property(_lib ${component} COMPONENT_LIB)
  if(_lib AND TARGET ${_lib})
    message(STATUS "Silencing warnings for ${component} (target=${_lib})")
    target_compile_options(${_lib} PRIVATE ${ARGN})
  else()
    message(WARNING "Could not find component '${component}' to silence")
  endif()
endfunction()

# Silence warnings for LVGL
silence_warnings_for(lvgl__lvgl
  -Wno-char-subscripts
  -Wno-format
  -Wno-unused-function
)
