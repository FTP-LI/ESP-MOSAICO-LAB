// SPDX-License-Identifier: GPL-3.0-only
#include "scene_model.h"
tt_scene_t tt_scene_for_time(int month,int hour,bool southern)
{
    if (month<1 || month>12) month=4;
    if (hour<0 || hour>23) hour=12;
    int season=(month%12)/3;
    season=(season+3)%4; /* March-May = spring. */
    if (southern) season=(season+2)%4;
    return (tt_scene_t){(tt_season_t)season,
        hour<5 || hour>=20?TT_NIGHT:hour<8?TT_DAWN:hour<17?TT_DAY:TT_DUSK};
}
