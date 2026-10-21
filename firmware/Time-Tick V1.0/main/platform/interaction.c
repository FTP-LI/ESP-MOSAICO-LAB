// SPDX-License-Identifier: GPL-3.0-only
#include "interaction.h"
#include "alarm_model.h"
#include "bsp/esp_mosaico.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "soc/gpio_reg.h"
#include "soc/soc.h"
#include <stdatomic.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct { int sound; unsigned volume, generation; bool alarm; } request_t;
static QueueHandle_t requests;
static atomic_bool audio_error;
static atomic_bool alarm_active;
static atomic_uint audio_generation;
static esp_codec_dev_handle_t speaker;
static esp_codec_dev_sample_info_t format={.sample_rate=48000,.bits_per_sample=16,.channel=2};

static bool current(request_t r) { return r.generation==atomic_load(&audio_generation); }
typedef struct { unsigned max_us,over_budget; } render_stats_t;
static bool tone(unsigned hz,unsigned duration,request_t r,render_stats_t *stats)
{
    int16_t pcm[960];
    unsigned total=48*duration;
    for (unsigned at=0;at<total;at+=480) {
        if (!current(r)) return true; /* Cancellation is not an audio failure. */
        int64_t started=esp_timer_get_time();
        for (unsigned i=0;i<480;++i) {
            unsigned pos=at+i;
            int16_t value=0;
            if (r.alarm) value=tt_alarm_sample((unsigned)r.sound,hz,pos,total);
            else if (pos<total) {
                float envelope=fminf(1.f,fminf(pos/480.f,(total-pos)/480.f));
                float phase=6.2831853f*hz*pos/48000.f;
                value=(int16_t)(6000.f*envelope*sinf(phase));
            }
            pcm[i*2]=pcm[i*2+1]=value;
        }
        unsigned elapsed=(unsigned)(esp_timer_get_time()-started);
        if (stats) {
            if (elapsed>stats->max_us) stats->max_us=elapsed;
            if (elapsed>=10000) ++stats->over_budget;
        }
        if (esp_codec_dev_write(speaker,pcm,sizeof(pcm))!=ESP_CODEC_DEV_OK) return false;
    }
    return true;
}
static bool silence(unsigned ms,request_t r)
{
    int16_t pcm[960]={0};
    for (unsigned at=0;at<ms && current(r);at+=10) {
        if (esp_codec_dev_write(speaker,pcm,sizeof(pcm))!=ESP_CODEC_DEV_OK) return false;
    }
    return true;
}
static void gap(unsigned ms,request_t r)
{
    for (unsigned at=0;at<ms && current(r);at+=10) vTaskDelay(pdMS_TO_TICKS(10));
}
static void play(request_t r)
{
    if (!current(r)) return;
    if (!speaker) speaker=bsp_audio_codec_speaker_init();
    if (!speaker || esp_codec_dev_open(speaker,&format)!=ESP_CODEC_DEV_OK) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_audio_idle());
        atomic_store(&audio_error,true); ESP_LOGE("tt_alarm","speaker open failed"); return;
    }
    unsigned codec_volume=r.alarm?tt_alarm_codec_volume(r.volume):r.volume;
    bool ok=esp_codec_dev_set_out_vol(speaker,(int)codec_volume)==ESP_CODEC_DEV_OK;
    if (r.alarm) {
        /* The codec wrapper does not propagate set_vol's I2C write result. */
        int gain=-1,mute=-1,slot=-1;
        int readback=esp_codec_dev_read_reg(speaker,0x32,&gain);
        readback|=esp_codec_dev_read_reg(speaker,0x31,&mute);
        readback|=esp_codec_dev_read_reg(speaker,0x09,&slot);
        const unsigned pin=BSP_AUDIO_PA_CTRL,bit=1U<<(pin%32);
        unsigned pa_enabled=(REG_READ(pin<32?GPIO_ENABLE_REG:GPIO_ENABLE1_REG)&bit)!=0;
        unsigned pa_high=(REG_READ(pin<32?GPIO_OUT_REG:GPIO_OUT1_REG)&bit)!=0;
        ESP_LOGI("tt_alarm","output check: read=%d DAC=%d mute=%d slot=%d PA=%u/%u",
                 readback,gain,mute,slot,pa_enabled,pa_high);
        static bool pins_reported;
        if (!pins_reported) {
            gpio_dump_io_configuration(stdout,(1ULL<<BSP_AUDIO_I2S_MCLK)|(1ULL<<BSP_AUDIO_I2S_SCLK)|
                (1ULL<<BSP_AUDIO_I2S_LRCLK)|(1ULL<<BSP_AUDIO_I2S_SDOUT)|(1ULL<<BSP_AUDIO_PA_CTRL));
            const uint8_t regs[]={0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x0d,0x0e,0x12,0x13,0x14,0x37,0x44,0x45};
            char values[160]; unsigned at=0,read_errors=0;
            for (unsigned i=0;i<sizeof(regs);++i) {
                int value=-1;
                if (esp_codec_dev_read_reg(speaker,regs[i],&value)!=ESP_CODEC_DEV_OK) ++read_errors;
                at+=(unsigned)snprintf(values+at,sizeof(values)-at," %02x=%02x",regs[i],(unsigned)value&0xffu);
            }
            ESP_LOGI("tt_alarm","codec registers (read_errors=%u):%s",read_errors,values);
            pins_reported=true;
        }
        unsigned count=0;
        render_stats_t stats={0};
        int64_t started=esp_timer_get_time();
        const tt_alarm_note_t *notes=tt_alarm_notes((unsigned)r.sound,&count);
        for (unsigned i=0;ok && current(r) && i<count;++i) {
            ok=tone(notes[i].hz,notes[i].duration_ms,r,&stats);
            if (ok) ok=silence(notes[i].gap_ms,r);
        }
        /* Six 5 ms DMA buffers: let the final faded note leave before closing PA. */
        if (ok) ok=silence(40,r);
        ESP_LOGI("tt_alarm","ringtone=%d volume=%u codec=%u render_max=%uus render_over_10ms=%u play_ms=%u audio=%s",
                 r.sound,r.volume,codec_volume,stats.max_us,stats.over_budget,(unsigned)((esp_timer_get_time()-started)/1000),
                 ok?(current(r)?"ok":"cancelled"):"failed");
    } else {
        const unsigned notes[]={660,880,1047};
        unsigned count=r.sound==TT_SOUND_FINISH || r.sound==TT_SOUND_REWARD?3:1;
        for (unsigned i=0;ok && current(r) && i<count;++i) {
            ok=tone(r.sound==TT_SOUND_PAUSE?440:notes[i],count==1?60:120,r,NULL);
            if (count>1) gap(40,r);
        }
    }
    (void)esp_codec_dev_close(speaker); atomic_store(&audio_error,!ok);
    ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_audio_idle());
}
static void worker(void *arg)
{
    (void)arg; request_t r;
    /* Current release only plays queued cues/alarms. No microphone or wake
     * polling: a request wakes the task immediately, even during an alarm. */
    for (;;) {
        if (xQueueReceive(requests,&r,portMAX_DELAY)==pdTRUE && current(r)) play(r);
    }
}
bool tt_interaction_start(void)
{
    requests=xQueueCreate(4,sizeof(request_t));
    if (!requests) { atomic_store(&audio_error,true); return false; }
    if (xTaskCreate(worker,"tt_interaction",12288,NULL,3,NULL)!=pdPASS) {
        vQueueDelete(requests); requests=NULL; atomic_store(&audio_error,true); return false;
    }
    return true;
}
void tt_interaction_sound(tt_sound_t sound,unsigned volume)
{
    if (!requests || !volume || atomic_load(&alarm_active)) return;
    request_t r={.sound=sound,.volume=volume>80?80:volume,.generation=atomic_load(&audio_generation)};
    (void)xQueueSend(requests,&r,0);
}
void tt_interaction_alarm_active(bool active)
{
    if (atomic_exchange(&alarm_active,active)==active) return;
    atomic_fetch_add(&audio_generation,1);
    if (requests) xQueueReset(requests);
    if (active) atomic_store(&audio_error,requests==NULL);
}
void tt_interaction_alarm_pulse(unsigned ringtone,unsigned volume)
{
    if (!requests || !volume || ringtone>=TT_RING_COUNT || !atomic_load(&alarm_active)) return;
    request_t r={.sound=(int)ringtone,.volume=volume>80?80:volume,.alarm=true,.generation=atomic_load(&audio_generation)};
    if (xQueueSend(requests,&r,0)!=pdTRUE) atomic_store(&audio_error,true);
}
void tt_interaction_snapshot(tt_interaction_view_t *v)
{
    *v=(tt_interaction_view_t){.audio_error=atomic_load(&audio_error),.alarm_active=atomic_load(&alarm_active)};
}

