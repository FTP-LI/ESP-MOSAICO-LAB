cmake_minimum_required(VERSION 3.16)

# Reuse the official, pinned Mosaico application and packaging integration.
get_filename_component(MOSAICO_UTILS_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../examples/esp-mosaico-vibe/submodule/esp-mosaico-utils"
    ABSOLUTE)
if(NOT EXISTS "${MOSAICO_UTILS_ROOT}/mosaico-tools/cmake/system_update.cmake")
    message(FATAL_ERROR "Initialize the official Vibe reference checkout; see README.md")
endif()
set(MOSAICO_BSP_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(MOSAICO_ESP_IRIS_ROOT "${MOSAICO_UTILS_ROOT}/ESP-Iris")
if(NOT SDKCONFIG)
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
endif()
if(NOT SDKCONFIG_DEFAULTS)
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.application.defaults")
endif()
list(APPEND EXTRA_COMPONENT_DIRS
    "${MOSAICO_ESP_IRIS_ROOT}/components/esp_iris"
    "${MOSAICO_UTILS_ROOT}/esp-mosaico-recovery/components/esp_mosaico_app_recovery")
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/version.txt" PROJECT_VER LIMIT_COUNT 1)
include("${MOSAICO_UTILS_ROOT}/esp-mosaico-recovery/cmake/mosaico_idf_project.cmake")
idf_build_set_property(MINIMAL_BUILD ON)
project(time_tick)
# IDF's project() does not forward CMake VERSION; use the built app version
# for the official bundle target so its release matches esp_app_desc.
idf_build_get_property(PROJECT_VERSION PROJECT_VER)
include("${MOSAICO_UTILS_ROOT}/mosaico-tools/cmake/system_update.cmake")
