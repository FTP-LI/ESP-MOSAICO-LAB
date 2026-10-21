// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "lvgl.h"
#include "scene_model.h"
#include "weather_model.h"
/* RGB565 scene tint, rebuilt only after a weather/time transition settles. */
bool tt_scene_background(lv_image_dsc_t *cache,const lv_image_dsc_t *source,
                         int visible_height,tt_weather_kind_t weather,tt_scene_t scene);
void tt_scene_background_free(lv_image_dsc_t *cache);
