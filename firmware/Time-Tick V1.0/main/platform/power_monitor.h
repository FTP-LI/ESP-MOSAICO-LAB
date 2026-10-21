// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "power_model.h"
#include "esp_err.h"
esp_err_t tt_power_start(void);
void tt_power_snapshot(tt_power_view_t *view);
void tt_power_usage_snapshot(tt_power_usage_t *usage);
