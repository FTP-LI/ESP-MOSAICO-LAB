// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "settings.h"
#include "weather_model.h"
#include "location_model.h"
#include "power_model.h"
#include <time.h>
#define TT_WIFI_MAX_APS 24
typedef enum { TT_SCAN_IDLE, TT_SCAN_RUNNING, TT_SCAN_DONE, TT_SCAN_FAILED } tt_scan_state_t;
typedef enum { TT_NET_IDLE, TT_NET_CONNECTING, TT_NET_CONNECTED, TT_NET_FAILED } tt_net_state_t;
typedef struct { char ssid[33]; int8_t rssi; bool secured; } tt_wifi_ap_t;
typedef struct {
    tt_power_view_t power;
    bool connected, weather_valid, simulated;
    float temperature;
    int humidity, weather_code;
    time_t updated,scene_time; /* PC override only; zero uses the device clock. */
    char location[80], message[96];
    char ssid[33];
    tt_scan_state_t scan_state;
    tt_net_state_t network_state;
    uint32_t scan_generation;
    uint8_t ap_count;
    tt_wifi_ap_t aps[TT_WIFI_MAX_APS];
    char network_message[80];
    tt_location_view_t lookup;
} tt_services_view_t;
#ifdef ESP_PLATFORM
esp_err_t tt_services_start(const tt_settings_t *settings);
void tt_services_configure(const tt_settings_t *settings);
void tt_services_scan(void);
void tt_services_snapshot(tt_services_view_t *view);
void tt_services_locate(const tt_location_request_t *request);
#endif
