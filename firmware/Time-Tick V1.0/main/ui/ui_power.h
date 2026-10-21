// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui_theme.h"
#include "power_model.h"
lv_obj_t *tt_power_badge(lv_obj_t *parent,lv_event_cb_t open);
void tt_power_details(lv_obj_t *parent,uint8_t language);
void tt_power_ui_update(const tt_power_view_t *power,uint8_t language);
unsigned tt_visual_period(bool atmosphere);
void tt_power_ui_activity(unsigned idle_ms,bool keep_active);
