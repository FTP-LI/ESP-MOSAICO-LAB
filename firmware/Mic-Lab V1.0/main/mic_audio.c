// SPDX-License-Identifier: GPL-3.0-only
#include "mic_audio.h"
#include "mic_storage.h"
#include "usb_mic.h"
#include <stdatomic.h>
#include <string.h>
#include "bsp/esp_mosaico.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
static mic_core_t model;
static mic_snapshot_t snapshot;
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static QueueHandle_t commands;
static atomic_int desired_gain;
static atomic_bool desired_mute;
static esp_codec_dev_handle_t input,output;
static bool audio_ready;
static esp_codec_dev_sample_info_t format={.sample_rate=MIC_RATE,.bits_per_sample=16,.channel=1,.channel_mask=1};
void mic_audio_gain(int db){if(db<0)db=0;if(db>42)db=42;atomic_store(&desired_gain,(db/6)*6);}
void mic_audio_mute(bool muted){atomic_store(&desired_mute,muted);}
bool mic_audio_command(mic_command_t cmd){return commands&&xQueueSend(commands,&cmd,0)==pdTRUE;}
void mic_audio_snapshot(mic_snapshot_t *out){portENTER_CRITICAL(&lock);*out=snapshot;portEXIT_CRITICAL(&lock);}
static void publish(bool loading,bool saved,bool storage_ready,bool save_error,bool audio_error){
    mic_snapshot_t s={.mode=model.mode,.peak_db=model.peak_db,.rms_db=model.rms_db,.samples=model.samples,
        .cursor=model.cursor,.clipped=model.clipped,.loading=loading,.saved=saved,.storage_ready=storage_ready,
        .save_error=save_error,.audio_error=audio_error,.mute=atomic_load(&desired_mute),.gain_db=atomic_load(&desired_gain)};
    s.audio_ready=audio_ready&&!audio_error;
    portENTER_CRITICAL(&lock);snapshot=s;portEXIT_CRITICAL(&lock);
}
static void audio_worker(void *unused){
    (void)unused;
    int16_t raw[480],pcm[480];
    bool loading=true,saved=false,store_ready=false,save_error=false,submitted=false;
    bool playing=false,record_open=false,audio_error=false;
    int applied=-1;int64_t resume_at=0,clip_until=0;
    input=bsp_audio_codec_microphone_init();output=bsp_audio_codec_speaker_init();
    if(!input||!output)audio_error=true;
    while(true){
        int64_t start=esp_timer_get_time();
        mic_storage_result_t result;
        if(mic_storage_poll(&result)){
            store_ready=result.mounted;
            if(result.loading){loading=false;model.samples=result.result==CLIP_OK?result.samples:0;saved=result.result==CLIP_OK;
                save_error=result.result!=CLIP_OK&&result.result!=CLIP_MISSING;}
            else {submitted=false;model.mode=MIC_LIVE;saved=result.result==CLIP_OK;save_error=!saved;}
        }
        mic_command_t cmd;
        while(xQueueReceive(commands,&cmd,0)==pdTRUE){
            if(loading||audio_error)continue;
            switch(cmd){
            case MIC_CMD_RECORD:if(store_ready && mic_core_record(&model)){saved=false;save_error=false;}break;
            case MIC_CMD_STOP:(void)mic_core_stop(&model);break;
            case MIC_CMD_PLAY:(void)mic_core_play(&model);break;
            case MIC_CMD_SAVE:if(model.mode==MIC_LIVE&&model.samples&&!saved)model.mode=MIC_SAVING;break;
            }
        }
        bool want_play=model.mode==MIC_PLAYING;
        if(want_play!=playing && !audio_error){
            if(want_play){
                if(record_open){if(esp_codec_dev_close(input)!=ESP_CODEC_DEV_OK)audio_error=true;record_open=false;}
                if(!audio_error && (esp_codec_dev_open(output,&format)!=ESP_CODEC_DEV_OK ||
                    esp_codec_dev_set_out_vol(output,35)!=ESP_CODEC_DEV_OK || esp_codec_dev_set_out_mute(output,false)!=ESP_CODEC_DEV_OK))audio_error=true;
            }else{
                if(esp_codec_dev_set_out_mute(output,true)!=ESP_CODEC_DEV_OK || esp_codec_dev_close(output)!=ESP_CODEC_DEV_OK)audio_error=true;
                resume_at=start+200000;applied=-1;
            }
            playing=want_play;
        }
        memset(pcm,0,sizeof(pcm));bool silence=audio_error||want_play||start<resume_at||atomic_load(&desired_mute)||usb_mic_host_mute();
        if(!audio_error&&want_play){
            mic_core_playback(&model,pcm,480);
            if(esp_codec_dev_write(output,pcm,sizeof(pcm))!=ESP_CODEC_DEV_OK)audio_error=true;
            model.peak_db=model.rms_db=-90;model.clipped=false;
        }else if(!audio_error){
            if(!record_open){record_open=esp_codec_dev_open(input,&format)==ESP_CODEC_DEV_OK;if(!record_open)audio_error=true;applied=-1;}
            int gain=atomic_load(&desired_gain);
            if(!audio_error&&applied!=gain){if(esp_codec_dev_set_in_gain(input,(float)gain)!=ESP_CODEC_DEV_OK)audio_error=true;else applied=gain;}
            if(!audio_error && esp_codec_dev_read(input,raw,sizeof(raw))==ESP_CODEC_DEV_OK){
                audio_ready=true;
                if(start<resume_at)memset(raw,0,sizeof(raw));
                mic_core_process(&model,raw,pcm,480,silence,usb_mic_host_db());
                if(model.clipped)clip_until=start+1000000;
                model.clipped=start<clip_until;
            }else{audio_error=true;memset(pcm,0,sizeof(pcm));}
        }
        if(audio_error){
            silence=true;model.peak_db=model.rms_db=-90;
            if(model.mode==MIC_RECORDING)(void)mic_core_stop(&model);
            if(model.mode==MIC_PLAYING)model.mode=MIC_LIVE;
            if(output)(void)esp_codec_dev_set_out_mute(output,true);
        }
        usb_mic_push(pcm,480,silence||audio_error);
        if(model.mode==MIC_SAVING&&!submitted){
            submitted=mic_storage_save(model.samples);
            if(!submitted){model.mode=MIC_LIVE;save_error=true;}
        }
        publish(loading,saved,store_ready,save_error,audio_error);
        int64_t elapsed=esp_timer_get_time()-start;
        if(elapsed<10000)vTaskDelay(pdMS_TO_TICKS((10000-elapsed+999)/1000));
        else taskYIELD();
    }
}
esp_err_t mic_audio_start(int gain_db,bool mute){
    int16_t *clip=heap_caps_malloc(MIC_MAX_SAMPLES*sizeof(int16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!clip)return ESP_ERR_NO_MEM;
    mic_core_init(&model,clip,MIC_MAX_SAMPLES);mic_audio_gain(gain_db);mic_audio_mute(mute);
    commands=xQueueCreate(8,sizeof(mic_command_t));if(!commands)return ESP_ERR_NO_MEM;
    esp_err_t err=mic_storage_start(clip,MIC_MAX_SAMPLES);if(err!=ESP_OK)return err;
    publish(true,false,false,false,false);
    if(xTaskCreatePinnedToCore(audio_worker,"mic_audio",6144,NULL,6,NULL,0)!=pdPASS)return ESP_ERR_NO_MEM;
    return ESP_OK;
}
