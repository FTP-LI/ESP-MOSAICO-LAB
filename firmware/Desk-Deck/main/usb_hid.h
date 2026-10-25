// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
typedef struct {
    uint8_t modifier, key;
    uint16_t consumer;
} deck_hid_action_t;
esp_err_t deck_hid_start(void);
bool deck_hid_ready(void);
bool deck_hid_send(deck_hid_action_t action);
void deck_hid_cancel_pending(void);
unsigned deck_hid_completed(void);
unsigned deck_hid_dropped(void);
