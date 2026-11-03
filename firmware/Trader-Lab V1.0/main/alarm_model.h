// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Append only: IDs 0..2 are already stored in schema 5 settings. */
enum { TT_RING_BELL, TT_RING_DIGITAL, TT_RING_MELODY, TT_RING_DOORBELL,
       TT_RING_WINDCHIME, TT_RING_MARIMBA, TT_RING_SUNRISE, TT_RING_RETRO,
       TT_RING_PULSE, TT_RING_COUNT };
typedef struct { uint8_t ringtone, volume, duration_s; bool enabled; } tt_alarm_config_t;
typedef struct {
    tt_alarm_config_t config;
    int64_t started_ms, until_ms, next_ring_ms;
    bool active, preview, vibration;
    int8_t origin_axis; /* Last reliable IMU pose at expiry, or 0 if unavailable. */
} tt_alarm_t;
typedef struct { bool active, ring, motor; } tt_alarm_output_t;
typedef struct { uint16_t hz, duration_ms, gap_ms; } tt_alarm_note_t;
tt_alarm_config_t tt_alarm_default(void);
bool tt_alarm_config_valid(const tt_alarm_config_t *config);
void tt_alarm_start(tt_alarm_t *alarm,const tt_alarm_config_t *config,bool vibration,bool preview,int64_t now);
void tt_alarm_stop(tt_alarm_t *alarm);
/* Call only with a settled IMU axis while the timer is FINISHED. If no origin
 * is known, the first stable pose anchors it; a change consumes this flip. */
bool tt_alarm_turn(tt_alarm_t *alarm,int stable_axis);
tt_alarm_output_t tt_alarm_poll(tt_alarm_t *alarm,int64_t now);
const tt_alarm_note_t *tt_alarm_notes(unsigned ringtone,unsigned *count);
/* Alarm-only gain; the codec uses -50..0 dB for its 0..100 range. */
unsigned tt_alarm_codec_volume(unsigned volume);
/* 48 kHz mono, with 10 ms attack/release and signed 16-bit headroom. */
int16_t tt_alarm_sample(unsigned ringtone,unsigned hz,unsigned frame,unsigned total_frames);
