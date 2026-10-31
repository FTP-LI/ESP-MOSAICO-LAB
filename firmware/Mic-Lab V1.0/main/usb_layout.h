// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
size_t mic_usb_extend(const uint8_t *source,size_t source_length,uint8_t *out,size_t capacity,bool high_speed);
