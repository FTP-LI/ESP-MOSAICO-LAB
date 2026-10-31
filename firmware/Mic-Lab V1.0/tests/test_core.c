#include "mic_core.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    int16_t clip[8],out[8],in[8]={16384,-16384,0,0,32767,-32768,1,-1};
    mic_core_t m;mic_core_init(&m,clip,8);
    assert(m.mode==MIC_LIVE && m.peak_db==-90 && m.rms_db==-90);
    assert(!mic_core_play(&m));
    mic_core_process(&m,in,out,2,false,0);
    assert(fabsf(m.peak_db+6.0206f)<0.01f && fabsf(m.rms_db+6.0206f)<0.01f);
    assert(out[0]==16384 && out[1]==-16384 && !m.clipped);
    mic_core_process(&m,in+4,out,2,false,0);
    assert(m.clipped && m.peak_db==0 && out[1]==-32768);
    assert(mic_core_record(&m));
    assert(!mic_core_record(&m) && !mic_core_play(&m));
    mic_core_process(&m,in,out,4,true,0);
    assert(m.samples==4 && out[0]==0 && clip[0]==0 && m.peak_db>-7);
    mic_core_process(&m,in,out,8,false,-6);
    assert(m.samples==8 && m.mode==MIC_SAVING);
    assert(clip[4]>8200 && clip[4]<8220);
    assert(!mic_core_record(&m) && !mic_core_play(&m) && !mic_core_stop(&m));
    m.mode=MIC_LIVE;
    assert(mic_core_play(&m));
    assert(mic_core_playback(&m,out,6)==6 && m.mode==MIC_PLAYING);
    assert(mic_core_playback(&m,out,8)==2 && m.mode==MIC_PLAYING && out[2]==0);
    int16_t drain[480];
    for(int i=0;i<4;++i){
        assert(m.mode==MIC_PLAYING && mic_core_playback(&m,drain,480)==0);
        for(int j=0;j<480;++j)assert(drain[j]==0);
    }
    assert(m.mode==MIC_LIVE && m.cursor==8);
    assert(mic_core_record(&m) && m.samples==0);
    mic_core_process(&m,in,out,2,false,-100);
    assert(mic_core_stop(&m) && m.mode==MIC_SAVING && m.samples==2);
    m.mode=MIC_LIVE;assert(mic_core_play(&m));assert(mic_core_stop(&m) && m.mode==MIC_LIVE);
    mic_core_init(&m,NULL,0);assert(!mic_core_record(&m));
    puts("core: meters, clipping, mute, attenuation, bounded recording and playback PASS");
}
