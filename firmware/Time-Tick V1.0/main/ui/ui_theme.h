// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "lvgl.h"
#include "settings.h"
#include <time.h>

/* Native-resolution illustrated artwork. All coordinates are native 480 x 480 pixels. */
#define TT_INK     0x3e493c
#define TT_WOOD    0xb67948
#define TT_PAPER   0xf7f4e9
#define TT_CREAM   0xfffcf0
#define TT_GREEN   0x527a55
#define TT_MUTED   0x80715b

/* UI-thread only. Automatic mode retains its palette until the clock is valid;
 * forced modes work offline. Color-only updates preserve editing state. */
bool tt_theme_time(time_t local_time,const tt_theme_config_t *config);
bool tt_theme_is_night(void);
uint32_t tt_theme_color(lv_style_prop_t property,uint32_t day_color);
void tt_theme_bind(lv_obj_t *obj,lv_style_prop_t property,uint32_t day_color,lv_style_selector_t selector);
void tt_theme_input(lv_obj_t *obj);
void tt_theme_check(lv_obj_t *obj);
void tt_theme_dropdown(lv_obj_t *list);

void tt_theme_backdrop(lv_obj_t *screen);
/* Polling service snapshots must not relayout unchanged text. */
void tt_label_set_text(lv_obj_t *label,const char *text);
lv_obj_t *tt_theme_weather(lv_obj_t *parent,int x,int y);
void tt_theme_weather_set(lv_obj_t *obj,int code);
