cmake_minimum_required(VERSION 3.24)
project(time_tick_preview LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT MINGW OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Preview currently uses Windows x64 MinGW GCC; firmware uses ESP-IDF separately.")
endif()

# Reuse the exact LVGL resolved by this firmware's dependencies.lock.
set(LVGL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/lvgl__lvgl" CACHE PATH "Firmware LVGL source")
if(NOT EXISTS "${LVGL_DIR}/lvgl.h")
    message(FATAL_ERROR "Resolve firmware dependencies with the official ESP-IDF extension first.")
endif()
file(READ "${LVGL_DIR}/lv_version.h" LVGL_VERSION_HEADER)
if(NOT LVGL_VERSION_HEADER MATCHES "LVGL_VERSION_MAJOR 9" OR
   NOT LVGL_VERSION_HEADER MATCHES "LVGL_VERSION_MINOR 5" OR
   NOT LVGL_VERSION_HEADER MATCHES "LVGL_VERSION_PATCH 0")
    message(FATAL_ERROR "This preview is validated with the firmware's LVGL 9.5.0.")
endif()

include(FetchContent)
FetchContent_Declare(sdl2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-mingw.zip
    URL_HASH SHA256=f15cff5fca62ec9381a016ef1d42a95c638cd72d2f226ba5781c76fe43dbd1ac
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(sdl2)
set(SDL2_ROOT "${sdl2_SOURCE_DIR}/x86_64-w64-mingw32")
add_library(tt_sdl2 SHARED IMPORTED)
set_target_properties(tt_sdl2 PROPERTIES
    IMPORTED_IMPLIB "${SDL2_ROOT}/lib/libSDL2.dll.a"
    IMPORTED_LOCATION "${SDL2_ROOT}/bin/SDL2.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL2_ROOT}/include/SDL2")
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${LVGL_DIR}" lvgl)
target_link_libraries(lvgl PUBLIC tt_sdl2)

include("${CMAKE_CURRENT_SOURCE_DIR}/../main/sources.cmake")
set(TT_CJSON "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/espressif__cjson/cJSON")
add_executable(time_tick_preview main.c location_pc.c "${TT_CJSON}/cJSON.c" ${TT_SHARED_SOURCES})
target_include_directories(time_tick_preview PRIVATE ${TT_INCLUDE_DIRS} "${TT_CJSON}")
target_compile_definitions(time_tick_preview PRIVATE SDL_MAIN_HANDLED)
target_compile_options(time_tick_preview PRIVATE -Wall -Wextra -Werror)
target_link_options(time_tick_preview PRIVATE -static-libgcc)
target_link_libraries(time_tick_preview PRIVATE lvgl winhttp)
add_custom_command(TARGET time_tick_preview POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:tt_sdl2>" "$<TARGET_FILE_DIR:time_tick_preview>")

enable_testing()
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../tests" core-tests)
add_test(NAME preview_smoke COMMAND time_tick_preview --smoke)
add_test(NAME preview_configured COMMAND time_tick_preview --smoke-configured)
add_test(NAME preview_timer COMMAND time_tick_preview --smoke-timer)
add_test(NAME preview_weather COMMAND time_tick_preview --smoke-weather)
add_test(NAME preview_scene COMMAND time_tick_preview --smoke-scene)
add_test(NAME preview_setup COMMAND time_tick_preview --smoke-setup)
add_test(NAME preview_brightness COMMAND time_tick_preview --smoke-brightness)
add_test(NAME preview_theme COMMAND time_tick_preview --smoke-theme)
set_tests_properties(preview_smoke preview_configured preview_timer preview_weather preview_setup preview_scene preview_brightness preview_theme PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy" TIMEOUT 30)
