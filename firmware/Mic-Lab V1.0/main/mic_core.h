// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MIC_RATE 48000u
#define MIC_MAX_SAMPLES (MIC_RATE * 30u)
typedef enum { MIC_LIVE, MIC_RECORDING, MIC_SAVING, MIC_PLAYING } mic_mode_t;
typedef struct {
    int16_t *clip;
    size_t capacity, samples, cursor, drain_samples;
    mic_mode_t mode;
    float peak_db, rms_db;
    bool clipped;
} mic_core_t;
void mic_core_init(mic_core_t *m, int16_t *clip, size_t capacity);
bool mic_core_record(mic_core_t *m);
bool mic_core_stop(mic_core_t *m);
bool mic_core_play(mic_core_t *m);
void mic_core_process(mic_core_t *m, const int16_t *in, int16_t *out, size_t n, bool mute, int attenuation_db);
size_t mic_core_playback(mic_core_t *m, int16_t *out, size_t n);
