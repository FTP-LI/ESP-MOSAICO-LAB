// SPDX-License-Identifier: GPL-3.0-only
#include "power_model.h"
bool tt_power_decode(tt_power_view_t *p,uint16_t flags,uint16_t percent,uint16_t voltage,int16_t current,uint32_t now)
{
    *p=(tt_power_view_t){.sampled_ms=now};
    /* BQ27220 BatteryStatus: BATTPRES bit 3, DSG bit 0, FC bit 9.
     * Zero current alone cannot establish that USB is connected. */
    if (flags==0xffff || percent>100 || voltage>5000) return false;
    if (!(flags & (1u<<3))) { p->valid=true; p->state=TT_POWER_ABSENT; return true; }
    if (voltage<2000) return false;
    p->valid=true; p->percent=(uint8_t)percent; p->voltage_mv=(int16_t)voltage; p->current_ma=current;
    p->state=(flags&1u)?TT_POWER_BATTERY:(flags&(1u<<9))?TT_POWER_FULL:current>2?TT_POWER_CHARGING:
        current< -2?TT_POWER_BATTERY:TT_POWER_IDLE;
    return true;
}
bool tt_power_on_battery(const tt_power_view_t *p) { return p->valid && p->state==TT_POWER_BATTERY; }
void tt_power_usage_sample(tt_power_usage_t *u,const tt_power_view_t *p)
{
    if (!tt_power_on_battery(p) || p->simulated || p->current_ma>=0) { u->active=false; return; }
    unsigned ma=(unsigned)-(int)p->current_ma;
    uint32_t dt=p->sampled_ms-u->last_ms;
    if (!u->active || dt>20000) *u=(tt_power_usage_t){.active=true};
    else if (dt) {
        uint64_t charge=(uint64_t)(ma+u->last_ma)*dt/2;
        uint32_t settled=u->elapsed_ms>=60000?dt:
            u->elapsed_ms+dt>60000?u->elapsed_ms+dt-60000:0;
        u->elapsed_ms+=dt; u->charge_ma_ms+=charge; ++u->intervals;
        u->settled_ms+=settled; u->settled_ma_ms+=charge*settled/dt;
    }
    u->last_ms=p->sampled_ms; u->last_ma=ma;
}
unsigned tt_power_visual_period(const tt_power_view_t *p,bool atmosphere,uint32_t idle_ms)
{
    /* Decoration only. Timer/IMU, input and audio deadlines are unchanged. */
    if (idle_ms>=30000) return atmosphere?1000:100;
    if (!tt_power_on_battery(p)) return atmosphere?100:50;
    if (p->percent<=15) return atmosphere?250:100;
    return atmosphere?160:80;
}
unsigned tt_power_brightness(const tt_power_view_t *p,unsigned requested,uint32_t idle_ms,bool alert)
{
    if (alert || !tt_power_on_battery(p) || idle_ms<60000) return requested;
    unsigned cap=p->percent<=15?20:40;
    return requested<cap?requested:cap;
}
