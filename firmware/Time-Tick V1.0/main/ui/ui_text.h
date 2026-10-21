// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>
#include "lvgl.h"
LV_FONT_DECLARE(tt_font_16);
LV_FONT_DECLARE(tt_font_52);
LV_FONT_DECLARE(tt_font_84);
LV_FONT_DECLARE(tt_font_18);
LV_FONT_DECLARE(tt_font_24);
const char *tt_text(uint8_t language, const char *english);
