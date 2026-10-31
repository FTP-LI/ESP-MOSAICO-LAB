// SPDX-License-Identifier: GPL-3.0-only
#include "mic_core.h"
#include <math.h>
#include <string.h>
static float dbfs(float amplitude) {
    return amplitude>1.0362f ? 20.0f*log10f(amplitude/32768.0f) : -90.0f;
}
void mic_core_init(mic_core_t *m,int16_t *clip,size_t capacity) {
    memset(m,0,sizeof(*m));m->clip=clip;
    m->capacity=capacity>MIC_MAX_SAMPLES?MIC_MAX_SAMPLES:capacity;
    m->peak_db=m->rms_db=-90;
}
bool mic_core_record(mic_core_t *m) {
    if(m->mode!=MIC_LIVE || !m->clip || !m->capacity)return false;
    m->samples=m->cursor=0;m->mode=MIC_RECORDING;return true;
}
bool mic_core_stop(mic_core_t *m) {
    if(m->mode==MIC_RECORDING){m->mode=m->samples?MIC_SAVING:MIC_LIVE;return true;}
    if(m->mode==MIC_PLAYING){m->mode=MIC_LIVE;return true;}
    return false;
}
bool mic_core_play(mic_core_t *m) {
    if(m->mode!=MIC_LIVE || !m->samples)return false;
    m->cursor=0;m->drain_samples=1920;m->mode=MIC_PLAYING;return true;
}
void mic_core_process(mic_core_t *m,const int16_t *in,int16_t *out,size_t n,bool mute,int attenuation_db) {
    int peak=0;double sum=0;
    if(attenuation_db>0)attenuation_db=0;
    if(attenuation_db<-60)attenuation_db=-60;
    float scale=powf(10.0f,attenuation_db/20.0f);
    for(size_t i=0;i<n;++i){
        int x=in[i],a=x<0?-x:x;if(a>peak)peak=a;sum+=(double)x*x;
        out[i]=mute?0:(int16_t)lrintf(x*scale);
    }
    m->peak_db=dbfs((float)peak);m->rms_db=dbfs(n?(float)sqrt(sum/n):0);m->clipped=peak>=32760;
    if(m->mode==MIC_RECORDING){
        size_t remaining=m->capacity-m->samples,take=n<remaining?n:remaining;
        memcpy(m->clip+m->samples,out,take*sizeof(*out));m->samples+=take;
        if(m->samples==m->capacity)m->mode=MIC_SAVING;
    }
}
size_t mic_core_playback(mic_core_t *m,int16_t *out,size_t n) {
    memset(out,0,n*sizeof(*out));
    if(m->mode!=MIC_PLAYING)return 0;
    // Feed 40 ms of zeros after the final PCM block before closing the codec.
    // Pinned BSP TX DMA holds 6 * 240 frames (30 ms at 48 kHz).
    if(m->cursor==m->samples){
        size_t drain=n<m->drain_samples?n:m->drain_samples;
        m->drain_samples-=drain;
        if(!m->drain_samples)m->mode=MIC_LIVE;
        return 0;
    }
    size_t remain=m->samples-m->cursor,take=n<remain?n:remain;
    memcpy(out,m->clip+m->cursor,take*sizeof(*out));m->cursor+=take;
    return take;
}
