# One source list for both ESP-IDF and the PC preview.
set(TT_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(TT_INCLUDE_DIRS
    "${TT_SOURCE_ROOT}/core"
    "${TT_SOURCE_ROOT}/platform"
    "${TT_SOURCE_ROOT}/ui"
    "${TT_SOURCE_ROOT}/generated")
set(TT_SHARED_SOURCES
    core/timetick_core.c
    core/settings_model.c
    core/alarm_model.c
    core/pet_model.c
    core/pet_animation.c
    core/power_model.c
    core/weather_model.c
    core/location_model.c
    core/scene_model.c
    ui/ui.c
    ui/ui_focus.c
    ui/ui_theme.c
    ui/ui_power.c
    ui/ui_sprite.c
    ui/ui_pet_weather.c
    ui/ui_atmosphere.c
    ui/ui_scene_background.c
    ui/ui_text.c
    ui/ui_network.c
    ui/ui_location.c
    ui/ui_garden.c
    ui/ui_pet.c)
# Asset files are generated offline and checked into the source tree.
list(APPEND TT_SHARED_SOURCES
    generated/fonts/font_16.c
    generated/fonts/font_18.c
    generated/fonts/font_24.c
    generated/fonts/font_52.c
    generated/fonts/font_84.c
    generated/ui_brand.c
    generated/ui_daimao.c
    generated/ui_weather_art.c)
list(TRANSFORM TT_SHARED_SOURCES PREPEND "${TT_SOURCE_ROOT}/")
