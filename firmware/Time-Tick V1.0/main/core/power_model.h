// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum { TT_POWER_UNKNOWN, TT_POWER_ABSENT, TT_POWER_BATTERY, TT_POWER_CHARGING, TT_POWER_FULL, TT_POWER_IDLE } tt_power_state_t;
typedef struct {
    tt_power_state_t state;
    bool valid, simulated;
    uint8_t percent;
    int16_t voltage_mv, current_ma;
    uint32_t sampled_ms;
} tt_power_view_t;
/* Last contiguous battery run, retained in RAM after reconnecting USB.
 * Integrate only consecutive valid discharge samples; never charge/USB data. */
typedef struct {
    bool active;
    uint32_t last_ms,elapsed_ms,settled_ms,intervals;
    unsigned last_ma;
    uint64_t charge_ma_ms,settled_ma_ms;
} tt_power_usage_t;
void tt_power_usage_sample(tt_power_usage_t *usage,const tt_power_view_t *power);
bool tt_power_decode(tt_power_view_t *out,uint16_t flags,uint16_t percent,uint16_t voltage,int16_t current,uint32_t now);
bool tt_power_on_battery(const tt_power_view_t *power);
unsigned tt_power_brightness(const tt_power_view_t *power,unsigned requested,uint32_t idle_ms,bool alert);
unsigned tt_power_visual_period(const tt_power_view_t *power,bool atmosphere,uint32_t idle_ms);
