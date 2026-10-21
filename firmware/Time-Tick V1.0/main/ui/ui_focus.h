// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui_theme.h"

lv_obj_t *tt_focus_create(lv_obj_t *parent,lv_event_cb_t toggle,lv_event_cb_t stop);
void tt_focus_render(lv_obj_t *view,const tt_timer_t *timer,tt_face_t face,
                     const tt_settings_t *settings,bool alarm);
