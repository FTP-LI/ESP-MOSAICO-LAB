// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "lvgl.h"
#include "weather_model.h"
#include "pet_animation.h"
/* A small, non-interactive layer behind the existing pet artwork. */
lv_obj_t *tt_pet_weather_create(lv_obj_t *parent);
void tt_pet_weather_update(lv_obj_t *obj,int x,int y,tt_weather_kind_t weather,
                          tt_animation_clip_t clip,bool moving);
