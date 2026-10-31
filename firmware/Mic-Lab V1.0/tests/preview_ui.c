// SPDX-License-Identifier: GPL-3.0-only
// Host fixtures exercise the actual LVGL screen; audio/USB data below are simulated.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../main/mic_ui.c"
static mic_snapshot_t fixture;
static mic_command_t command;
static bool connected=true,active=true,host_muted;
static int host_db;
static uint8_t pixels[480*480*4],draw_buffer[480*480*4];
void mic_audio_snapshot(mic_snapshot_t *s){*s=fixture;}
bool mic_audio_command(mic_command_t c){command=c;return true;}
void mic_audio_gain(int db){fixture.gain_db=db;}
void mic_audio_mute(bool v){fixture.mute=v;}
bool usb_mic_connected(void){return connected;}
bool usb_mic_active(void){return active;}
bool usb_mic_host_mute(void){return host_muted;}
int usb_mic_host_db(void){return host_db;}
static void flush(lv_display_t *d,const lv_area_t *area,uint8_t *data){
    for(int y=area->y1;y<=area->y2;++y){
        size_t width=(size_t)(area->x2-area->x1+1)*4;
        memcpy(pixels+((size_t)y*480+area->x1)*4,data+(size_t)(y-area->y1)*width,width);
    }
    lv_display_flush_ready(d);
}
static void u32(uint8_t *p,uint32_t n){for(int i=0;i<4;++i)p[i]=(uint8_t)(n>>(i*8));}
static void screenshot(const char *path){
    mic_ui_update();lv_timer_handler();lv_tick_inc(300);lv_timer_handler();lv_refr_now(NULL);
    uint8_t h[54]={0};h[0]='B';h[1]='M';u32(h+2,54+480*480*4);u32(h+10,54);u32(h+14,40);u32(h+18,480);u32(h+22,480);h[26]=1;h[28]=32;
    FILE *f=fopen(path,"wb");assert(f);assert(fwrite(h,1,54,f)==54);
    for(int y=479;y>=0;--y)assert(fwrite(pixels+y*480*4,1,480*4,f)==480*4);
    assert(fclose(f)==0);
}
static void inside(lv_obj_t *child,lv_obj_t *parent){
    lv_area_t a,b;lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);
    assert(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2);
}
int main(void){
    lv_init();lv_display_t *display=lv_display_create(480,480);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display,flush);
    fixture=(mic_snapshot_t){.mode=MIC_LIVE,.peak_db=-18,.rms_db=-24,.gain_db=24,.storage_ready=true};
    mic_ui_create();screenshot("live.bmp");
    assert(lv_obj_has_state(play_button,LV_STATE_DISABLED));
    lv_obj_send_event(mute_button,LV_EVENT_CLICKED,NULL);assert(fixture.mute);
    lv_obj_send_event(plus,LV_EVENT_CLICKED,NULL);assert(fixture.gain_db==30);
    lv_obj_send_event(record_button,LV_EVENT_CLICKED,NULL);assert(command==MIC_CMD_RECORD);
    fixture.mode=MIC_RECORDING;fixture.samples=MIC_RATE*12;fixture.mute=false;
    screenshot("recording.bmp");assert(lv_obj_has_state(play_button,LV_STATE_DISABLED));
    lv_obj_send_event(record_button,LV_EVENT_CLICKED,NULL);assert(command==MIC_CMD_STOP);
    fixture.mode=MIC_SAVING;screenshot("saving.bmp");
    assert(lv_obj_has_state(record_button,LV_STATE_DISABLED)&&lv_obj_has_state(play_button,LV_STATE_DISABLED));
    fixture.mode=MIC_LIVE;fixture.saved=true;fixture.mute=true;fixture.samples=MIC_MAX_SAMPLES;
    screenshot("muted.bmp");inside(mute_text,mute_button);inside(record_text,record_button);inside(play_text,play_button);
    fixture.mute=false;screenshot("saved.bmp");
    lv_obj_send_event(play_button,LV_EVENT_CLICKED,NULL);assert(command==MIC_CMD_PLAY);
    fixture.mode=MIC_PLAYING;fixture.cursor=MIC_RATE*5;fixture.peak_db=fixture.rms_db=-90;
    screenshot("playback.bmp");assert(lv_obj_has_state(record_button,LV_STATE_DISABLED));
    lv_obj_send_event(play_button,LV_EVENT_CLICKED,NULL);assert(command==MIC_CMD_STOP);
    fixture.mode=MIC_LIVE;fixture.saved=false;fixture.save_error=true;
    screenshot("save-error.bmp");lv_obj_send_event(record_button,LV_EVENT_CLICKED,NULL);assert(command==MIC_CMD_SAVE);
    fixture.peak_db=0;fixture.rms_db=-1;fixture.clipped=true;host_db=-60;
    screenshot("overload.bmp");inside(rms,lv_screen_active());
    fixture.audio_error=true;screenshot("audio-error.bmp");assert(lv_obj_has_state(record_button,LV_STATE_DISABLED));
    puts("ui: PASS (actual LVGL rendering, control events, save/error state guards, label bounds)");
    return 0;
}
