// SPDX-License-Identifier: GPL-3.0-only
#include "alarm_model.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
int main(void)
{
    tt_alarm_config_t c=tt_alarm_default(); tt_alarm_t a={0};
    assert(c.enabled && c.volume==65 && c.duration_s==30 && tt_alarm_config_valid(&c));
    tt_alarm_start(&a,&c,true,false,1000);
    tt_alarm_output_t o=tt_alarm_poll(&a,1000); assert(o.active && o.ring && o.motor);
    o=tt_alarm_poll(&a,1100); assert(o.active && !o.ring && o.motor);
    assert(!tt_alarm_poll(&a,1180).motor && tt_alarm_poll(&a,1340).motor);
    assert(!tt_alarm_poll(&a,1520).motor);
    assert(!tt_alarm_poll(&a,3999).ring && tt_alarm_poll(&a,4000).ring);
    assert(tt_alarm_poll(&a,29999).ring); /* Delayed poll: only one reminder. */
    assert(!tt_alarm_poll(&a,30000).ring);
    o=tt_alarm_poll(&a,31000); assert(!o.active && !o.ring && !o.motor);
    assert(!tt_alarm_poll(&a,100000).active); /* No resurrection after timeout. */
    for (unsigned d=15;d<=60;d*=2) {
        c.duration_s=(uint8_t)d; tt_alarm_start(&a,&c,false,false,0);
        assert(tt_alarm_poll(&a,d*1000-1).active && !tt_alarm_poll(&a,d*1000).active);
    }
    tt_alarm_start(&a,&c,true,false,0); tt_alarm_stop(&a);
    o=tt_alarm_poll(&a,20); assert(!o.active && !o.ring && !o.motor);
    c.enabled=false; tt_alarm_start(&a,&c,true,false,0);
    o=tt_alarm_poll(&a,0); assert(o.active && !o.ring && o.motor);
    c.enabled=true; c.volume=0; tt_alarm_start(&a,&c,false,false,0);
    o=tt_alarm_poll(&a,0); assert(o.active && !o.ring && !o.motor);
    c=tt_alarm_default(); c.ringtone=TT_RING_MELODY; c.volume=47;
    tt_alarm_start(&a,&c,false,true,500);
    c.volume=80; /* The preview owns a snapshot of the unsaved form. */
    assert(a.config.volume==47 && a.config.ringtone==TT_RING_MELODY && a.preview);
    assert(tt_alarm_poll(&a,500).ring && !tt_alarm_poll(&a,3500).active);
    for (unsigned i=0;i<TT_RING_COUNT;++i) {
        unsigned n=0,total=0; const tt_alarm_note_t *notes=tt_alarm_notes(i,&n);
        assert(notes && n>1);
        for (unsigned j=0;j<n;++j) {
            assert(notes[j].hz>=400 && notes[j].hz<=3000);
            total+=notes[j].duration_ms+notes[j].gap_ms;
            unsigned frames=48*notes[j].duration_ms; int peak=0; double energy=0,sum=0;
            for (unsigned f=0;f<frames;++f) {
                int sample=tt_alarm_sample(i,notes[j].hz,f,frames);
                int magnitude=abs(sample); if (magnitude>peak) peak=magnitude;
                energy+=(double)sample*sample; sum+=sample;
                assert(magnitude<30000); /* No clipping or signed wraparound. */
                if (i<=TT_RING_MELODY || i==TT_RING_RETRO) {
                    /* Independent analytic reference: the faster renderer must
                     * preserve pitch, harmonics, gain and phase at every sample,
                     * including quadrant/table wraps and each note's decay. */
                    double phase=6.2831853071795864769*notes[j].hz*f/48000.;
                    double wave=i==TT_RING_RETRO?
                        .82*sin(phase)+.18*sin(3*phase)+.10*sin(5*phase):
                        sin(phase)+.25*sin(2*phase);
                    double fade=fmin(1.,fmin(f/480.,(frames-1-f)/480.));
                    assert(fabs(sample-24000.*fade*wave)<4.);
                }
            }
            double rms=sqrt(energy/frames);
            assert(peak>16000 && rms>7000 && rms<18000 && fabs(sum/frames)<40);
            if (i<=TT_RING_MELODY) assert(peak>25000 && rms>15000); /* Existing sounds retain their gain. */
            assert(tt_alarm_sample(i,notes[j].hz,0,frames)==0);
            assert(tt_alarm_sample(i,notes[j].hz,frames-1,frames)==0);
            assert(tt_alarm_sample(i,notes[j].hz,frames,frames)==0);
        }
        assert(total>=1000 && total<3000);
    }
    unsigned n=99; assert(!tt_alarm_notes(TT_RING_COUNT,&n) && n==0);
    assert(tt_alarm_sample(TT_RING_BELL,880,0,0)==0 && tt_alarm_codec_volume(0)==0);
    for (unsigned v=1;v<=80;++v) {
        assert(tt_alarm_codec_volume(v)>tt_alarm_codec_volume(v-1));
        assert(tt_alarm_codec_volume(v)<=95);
    }
    assert(tt_alarm_codec_volume(1000)==95);
    c.ringtone=TT_RING_COUNT; assert(!tt_alarm_config_valid(&c));
    c=tt_alarm_default(); c.volume=81; assert(!tt_alarm_config_valid(&c));
    c=tt_alarm_default(); c.duration_s=0; assert(!tt_alarm_config_valid(&c));
    c=tt_alarm_default();
    for (int origin=-3;origin<=3;++origin) for (int turn=-3;turn<=3;++turn) {
        if (!origin || !turn || turn==origin) continue;
        tt_alarm_start(&a,&c,true,false,0);
        assert(!tt_alarm_turn(&a,0) && !tt_alarm_turn(&a,4));
        assert(!tt_alarm_turn(&a,origin) && a.active);
        assert(!tt_alarm_turn(&a,origin) && a.active); /* No repeated-sample dismissal. */
        assert(tt_alarm_turn(&a,turn) && !a.active);
        o=tt_alarm_poll(&a,100); assert(!o.active && !o.ring && !o.motor);
    }
    tt_alarm_start(&a,&c,true,false,0); assert(!tt_alarm_turn(&a,1));
    assert(!tt_alarm_poll(&a,30000).active);
    assert(tt_alarm_turn(&a,2)); /* Sound timeout still allows closing the finished screen. */
    tt_alarm_start(&a,&c,true,true,0);
    assert(!tt_alarm_turn(&a,1) && !tt_alarm_turn(&a,2) && a.active); /* Settings preview is independent. */
    puts("PASS: alarm timing, cancellation, mute, preview, gain, waveform headroom and note envelopes");
    return 0;
}
