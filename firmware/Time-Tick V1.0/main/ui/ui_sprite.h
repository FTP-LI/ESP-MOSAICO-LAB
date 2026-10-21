// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "lvgl.h"
#include "settings.h"
#include "pet_animation.h"
#include "scene_model.h"
#include "weather_model.h"
lv_obj_t *tt_sprite_create(lv_obj_t *parent,int x,int y,tt_pet_t type);
void tt_sprite_set(lv_obj_t *obj,tt_pet_t type,tt_animation_clip_t clip,bool left);
void tt_sprite_walk(lv_obj_t *obj,uint32_t distance_q8);
void tt_sprite_heart(lv_obj_t *obj);
void tt_sprite_environment(lv_obj_t *obj,tt_weather_kind_t weather,tt_scene_t scene);
bool tt_sprite_busy(lv_obj_t *obj);
tt_animation_t tt_sprite_animation(lv_obj_t *obj);
