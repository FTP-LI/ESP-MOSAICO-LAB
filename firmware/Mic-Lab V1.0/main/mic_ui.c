// SPDX-License-Identifier: GPL-3.0-only
#include "mic_ui.h"
#include "mic_audio.h"
#include "usb_mic.h"
#include "mic_font.h"
#include <math.h>
#include <stdio.h>
static lv_obj_t *connection,*peak,*rms,*gain,*status,*progress,*mute_button,*mute_text,*record_button,*record_text,*play_button,*play_text,*minus,*plus;
static lv_obj_t *segments[30];
static lv_color_t ink(void){return lv_color_hex(0x172b40);}
static lv_obj_t *label(const char *text,int x,int y,const lv_font_t *font){
    lv_obj_t *o=lv_label_create(lv_screen_active());lv_label_set_text(o,text);lv_obj_set_pos(o,x,y);
    lv_obj_set_style_text_color(o,ink(),0);lv_obj_set_style_text_font(o,font,0);return o;
}
static void event(lv_event_t *e){
    int action=(int)(intptr_t)lv_event_get_user_data(e);mic_snapshot_t s;mic_audio_snapshot(&s);
    if(action==0)mic_audio_mute(!s.mute);
    else if(action==1)mic_audio_gain(s.gain_db-6);
    else if(action==2)mic_audio_gain(s.gain_db+6);
    else if(action==3)mic_audio_command(s.mode==MIC_RECORDING?MIC_CMD_STOP:(s.save_error&&s.samples&&!s.saved?MIC_CMD_SAVE:MIC_CMD_RECORD));
    else if(action==4)mic_audio_command(s.mode==MIC_PLAYING?MIC_CMD_STOP:MIC_CMD_PLAY);
}
static lv_obj_t *button(int x,int y,int w,const char *text,int action,lv_obj_t **caption){
    lv_obj_t *o=lv_button_create(lv_screen_active());lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,54);
    lv_obj_set_style_radius(o,12,0);lv_obj_set_style_bg_color(o,lv_color_hex(0x28649d),0);lv_obj_set_style_shadow_width(o,0,0);
    lv_obj_set_style_bg_opa(o,LV_OPA_40,LV_STATE_DISABLED);
    lv_obj_t *l=lv_label_create(o);lv_label_set_text(l,text);lv_obj_set_style_text_font(l,&lv_font_mic_20,0);lv_obj_center(l);
    lv_obj_add_event_cb(o,event,LV_EVENT_CLICKED,(void *)(intptr_t)action);if(caption)*caption=l;return o;
}
static void enable(lv_obj_t *o,bool yes){if(yes)lv_obj_remove_state(o,LV_STATE_DISABLED);else lv_obj_add_state(o,LV_STATE_DISABLED);}
void mic_ui_create(void){
    lv_obj_t *root=lv_screen_active();lv_obj_set_style_bg_color(root,lv_color_hex(0xe9eef3),0);lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLLABLE);
    label("Mic-Lab",26,22,&lv_font_montserrat_32);label("V1.0",367,31,&lv_font_mic_16);
    connection=label("等待连接电脑",26,65,&lv_font_mic_16);
    label("输入电平",26,106,&lv_font_mic_20);peak=label("-90 dBFS",246,104,&lv_font_montserrat_24);
    for(int i=0;i<30;++i){segments[i]=lv_obj_create(root);lv_obj_remove_style_all(segments[i]);
        lv_obj_set_pos(segments[i],26+i*14,150);lv_obj_set_size(segments[i],10,42);lv_obj_set_style_radius(segments[i],3,0);lv_obj_set_style_bg_opa(segments[i],LV_OPA_COVER,0);}
    label("-60",26,201,&lv_font_mic_16);label("-30",226,201,&lv_font_mic_16);label("0",434,201,&lv_font_mic_16);
    rms=label("RMS -90 dBFS",26,232,&lv_font_mic_16);
    gain=label("增益 24 dB",26,274,&lv_font_mic_20);
    minus=button(240,258,96,"-",1,NULL);plus=button(346,258,108,"+",2,NULL);
    mute_button=button(26,327,128,"静音",0,&mute_text);
    record_button=button(164,327,140,"录音",3,&record_text);
    play_button=button(314,327,140,"回放",4,&play_text);
    status=label("正在读取录音",26,399,&lv_font_mic_20);lv_obj_set_width(status,428);lv_label_set_long_mode(status,LV_LABEL_LONG_DOT);
    progress=label("48 kHz / 16-bit / Mono",26,440,&lv_font_mic_16);
    mic_ui_update();
}
void mic_ui_update(void){
    mic_snapshot_t s;mic_audio_snapshot(&s);char text[128];
    lv_label_set_text(connection,usb_mic_active()?"电脑正在收音":usb_mic_connected()?"USB 已连接 · 等待电脑收音":"等待连接电脑");
    snprintf(text,sizeof(text),"%.0f dBFS",s.peak_db);lv_label_set_text(peak,text);
    snprintf(text,sizeof(text),"RMS %.0f dBFS   电脑音量 %d dB%s",s.rms_db,usb_mic_host_db(),s.clipped?"  过载":"");lv_label_set_text(rms,text);
    int lit=(int)ceilf((s.peak_db+60)*0.5f);if(lit<0)lit=0;if(lit>30)lit=30;
    for(int i=0;i<30;++i)lv_obj_set_style_bg_color(segments[i],lv_color_hex(i>=lit?0xcbd6df:i>=27?0xbc394a:i>=22?0xb77919:0x218a78),0);
    snprintf(text,sizeof(text),"增益 %d dB",s.gain_db);lv_label_set_text(gain,text);
    lv_label_set_text(mute_text,s.mute?"取消静音":"静音");
    lv_obj_set_style_bg_color(mute_button,lv_color_hex(s.mute?0xbc394a:0x28649d),0);
    bool busy=s.loading||s.mode==MIC_SAVING||s.audio_error;
    enable(minus,!s.audio_error&&s.gain_db>0);enable(plus,!s.audio_error&&s.gain_db<42);
    enable(record_button,!busy&&s.mode!=MIC_PLAYING&&s.storage_ready);
    enable(play_button,!busy&&s.mode!=MIC_RECORDING&&s.samples>0);
    lv_label_set_text(record_text,s.mode==MIC_RECORDING?"停止录音":s.save_error&&s.samples&&!s.saved?"重试保存":"录音");
    lv_label_set_text(play_text,s.mode==MIC_PLAYING?"停止回放":"回放");
    const char *state=s.audio_error?"音频异常，请重启设备":s.loading?"正在读取录音":s.mode==MIC_RECORDING?"正在录音":
        s.mode==MIC_SAVING?"正在保存，请勿断电":s.mode==MIC_PLAYING?"正在回放 · 电脑端静音":!s.storage_ready?"存储不可用，USB 仍可使用":
        s.save_error?(s.samples?"保存失败，可重试保存":"保存文件异常，请重新录音"):s.mute?"已静音 · 输入电平仍可见":usb_mic_host_mute()?"电脑已静音":s.saved?"已保存 · 断电后可回放":"准备就绪 · 最长录音 30 秒";
    lv_label_set_text(status,state);
    snprintf(text,sizeof(text),"%s %.1f / 30.0 秒",s.mode==MIC_PLAYING?"回放":"录音",(double)(s.mode==MIC_PLAYING?s.cursor:s.samples)/MIC_RATE);
    lv_label_set_text(progress,text);
}
