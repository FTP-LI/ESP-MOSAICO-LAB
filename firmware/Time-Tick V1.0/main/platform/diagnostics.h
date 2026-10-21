// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"
void tt_diagnostics_start(lv_display_t *display);
void tt_diagnostics_ui(bool acquired);
void tt_diagnostics_ui_heartbeat(void);
void tt_diagnostics_display_request(int *rotation,unsigned *brightness);
void tt_diagnostics_tick(int64_t now);
