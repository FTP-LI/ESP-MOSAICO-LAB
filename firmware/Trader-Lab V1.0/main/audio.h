// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
bool tl_audio_start(void);
void tl_audio_play(unsigned volume);
void tl_audio_stop(void);
bool tl_audio_error(void);
