// SPDX-License-Identifier: GPL-3.0-only
#include "power_model.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    tt_power_view_t p;
    assert(tt_power_decode(&p,9,67,3850,-118,10) && p.state==TT_POWER_BATTERY && p.percent==67);
    assert(tt_power_brightness(&p,75,59999,false)==75 && tt_power_brightness(&p,75,60000,false)==40);
    assert(tt_power_brightness(&p,20,60000,false)==20 && tt_power_brightness(&p,75,60000,true)==75);
    assert(tt_power_decode(&p,9,12,3500,-90,20) && tt_power_brightness(&p,75,60000,false)==20);
    assert(tt_power_decode(&p,8,78,4020,26,30) && p.state==TT_POWER_CHARGING && !tt_power_on_battery(&p));
    assert(tt_power_brightness(&p,75,600000,false)==75);
    /* Idle decoration throttles on USB as well as battery, and restores
     * instantly when the caller resets activity for touch/flip/alerts. */
    for (unsigned source=0;source<3;++source) {
        p=(tt_power_view_t){.valid=source!=2,.state=source?TT_POWER_BATTERY:TT_POWER_FULL,.percent=50};
        unsigned active_pet=source==1?80:50,active_air=source==1?160:100;
        assert(tt_power_visual_period(&p,false,29999)==active_pet);
        assert(tt_power_visual_period(&p,true,29999)==active_air);
        assert(tt_power_visual_period(&p,false,30000)==100);
        assert(tt_power_visual_period(&p,true,30000)==1000);
        assert(tt_power_visual_period(&p,false,0)==active_pet);
        assert(tt_power_visual_period(&p,true,0)==active_air);
    }
    p=(tt_power_view_t){.valid=true,.state=TT_POWER_BATTERY,.percent=15};
    assert(tt_power_visual_period(&p,false,0)==100 && tt_power_visual_period(&p,true,0)==250);
    assert(tt_power_visual_period(&p,true,30000)==1000);
    assert(tt_power_decode(&p,8|512,100,4200,0,40) && p.state==TT_POWER_FULL);
    assert(tt_power_decode(&p,8,50,3800,0,50) && p.state==TT_POWER_IDLE);
    assert(tt_power_decode(&p,0,0,0,0,60) && p.state==TT_POWER_ABSENT);
    assert(!tt_power_decode(&p,0xffff,100,4200,0,60) && !p.valid);
    assert(!tt_power_decode(&p,8,101,4200,0,60) && !p.valid);
    assert(!tt_power_decode(&p,8,0,0,0,60) && !p.valid);
    assert(tt_power_brightness(&p,75,600000,false)==75);
    tt_power_usage_t u={0};
    /* An hour at 60mA consumes 60mAh; reconnecting USB preserves the run.
     * First-minute warmup is excluded from the settled average. */
    for (unsigned i=0;i<=360;++i) {
        assert(tt_power_decode(&p,9,90,4000,-60,100+i*10000));
        tt_power_usage_sample(&u,&p);
    }
    assert(u.active && u.elapsed_ms==3600000 && u.intervals==360);
    assert(u.charge_ma_ms/3600==60000 && u.settled_ms==3540000);
    assert(u.settled_ma_ms/u.settled_ms==60);
    assert(tt_power_decode(&p,8,90,4150,50,3610100)); tt_power_usage_sample(&u,&p);
    assert(!u.active && u.elapsed_ms==3600000 && u.charge_ma_ms/3600==60000);
    assert(tt_power_decode(&p,9,90,4000,-105,3620100)); tt_power_usage_sample(&u,&p);
    assert(u.active && u.elapsed_ms==0); /* New discharge segment. */
    p.sampled_ms+=10000; p.current_ma=-55; tt_power_usage_sample(&u,&p);
    assert(u.charge_ma_ms==800000); /* Trapezoidal integration. */
    p.valid=false; tt_power_usage_sample(&u,&p); assert(!u.active);
    p.valid=true; p.sampled_ms+=10000; tt_power_usage_sample(&u,&p); assert(!u.elapsed_ms);
    p.sampled_ms+=30000; tt_power_usage_sample(&u,&p); assert(!u.elapsed_ms);
    p.simulated=true; tt_power_usage_sample(&u,&p); assert(!u.active);
    p.simulated=false; p.sampled_ms=UINT32_MAX-5000; tt_power_usage_sample(&u,&p);
    p.sampled_ms+=10000; tt_power_usage_sample(&u,&p); assert(u.elapsed_ms==10000);
    puts("PASS: checked gauge states, no invented USB status, low battery and idle dim/restore boundaries");
}
