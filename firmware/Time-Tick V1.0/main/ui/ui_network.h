// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui.h"
#include "lvgl.h"
/* One network page, owned by the settings screen. No Wi-Fi calls from LVGL. */
void tt_network_ui_create(lv_obj_t *parent, tt_settings_t *draft, tt_ui_submit_t submit, void *context);
void tt_network_ui_update(const tt_services_view_t *view);
void tt_network_ui_close(void);
bool tt_network_ui_attempted(void);
