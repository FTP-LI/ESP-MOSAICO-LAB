// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "settings.h"
#include <stdbool.h>
#include <stddef.h>
#define TT_LOCATION_MAX 5
#define TT_LOCATION_BODY_MAX 8192
typedef enum { TT_LOCATION_IP, TT_LOCATION_SEARCH } tt_location_kind_t;
typedef enum { TT_LOCATION_IDLE, TT_LOCATION_LOADING, TT_LOCATION_READY,
               TT_LOCATION_EMPTY, TT_LOCATION_FAILED, TT_LOCATION_OFFLINE } tt_location_state_t;
typedef struct {
    char name[80], region[128];
    int32_t latitude_e6, longitude_e6;
} tt_location_t;
typedef struct {
    uint32_t id;
    tt_location_kind_t kind;
    uint8_t language;
    char query[80];
} tt_location_request_t;
typedef struct {
    uint32_t id;
    tt_location_kind_t kind;
    tt_location_state_t state;
    uint8_t count;
    bool simulated;
    tt_location_t places[TT_LOCATION_MAX];
} tt_location_view_t;
bool tt_location_url(const tt_location_request_t *request,char *url,size_t capacity);
/* Callers initialize id/kind. Invalid or oversized responses produce no candidates. */
void tt_location_parse(const char *body,tt_location_view_t *view);
