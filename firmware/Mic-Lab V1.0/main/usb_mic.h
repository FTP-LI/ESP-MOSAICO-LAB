// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool usb_mic_connected(void);
bool usb_mic_active(void);
bool usb_mic_host_mute(void);
int usb_mic_host_db(void);
void usb_mic_push(const int16_t *pcm,size_t samples,bool silence);
unsigned usb_mic_dropped(void);
unsigned usb_mic_short_packets(void);
