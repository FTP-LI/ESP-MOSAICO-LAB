// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "clip_file.h"
#include "esp_err.h"
#include <stdbool.h>
typedef struct { bool loading,mounted;clip_result_t result;size_t samples; } mic_storage_result_t;
esp_err_t mic_storage_start(int16_t *clip,size_t capacity);
bool mic_storage_save(size_t count);
bool mic_storage_poll(mic_storage_result_t *result);
