// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef enum { CLIP_OK, CLIP_MISSING, CLIP_INVALID, CLIP_IO_ERROR } clip_result_t;
clip_result_t clip_file_save(const char *directory,const int16_t *samples,size_t count);
clip_result_t clip_file_load(const char *directory,int16_t *samples,size_t capacity,size_t *count);
