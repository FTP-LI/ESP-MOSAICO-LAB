// SPDX-License-Identifier: GPL-3.0-only
#include "scene_model.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(tt_scene_for_time(1,22,false).season==TT_WINTER);
    assert(tt_scene_for_time(3,6,false).season==TT_SPRING);
    assert(tt_scene_for_time(7,12,false).season==TT_SUMMER);
    assert(tt_scene_for_time(10,18,false).season==TT_AUTUMN);
    assert(tt_scene_for_time(1,22,true).season==TT_SUMMER);
    assert(tt_scene_for_time(3,6,false).daylight==TT_DAWN);
    assert(tt_scene_for_time(3,12,false).daylight==TT_DAY);
    assert(tt_scene_for_time(3,18,false).daylight==TT_DUSK);
    assert(tt_scene_for_time(3,22,false).daylight==TT_NIGHT);
    puts("PASS: seasonal/daylight mapping");
    return 0;
}
