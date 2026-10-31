// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "mic_core.h"
#include "esp_err.h"
typedef enum { MIC_CMD_RECORD, MIC_CMD_STOP, MIC_CMD_PLAY, MIC_CMD_SAVE } mic_command_t;
typedef struct {
    mic_mode_t mode;
    float peak_db,rms_db;
    size_t samples,cursor;
    bool loading,saved,storage_ready,save_error,audio_error,audio_ready,clipped,mute;
    int gain_db;
} mic_snapshot_t;
esp_err_t mic_audio_start(int gain_db,bool mute);
bool mic_audio_command(mic_command_t cmd);
void mic_audio_gain(int db);
void mic_audio_mute(bool muted);
void mic_audio_snapshot(mic_snapshot_t *out);
