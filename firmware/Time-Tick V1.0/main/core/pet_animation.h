// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef enum { TT_ANIM_IDLE, TT_ANIM_WALK, TT_ANIM_HAPPY, TT_ANIM_SLEEP,
               TT_ANIM_EAT, TT_ANIM_PLAY, TT_ANIM_WASH } tt_animation_clip_t;
enum { TT_ANIMATION_FRAMES=6, TT_STRIDE_PIXELS=48, TT_ACTION_FRAME_MS=150 };
typedef struct {
    tt_animation_clip_t clip;
    uint32_t started, distance_q8;
    bool left;
} tt_animation_t;
void tt_animation_set(tt_animation_t *animation,tt_animation_clip_t clip,bool left,uint32_t now);
void tt_animation_walk(tt_animation_t *animation,uint32_t distance_q8);
unsigned tt_animation_frame(const tt_animation_t *animation,uint32_t now);
bool tt_animation_busy(const tt_animation_t *animation,uint32_t now);
uint32_t tt_animation_duration(tt_animation_clip_t clip);
