// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "location_model.h"
void tt_pc_location_request(const tt_location_request_t *request);
void tt_pc_location_snapshot(tt_location_view_t *view);
/* Actual HTTPS smoke check; only invoked by an explicit CLI option. */
int tt_pc_location_check(void);
