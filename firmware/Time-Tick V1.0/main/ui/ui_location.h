// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui.h"
#include "lvgl.h"
void tt_location_ui_create(lv_obj_t *parent,tt_settings_t *settings,tt_ui_submit_t submit,void *context);
void tt_location_ui_update(const tt_location_view_t *view);
void tt_location_ui_close(void);
