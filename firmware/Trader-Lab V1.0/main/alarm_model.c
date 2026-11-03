// SPDX-License-Identifier: GPL-3.0-only
#include "alarm_model.h"
#include <stddef.h>
#include <math.h>
tt_alarm_config_t tt_alarm_default(void)
{
    return (tt_alarm_config_t){.ringtone=TT_RING_BELL,.volume=65,.duration_s=30,.enabled=true};
}
bool tt_alarm_config_valid(const tt_alarm_config_t *c)
{
    return c->ringtone<TT_RING_COUNT && c->volume<=80 &&
        (c->duration_s==15 || c->duration_s==30 || c->duration_s==60);
}
void tt_alarm_start(tt_alarm_t *a,const tt_alarm_config_t *c,bool vibration,bool preview,int64_t now)
{
    *a=(tt_alarm_t){.config=*c,.vibration=vibration,.preview=preview,.active=tt_alarm_config_valid(c),
        .started_ms=now,.until_ms=now+(preview?3000:c->duration_s*1000),.next_ring_ms=now};
}
void tt_alarm_stop(tt_alarm_t *a) { a->active=false; }
bool tt_alarm_turn(tt_alarm_t *a,int axis)
{
    if (a->preview || !axis || axis < -3 || axis > 3) return false;
    if (!a->origin_axis) { a->origin_axis=(int8_t)axis; return false; }
    if (a->origin_axis==axis) return false;
    tt_alarm_stop(a);
    return true; /* Also dismiss the finished screen after sound times out. */
}
tt_alarm_output_t tt_alarm_poll(tt_alarm_t *a,int64_t now)
{
    tt_alarm_output_t out={0};
    if (!a->active) return out;
    if (now>=a->until_ms) { tt_alarm_stop(a); return out; }
    out.active=true;
    if (now>=a->next_ring_ms) {
        out.ring=a->config.enabled && a->config.volume>0;
        a->next_ring_ms=now+3000; /* Never queue a burst after a delayed poll. */
    }
    int64_t phase=(now-a->started_ms)%3000;
    out.motor=a->vibration && (phase<180 || (phase>=340 && phase<520));
    return out;
}
const tt_alarm_note_t *tt_alarm_notes(unsigned ringtone,unsigned *count)
{
    static const tt_alarm_note_t bell[]={{880,180,80},{1175,260,160},{880,180,80},{1175,400,0}};
    static const tt_alarm_note_t digital[]={{1200,140,80},{1200,140,80},{1200,140,240},{1500,140,80},{1500,140,80},{1500,140,0}};
    static const tt_alarm_note_t melody[]={{523,160,50},{659,160,50},{784,200,80},{1047,360,0}};
    static const tt_alarm_note_t doorbell[]={{784,280,40},{659,440,260},{784,280,40},{659,440,0}};
    static const tt_alarm_note_t windchime[]={{1047,240,90},{1319,260,70},{1175,240,110},{1568,280,80},{1319,360,0}};
    static const tt_alarm_note_t marimba[]={{523,130,60},{659,130,60},{784,160,100},{659,130,60},{784,130,60},{1047,300,0}};
    static const tt_alarm_note_t sunrise[]={{659,220,60},{784,220,60},{988,260,100},{1047,460,0}};
    static const tt_alarm_note_t retro[]={{659,80,10},{784,80,10},{988,120,30},{784,80,10},{988,120,30},{1319,200,80},{1047,380,0}};
    static const tt_alarm_note_t pulse[]={{988,100,70},{988,100,70},{1319,180,180},{988,100,70},{988,100,70},{1319,300,0}};
    switch (ringtone) {
    case TT_RING_BELL: *count=sizeof(bell)/sizeof(*bell); return bell;
    case TT_RING_DIGITAL: *count=sizeof(digital)/sizeof(*digital); return digital;
    case TT_RING_MELODY: *count=sizeof(melody)/sizeof(*melody); return melody;
    case TT_RING_DOORBELL: *count=sizeof(doorbell)/sizeof(*doorbell); return doorbell;
    case TT_RING_WINDCHIME: *count=sizeof(windchime)/sizeof(*windchime); return windchime;
    case TT_RING_MARIMBA: *count=sizeof(marimba)/sizeof(*marimba); return marimba;
    case TT_RING_SUNRISE: *count=sizeof(sunrise)/sizeof(*sunrise); return sunrise;
    case TT_RING_RETRO: *count=sizeof(retro)/sizeof(*retro); return retro;
    case TT_RING_PULSE: *count=sizeof(pulse)/sizeof(*pulse); return pulse;
    default: *count=0; return NULL;
    }
}
unsigned tt_alarm_codec_volume(unsigned volume)
{
    if (!volume) return 0;
    if (volume>80) volume=80;
    return volume+15; /* +7.5 dB; leave 2.5 dB below the codec's full scale. */
}
/* Q15 quarter-wave table: round(32767 * sin(pi * i / 512)), i=0..256.
 * Integer phase and linear interpolation avoid sinf's costly large-angle
 * reduction on the MCU. All harmonics share 514 bytes of read-only data. */
static float alarm_sine(unsigned phase)
{
    static const int16_t quarter[]={
        0,201,402,603,804,1005,1206,1407,1608,1809,2009,2210,
        2410,2611,2811,3012,3212,3412,3612,3811,4011,4210,4410,4609,
        4808,5007,5205,5404,5602,5800,5998,6195,6393,6590,6786,6983,
        7179,7375,7571,7767,7962,8157,8351,8545,8739,8933,9126,9319,
        9512,9704,9896,10087,10278,10469,10659,10849,11039,11228,11417,11605,
        11793,11980,12167,12353,12539,12725,12910,13094,13279,13462,13645,13828,
        14010,14191,14372,14553,14732,14912,15090,15269,15446,15623,15800,15976,
        16151,16325,16499,16673,16846,17018,17189,17360,17530,17700,17869,18037,
        18204,18371,18537,18703,18868,19032,19195,19357,19519,19680,19841,20000,
        20159,20317,20475,20631,20787,20942,21096,21250,21403,21554,21705,21856,
        22005,22154,22301,22448,22594,22739,22884,23027,23170,23311,23452,23592,
        23731,23870,24007,24143,24279,24413,24547,24680,24811,24942,25072,25201,
        25329,25456,25582,25708,25832,25955,26077,26198,26319,26438,26556,26674,
        26790,26905,27019,27133,27245,27356,27466,27575,27683,27790,27896,28001,
        28105,28208,28310,28411,28510,28609,28706,28803,28898,28992,29085,29177,
        29268,29358,29447,29534,29621,29706,29791,29874,29956,30037,30117,30195,
        30273,30349,30424,30498,30571,30643,30714,30783,30852,30919,30985,31050,
        31113,31176,31237,31297,31356,31414,31470,31526,31580,31633,31685,31736,
        31785,31833,31880,31926,31971,32014,32057,32098,32137,32176,32213,32250,
        32285,32318,32351,32382,32412,32441,32469,32495,32521,32545,32567,32589,
        32609,32628,32646,32663,32678,32692,32705,32717,32728,32737,32745,32752,
        32757,32761,32765,32766,32767
    };
    phase%=48000;
    int sign=phase>=24000?-1:1;
    if (phase>=24000) phase-=24000;
    if (phase>12000) phase=24000-phase;
    unsigned scaled=phase*256,index=scaled/12000,fraction=scaled%12000;
    int value=quarter[index];
    if (index<256) value+=(quarter[index+1]-value)*(int)fraction/12000;
    return sign*value*(1.f/32767.f);
}
int16_t tt_alarm_sample(unsigned ringtone,unsigned hz,unsigned frame,unsigned total_frames)
{
    if (frame>=total_frames) return 0;
    float envelope=fminf(1.f,fminf(frame/480.f,(total_frames-1-frame)/480.f));
    unsigned phase=(hz*frame)%48000;
    float remaining=1.f-(float)frame/total_frames;
    float wave;
    switch (ringtone) {
    case TT_RING_DOORBELL:
        wave=alarm_sine(phase)+.20f*alarm_sine(2*phase);
        envelope*=.55f+.45f*remaining; break;
    case TT_RING_WINDCHIME:
        wave=alarm_sine(phase)+.12f*alarm_sine(3*phase)+.06f*alarm_sine(5*phase);
        envelope*=.35f+.65f*remaining*remaining; break;
    case TT_RING_MARIMBA:
        wave=.90f*alarm_sine(phase)+.20f*alarm_sine(3*phase);
        envelope*=.35f+.65f*remaining; break;
    case TT_RING_SUNRISE:
        wave=alarm_sine(phase)+.10f*alarm_sine(2*phase); break;
    case TT_RING_RETRO:
        wave=.82f*alarm_sine(phase)+.18f*alarm_sine(3*phase)+.10f*alarm_sine(5*phase); break;
    default:
        wave=alarm_sine(phase)+.25f*alarm_sine(2*phase); break;
    }
    /* All partial sums <=1.25; signed 16-bit output retains headroom. */
    return (int16_t)(24000.f*envelope*wave);
}
