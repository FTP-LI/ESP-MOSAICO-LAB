// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum { TT_SOUND_START, TT_SOUND_PAUSE, TT_SOUND_FINISH, TT_SOUND_REWARD, TT_SOUND_TAP } tt_sound_t;
typedef struct { bool audio_error, alarm_active; } tt_interaction_view_t;
#ifdef ESP_PLATFORM
bool tt_interaction_start(void);
void tt_interaction_sound(tt_sound_t sound,unsigned volume);
/* The main task owns the deadline; audio stays on the codec worker. */
void tt_interaction_alarm_active(bool active);
void tt_interaction_alarm_pulse(unsigned ringtone,unsigned volume);
void tt_interaction_snapshot(tt_interaction_view_t *view);
#endif
