// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum {
    TT_WEATHER_UNKNOWN, TT_WEATHER_CLEAR, TT_WEATHER_CLOUDY,
    TT_WEATHER_RAIN, TT_WEATHER_SNOW, TT_WEATHER_FOG, TT_WEATHER_STORM,
    TT_WEATHER_KINDS
} tt_weather_kind_t;
tt_weather_kind_t tt_weather_kind(int code);
const char *tt_weather_text(int code);
bool tt_weather_coordinates(const char *latitude,const char *longitude,int32_t *lat_e6,int32_t *lon_e6);
