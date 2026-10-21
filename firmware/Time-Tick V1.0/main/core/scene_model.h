// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
typedef enum { TT_SPRING, TT_SUMMER, TT_AUTUMN, TT_WINTER } tt_season_t;
typedef enum { TT_DAWN, TT_DAY, TT_DUSK, TT_NIGHT } tt_daylight_t;
typedef struct { tt_season_t season; tt_daylight_t daylight; } tt_scene_t;
tt_scene_t tt_scene_for_time(int month,int hour,bool southern);
