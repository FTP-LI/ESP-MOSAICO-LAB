// SPDX-License-Identifier: GPL-3.0-only
#ifdef NDEBUG
#undef NDEBUG /* Assertions are the test checks, including Release builds. */
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "timetick_core.h"

static tt_config_t config(void)
{
    tt_config_t c = tt_config_default();
    c.calibrated = true;
    return c;
}
static void feed(tt_orientation_t *o, float x, float y, float z, int64_t from, int64_t to)
{
    float a[] = {x,y,z}, g[] = {0,0,0};
    for (int64_t n=from; n<=to; n+=20) tt_orientation_sample(o,a,g,true,n);
}
static void orientation_tests(void)
{
    tt_orientation_t o = {0};
    feed(&o,0,1,0,0,440);
    assert(!o.settled);
    feed(&o,0,1,0,460,600);
    assert(o.settled && o.stable==2);
    feed(&o,0.707f,0.707f,0,620,1200);
    assert(!o.settled && o.stable==2);
    feed(&o,1,0,0,1220,2200);
    assert(o.settled && o.stable==1);
    float a[] = {0,1,0}, fast[] = {0,0,100};
    tt_orientation_sample(&o,a,fast,true,2220);
    assert(!o.settled && o.stable==1);
    feed(&o,0,0,0,2240,2800);
    assert(!o.settled);
    feed(&o,0,1,0,2820,3400);
    assert(o.settled && o.stable==2);
    feed(&o,-1,0,0,5000,5000); /* Read gap must not count toward the dwell. */
    assert(!o.settled);
    feed(&o,-1,0,0,5020,5500);
    assert(o.settled && o.stable==-1);
    float bad[] = {NAN,0,0}, still[] = {0,0,0};
    tt_orientation_sample(&o,bad,still,true,5520);
    assert(!o.settled);
}
static void mapping_tests(void)
{
    tt_config_t c = config();
    assert(tt_face_from_axis(&c,2)==TT_HOME);
    assert(tt_face_from_axis(&c,1)==TT_RIGHT);
    assert(tt_face_from_axis(&c,-2)==TT_INVERTED);
    assert(tt_face_from_axis(&c,-1)==TT_LEFT);
    assert(tt_face_from_axis(&c,3)==TT_FRONT);
    assert(tt_face_from_axis(&c,-3)==TT_UNKNOWN);
    c.calibrated=false;
    assert(tt_face_from_axis(&c,1)==TT_UNKNOWN);
    c.right_axis=-2;
    assert(!tt_config_valid(&c));
    assert(tt_rotation(TT_RIGHT,false,0)==270);
    assert(tt_rotation(TT_RIGHT,true,0)==90);
    assert(tt_rotation(TT_FRONT,false,90)==90);
    for (int previous=0;previous<360;previous+=90) {
        assert(tt_rotation(TT_FRONT,false,previous)==previous);
        assert(tt_rotation(TT_UNKNOWN,false,previous)==previous);
    }
    for(int home=-3;home<=3;++home) for(int right=-3;right<=3;++right) {
        if(!home||!right||home==right||home==-right)continue;
        c=config(); c.home_axis=home;c.right_axis=right;
        unsigned seen=0;
        for(int axis=-3;axis<=3;++axis) if(axis) seen|=1u<<tt_face_from_axis(&c,axis);
        assert(seen==95); /* Four upright positions, one flat, one ignored. */
    }
}
static void timer_tests(void)
{
    tt_config_t c=config();
    tt_timer_t t;
    tt_timer_init(&t);
    tt_timer_face(&t,&c,(tt_face_t)-1,0);
    tt_timer_face(&t,&c,(tt_face_t)20,0);
    assert(t.state==TT_IDLE);
    tt_timer_face(&t,&c,TT_RIGHT,1000);
    assert(t.state==TT_RUNNING && t.remaining_ms==300000);
    tt_timer_tick(&t,101000);
    tt_timer_face(&t,&c,TT_RIGHT,101000);
    assert(t.remaining_ms==200000); /* Same face does not restart on every sensor sample. */
    tt_timer_face(&t,&c,TT_HOME,101000);
    assert(t.state==TT_PAUSED);
    tt_timer_tick(&t,999000);
    assert(t.remaining_ms==200000);
    tt_timer_face(&t,&c,TT_RIGHT,999000);
    assert(t.remaining_ms==300000); /* Restart policy. */
    c.policy=TT_HOME_RESUME;
    tt_timer_face(&t,&c,TT_HOME,1000000);
    assert(t.remaining_ms==299000);
    tt_timer_face(&t,&c,TT_RIGHT,2000000);
    assert(t.remaining_ms==299000 && t.deadline_ms==2299000);
    tt_timer_face(&t,&c,TT_LEFT,2001000);
    assert(t.remaining_ms==1500000); /* Different preset always restarts. */
    c.policy=TT_HOME_CONTINUE;
    tt_timer_face(&t,&c,TT_HOME,2002000);
    assert(t.state==TT_RUNNING);
    assert(tt_timer_tick(&t,3501000));
    assert(t.state==TT_FINISHED && t.remaining_ms==0);
    assert(!tt_timer_tick(&t,3600000)); /* Only one expiry edge. */
    tt_timer_face(&t,&c,TT_FRONT,3601000);
    assert(t.state==TT_FINISHED); /* Flat must not replace an expired timer. */
    tt_timer_face(&t,&c,TT_RIGHT,3601000);
    tt_timer_toggle(&t,3602000);
    assert(t.state==TT_PAUSED && t.remaining_ms==299000);
    tt_timer_face(&t,&c,TT_FRONT,3800000);
    assert(t.state==TT_PAUSED && t.remaining_ms==299000);
    tt_timer_toggle(&t,4000000);
    assert(t.state==TT_RUNNING && t.deadline_ms==4299000);
    tt_timer_stop(&t);
    tt_timer_face(&t,&c,TT_RIGHT,4000001);
    assert(t.state==TT_IDLE); /* Dismiss does not immediately re-arm on the same face. */
    assert(tt_timer_start(&t,&c,TT_RIGHT,4000001)); /* Explicit Start can re-arm that face. */
    assert(t.state==TT_RUNNING && t.remaining_ms==300000);
    int64_t deadline=t.deadline_ms;
    tt_timer_face(&t,&c,TT_FRONT,4000250);
    assert(t.active==TT_RIGHT && t.observed==TT_RIGHT && t.deadline_ms==deadline);
    assert(!tt_timer_start(&t,&c,TT_FRONT,4000250));
    tt_timer_tick(&t,4000500);
    assert(t.remaining_ms==299501);
    tt_timer_face(&t,&c,TT_BACK,4000500);
    assert(t.active==TT_RIGHT && t.deadline_ms==deadline);
    assert(!tt_timer_start(&t,&c,TT_BACK,4000500));
    tt_timer_stop(&t);
    assert(!tt_timer_start(&t,&c,TT_HOME,4000001));
    assert(!tt_timer_start(&t,&c,TT_UNKNOWN,4000001));
    assert(c.seconds[TT_FRONT]==0);
    tt_timer_face(&t,&c,TT_FRONT,4000002);
    assert(t.state==TT_IDLE);
    assert(!tt_timer_start(&t,&c,TT_FRONT,4000002));
    c.calibrated=false;
    tt_timer_face(&t,&c,TT_RIGHT,4000003);
    assert(t.state==TT_IDLE);
}
int main(void)
{
    orientation_tests();
    mapping_tests();
    timer_tests();
    puts("PASS: dwell, motion rejection, sensor gaps, flat/down never start or reset timers, retained flat rotation and timer policies");
    return 0;
}
