// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui_theme.h"
#include "ui_sprite.h"
#include "pet_model.h"
#include "weather_model.h"
typedef void (*tt_garden_touch_t)(void);
lv_obj_t *tt_garden_create(lv_obj_t *parent,int x,int y,int height,tt_pet_t pet,uint8_t language,tt_garden_touch_t touch);
lv_obj_t *tt_garden_fullscreen(lv_obj_t *parent,tt_pet_t pet,uint8_t language,tt_garden_touch_t touch);
void tt_garden_weather(lv_obj_t *garden,tt_weather_kind_t weather);
void tt_garden_environment(lv_obj_t *garden,tt_scene_t scene);
void tt_garden_companion_visible(lv_obj_t *garden,bool visible);
void tt_garden_set(lv_obj_t *garden,tt_pet_t pet,int mood,uint8_t language,const tt_pet_state_t *state);
void tt_garden_react(lv_obj_t *garden,tt_pet_action_t action,const char *message);
void tt_garden_position(lv_obj_t *garden,int *x,int *y);

void tt_garden_say(lv_obj_t *garden,const char *message);

void tt_garden_walk_to(lv_obj_t *garden,int x,int y);
bool tt_garden_busy(lv_obj_t *garden);
tt_animation_t tt_garden_animation(lv_obj_t *garden);
