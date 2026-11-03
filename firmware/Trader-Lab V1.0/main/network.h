// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "model.h"
#include "global_market.h"
#define TL_MAX_AP 8
typedef struct { char ssid[33]; int8_t rssi; bool secured; } tl_ap_t;
typedef struct {
    tl_quote_t quotes[TL_MAX_WATCH];
    bool fetched_ok[TL_MAX_WATCH];
    tl_index_quote_t indices[TL_GLOBAL_COUNT];
    bool index_ok[TL_GLOBAL_COUNT];
    unsigned global_requests,global_failures;
    tl_ap_t aps[TL_MAX_AP];
    uint8_t ap_count;
    bool connected, clock_ok, scanning, fetching;
    unsigned generation, requests, failures;
    int disconnect_reason, http_status;
    char ip[16], message[96];
} tl_net_view_t;
bool tl_network_start(const tl_config_t *config);
unsigned tl_network_boot_stage(void);
void tl_network_configure(const tl_config_t *config);
void tl_network_scan(void);
void tl_network_refresh(void);
void tl_network_snapshot(tl_net_view_t *view);
