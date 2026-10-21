// SPDX-License-Identifier: GPL-3.0-only
#include "pet_animation.h"
void tt_animation_set(tt_animation_t *a,tt_animation_clip_t clip,bool left,uint32_t now)
{
    if (a->clip!=clip || (tt_animation_duration(clip) && !tt_animation_busy(a,now))) {
        a->clip=clip; a->started=now; a->distance_q8=0;
    }
    a->left=left;
}
void tt_animation_walk(tt_animation_t *a,uint32_t distance_q8)
{
    if (a->clip==TT_ANIM_WALK)
        a->distance_q8=(a->distance_q8+distance_q8)%(TT_STRIDE_PIXELS*256);
}
uint32_t tt_animation_duration(tt_animation_clip_t clip)
{
    return clip==TT_ANIM_HAPPY?900:clip==TT_ANIM_EAT?1800:clip==TT_ANIM_PLAY || clip==TT_ANIM_WASH?2700:0;
}
bool tt_animation_busy(const tt_animation_t *a,uint32_t now)
{
    uint32_t duration=tt_animation_duration(a->clip);
    return duration && (uint32_t)(now-a->started)<duration;
}
unsigned tt_animation_frame(const tt_animation_t *a,uint32_t now)
{
    if (a->clip==TT_ANIM_WALK) return a->distance_q8/(TT_STRIDE_PIXELS*256/TT_ANIMATION_FRAMES);
    if (tt_animation_busy(a,now)) return ((uint32_t)(now-a->started)/TT_ACTION_FRAME_MS)%TT_ANIMATION_FRAMES;
    return 0;
}
