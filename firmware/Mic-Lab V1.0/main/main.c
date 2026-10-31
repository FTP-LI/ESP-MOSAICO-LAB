// SPDX-License-Identifier: GPL-3.0-only
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bsp/esp_mosaico.h"
#include "esp_heap_caps.h"
#include "esp_iris.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "iris_ota_support.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "mic_audio.h"
#include "mic_ui.h"
#include "usb_mic.h"
static uint8_t *capture;
static esp_err_t screen_begin(const esp_iris_media_desc_t *request,esp_iris_media_desc_t *actual,uint32_t *size,void *context){
    (void)request;(void)context;if(capture)return ESP_ERR_INVALID_STATE;
    uint8_t *bytes=heap_caps_malloc(480*480*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!bytes)return ESP_ERR_NO_MEM;
    if(!bsp_display_lock(500)){free(bytes);return ESP_ERR_TIMEOUT;}
    lv_display_t *display=bsp_display_get();lv_draw_buf_t *buf=lv_display_get_buf_active(display);
    bool full=buf&&buf->header.cf==LV_COLOR_FORMAT_RGB565&&buf->header.w==480&&buf->header.h==480&&buf->header.stride==960&&lv_display_get_render_mode(display)==LV_DISPLAY_RENDER_MODE_FULL;
    if(full)for(size_t i=0;i<480*480*2;i+=2){bytes[i]=buf->data[i+1];bytes[i+1]=buf->data[i];}
    bsp_display_unlock();if(!full){free(bytes);return ESP_ERR_NOT_SUPPORTED;}
    capture=bytes;*size=480*480*2;*actual=(esp_iris_media_desc_t){.width=480,.height=480,.stride=960,.format=ESP_IRIS_PIXEL_FORMAT_RGB565};return ESP_OK;
}
static esp_err_t screen_read(uint32_t offset,uint8_t *out,size_t cap,size_t *used,void *ctx){
    (void)ctx;if(!capture||offset>=480*480*2)return ESP_ERR_INVALID_ARG;
    *used=480*480*2-offset;if(*used>cap)*used=cap;memcpy(out,capture+offset,*used);return ESP_OK;
}
static void screen_end(void *ctx){(void)ctx;free(capture);capture=NULL;}
static esp_err_t status_rpc(const esp_iris_rpc_request_t *r,uint8_t *out,size_t cap,size_t *used,void *ctx){
    (void)ctx;if(r->payload_size!=6||memcmp(r->payload,"status",6))return ESP_ERR_INVALID_ARG;
    mic_snapshot_t s;mic_audio_snapshot(&s);
    int n=snprintf((char *)out,cap,"{\"mode\":%d,\"peak_dbfs\":%.1f,\"gain_db\":%d,\"mute\":%s,\"samples\":%u,\"saved\":%s,\"storage_ready\":%s,\"audio_error\":%s,\"usb_active\":%s,\"usb_dropped_samples\":%u,\"usb_short_packets\":%u,\"free_internal\":%u}",
        s.mode,s.peak_db,s.gain_db,s.mute?"true":"false",(unsigned)s.samples,s.saved?"true":"false",s.storage_ready?"true":"false",s.audio_error?"true":"false",usb_mic_active()?"true":"false",usb_mic_dropped(),usb_mic_short_packets(),(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    if(n<0||(size_t)n>=cap)return ESP_ERR_INVALID_SIZE;
    *used=(size_t)n;return ESP_OK;
}
void app_main(void){
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_boot_probe());ESP_ERROR_CHECK(nvs_flash_init());
    int gain=24;bool mute=false;nvs_handle_t handle;int32_t stored;uint8_t flag;
    if(nvs_open("miclab",NVS_READONLY,&handle)==ESP_OK){
        if(nvs_get_i32(handle,"gain",&stored)==ESP_OK&&stored>=0&&stored<=42&&stored%6==0)gain=stored;
        if(nvs_get_u8(handle,"mute",&flag)==ESP_OK&&flag<=1)mute=flag;
        nvs_close(handle);}
    iris_ota_support_start();
    ESP_ERROR_CHECK(bsp_display_start()?ESP_OK:ESP_FAIL);ESP_ERROR_CHECK(bsp_display_brightness_set(60));
    ESP_ERROR_CHECK(mic_audio_start(gain,mute));
    ESP_ERROR_CHECK(bsp_display_lock(1000)?ESP_OK:ESP_ERR_TIMEOUT);
    mic_ui_create();bsp_display_unlock();
    const esp_iris_screen_backend_t backend={.begin=screen_begin,.read=screen_read,.end=screen_end};
    ESP_ERROR_CHECK(esp_iris_screen_register(&backend));ESP_ERROR_CHECK(esp_iris_rpc_register(0x4d4c,1,status_rpc,NULL));
    bool healthy=false;
    int64_t changed=0;int last_gain=gain;bool last_mute=mute;
    while(true){
        if(bsp_display_lock(30)){mic_ui_update();bsp_display_unlock();}
        mic_snapshot_t s;mic_audio_snapshot(&s);
        if(!healthy&&s.audio_ready){
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_mark_healthy());healthy=true;
        }
        if(s.gain_db!=last_gain||s.mute!=last_mute){last_gain=s.gain_db;last_mute=s.mute;changed=esp_timer_get_time();}
        if(changed&&esp_timer_get_time()-changed>750000){
            esp_err_t err=nvs_open("miclab",NVS_READWRITE,&handle);
            if(err==ESP_OK){err=nvs_set_i32(handle,"gain",last_gain);if(err==ESP_OK)err=nvs_set_u8(handle,"mute",last_mute);if(err==ESP_OK)err=nvs_commit(handle);nvs_close(handle);}
            if(err!=ESP_OK)ESP_LOGW("mic_settings","save failed: %s",esp_err_to_name(err));
            changed=0;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
