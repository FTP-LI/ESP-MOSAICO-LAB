// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "lvgl.h"
#include "weather_model.h"
#include "scene_model.h"
lv_obj_t *tt_atmosphere_create(lv_obj_t *parent,int width,int height);
void tt_atmosphere_set(lv_obj_t *obj,tt_weather_kind_t weather,tt_scene_t scene);
void tt_atmosphere_cached_tint(lv_obj_t *obj,bool cached);
