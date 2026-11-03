// SPDX-License-Identifier: GPL-3.0-only
// Display/IMU/recovery integration adapted from Desk-Deck. See docs/REFERENCE.md.
#include "model.h"
#include "network.h"
#include "audio.h"
#include "trader_font.h"
#ifndef TL_HOST
#include "bsp/esp_mosaico.h"
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "iris_ota_support.h"
#include "nvs.h"
#include "nvs_flash.h"
#else
#include "host_platform.h"
#endif
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { HOME, SETTINGS, WIFI, ADD, DETAIL, RULE, REMOVE, DISPLAY, INPUT, ALERTS };
enum { A_NAV, A_SETTINGS=10,A_BACK,A_WIFI,A_DISPLAY,A_REFRESH,A_PREV,A_NEXT,A_ADD,A_ALERTS,A_DETAIL=100,
    A_RULE=260,A_REMOVE,A_CONFIRM_REMOVE,A_SAVE_WIFI,A_SCAN,A_SSID,A_PASSWORD,A_SAVE_DISPLAY,A_SAVE_RULE,
    A_CODE,A_THRESHOLD,A_INPUT_CANCEL,A_INPUT_APPLY,A_ADD_SAVE,A_PREVIEW,A_ACK,A_DIRECTION,A_ENABLE };
enum { EDIT_SSID,EDIT_PASSWORD,EDIT_CODE,EDIT_PRICE };
static tl_config_t config,draft;
#ifdef TL_HOST
#define TL_UI_RAM
#else
#define TL_UI_RAM EXT_RAM_BSS_ATTR
#endif
static TL_UI_RAM tl_net_view_t network;
static tl_crossing_t crossings[TL_MAX_WATCH];
static TL_UI_RAM struct { int32_t values[TL_HISTORY]; unsigned count; int64_t time; uint8_t source; } history[TL_MAX_WATCH];
static lv_obj_t *root,*input_text,*status_label,*brightness_label,*volume_label;
static int panel=HOME,page,watch_page,global_page,selected,edit_field,edit_parent;
static bool redraw=true,save_requested,network_apply,suppress_touch,motor_ready;
static bool preview_sound,stop_sound;
static char notice[96]="连接 Wi-Fi 后获取真实行情", code_draft[7]="", price_draft[24]="";
static uint64_t active_alerts;
static unsigned seen_generation;
static int64_t alarm_started,alarm_until,next_ring;
static atomic_int requested_brightness=40,requested_page=-1;
static atomic_bool screen_test;
static atomic_uint live_page,live_angle,screen_steps;
static atomic_int imu_x,imu_y,imu_z;
static uint8_t *capture;
#ifndef TL_HOST
/* Preserve the last initialization step through an early watchdog reset, when
 * USB logging and a core dump may not yet be available. No settings are stored. */
static RTC_NOINIT_ATTR volatile struct { uint32_t magic,stage; } boot_trace;

#endif
static lv_color_t bg,card,ink,muted,accent,up,down;

static void colors(void)
{
    bg=lv_color_hex(config.light?0xf4f6fa:0x10151f); card=lv_color_hex(config.light?0xffffff:0x1a2230);
    ink=lv_color_hex(config.light?0x172236:0xf1f5fc); muted=lv_color_hex(config.light?0x66758b:0x9eacc1);
    accent=lv_color_hex(config.light?0x245ec5:0x6f9dff);
    up=lv_color_hex(config.light?0xc53049:0xff7585); down=lv_color_hex(config.light?0x087b61:0x44d1aa);
}
static lv_obj_t *label(lv_obj_t *parent,const char *text,int x,int y,int width,const lv_font_t *font,lv_color_t color)
{
    lv_obj_t *o=lv_label_create(parent); lv_label_set_text(o,text); lv_obj_set_pos(o,x,y);
    lv_obj_set_width(o,width); lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(o,font,0); lv_obj_set_style_text_color(o,color,0);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE); return o;
}
static void action(lv_event_t *event);
static lv_obj_t *button(lv_obj_t *parent,int x,int y,int w,int h,const char *text,int id)
{
    lv_obj_t *o=lv_button_create(parent); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    if(h<44) lv_obj_set_ext_click_area(o,4);
    lv_obj_set_style_radius(o,8,0); lv_obj_set_style_bg_color(o,card,0);
    lv_obj_set_style_border_width(o,1,0); lv_obj_set_style_border_color(o,lv_color_mix(muted,card,55),0);
    lv_obj_set_style_shadow_width(o,0,0); lv_obj_set_style_pad_all(o,0,0);
    lv_obj_set_style_bg_color(o,accent,LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(o,card,LV_STATE_DISABLED);
    lv_obj_set_style_opa(o,LV_OPA_40,LV_STATE_DISABLED);
    if(text) { lv_obj_t *l=label(o,text,4,0,w-8,&lv_font_trader_20,ink); lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0); lv_obj_center(l); }
    lv_obj_add_event_cb(o,action,LV_EVENT_CLICKED,(void *)(intptr_t)id); return o;
}
static void say(const char *text) { snprintf(notice,sizeof(notice),"%s",text); if(status_label) lv_label_set_text(status_label,notice); }
static void heading(const char *title)
{
    label(root,title,24,24,344,&lv_font_trader_20,ink);
    button(root,378,14,78,42,"返回",A_BACK);
    status_label=label(root,notice,24,65,432,&lv_font_trader_16,muted);
}
static void request_save(void) { save_requested=true; network_apply=true; }
static void reset_observations(void)
{
    memset(crossings,0,sizeof(crossings)); memset(history,0,sizeof(history));
    memset(network.quotes,0,sizeof(network.quotes)); memset(network.fetched_ok,0,sizeof(network.fetched_ok));
}
static void start_editor(int field,int parent)
{
    edit_field=field; edit_parent=parent; panel=INPUT; redraw=true;
}
static void action(lv_event_t *event)
{
    if(suppress_touch || atomic_load(&screen_test)) return;
    int id=(int)(intptr_t)lv_event_get_user_data(event);
    if(id>=A_NAV && id<A_NAV+2) { page=id; panel=HOME; redraw=true; return; }
    if(id>=A_DETAIL && id<A_DETAIL+TL_MAX_WATCH) { selected=id-A_DETAIL; panel=DETAIL; redraw=true; return; }
    switch(id) {
    case A_SETTINGS: draft=config; panel=SETTINGS; say("网络、显示与提醒偏好"); break;
    case A_BACK:
        atomic_store(&requested_brightness,config.brightness);
        stop_sound=true;
        panel=(panel==WIFI||panel==DISPLAY)?SETTINGS:(panel==RULE||panel==REMOVE)?DETAIL:HOME;
        draft=config; break;
    case A_WIFI: draft=config; panel=WIFI; say("选择网络，输入密码后保存连接"); tl_network_scan(); break;
    case A_DISPLAY: draft=config; panel=DISPLAY; say("亮度即时预览 · 返回撤销"); break;
    case A_REFRESH: tl_network_refresh(); say("已请求刷新 · 连续点击会合并"); break;
    case A_PREV: if(page==1&&panel==HOME){if(global_page>0)global_page--;}else if(watch_page>0)watch_page--;break;
    case A_NEXT: if(page==1&&panel==HOME){if(global_page<1)global_page++;}else if((watch_page+1)*4<config.count)watch_page++;break;
    case A_ALERTS: panel=ALERTS;watch_page=0;break;
    case A_ADD: code_draft[0]=0;panel=ADD;say("最多保存 64 只自选");break;
    case A_CODE: start_editor(EDIT_CODE,ADD); return;
    case A_SSID: start_editor(EDIT_SSID,WIFI); return;
    case A_PASSWORD: start_editor(EDIT_PASSWORD,WIFI); return;
    case A_THRESHOLD: start_editor(EDIT_PRICE,RULE); return;
    case A_SCAN: tl_network_scan(); say("正在扫描附近网络"); break;
    case A_SAVE_WIFI:
        if(!draft.ssid[0]) { say("请先选择或输入网络名称"); return; }
        if(draft.password[0] && strlen(draft.password)<8) { say("Wi-Fi 密码至少需要 8 个字符"); return; }
        memcpy(config.ssid,draft.ssid,sizeof(config.ssid)); memcpy(config.password,draft.password,sizeof(config.password));
        request_save(); memset(crossings,0,sizeof(crossings)); say("正在保存并连接网络"); break;
    case A_SAVE_DISPLAY:
        config.brightness=draft.brightness; config.light=draft.light; config.volume=draft.volume; config.vibration=draft.vibration;
        request_save(); panel=SETTINGS; say("显示与提醒偏好已提交保存"); break;
    case A_ADD_SAVE:
        if(!tl_code_valid(code_draft)) { say("请输入沪深 ETF 的六位代码"); return; }
        if(config.count>=TL_MAX_WATCH) { say("最多保存 64 只，请先移除一只"); return; }
        for(unsigned i=0;i<config.count;i++) if(!strcmp(code_draft,config.watch[i].code)) { say("这只 ETF 已经在自选中"); return; }
        memset(&config.watch[config.count],0,sizeof(config.watch[0])); strcpy(config.watch[config.count].code,code_draft); config.count++;
        reset_observations(); request_save(); panel=HOME; page=0; watch_page=(config.count-1)/4; say("已加入 · 联网核验 ETF 名称与报价"); break;
    case A_RULE:
        draft=config; if(draft.watch[selected].threshold) tl_price_text(draft.watch[selected].threshold,price_draft,sizeof(price_draft));
        else if(network.quotes[selected].price) tl_price_text(network.quotes[selected].price,price_draft,sizeof(price_draft));
        else price_draft[0]=0;
        panel=RULE; say("目标价格以人民币元为单位"); break;
    case A_DIRECTION: draft.watch[selected].below=!draft.watch[selected].below; break;
    case A_ENABLE: draft.watch[selected].enabled=!draft.watch[selected].enabled; break;
    case A_SAVE_RULE: {
        int32_t price;
        if(!tl_price_parse(price_draft,&price)) { say("价格请输入正数，最多三位小数"); return; }
        tl_watch_t *r=&config.watch[selected];
        r->threshold=price; r->below=draft.watch[selected].below; r->enabled=draft.watch[selected].enabled; r->fired=0;
        memset(&crossings[selected],0,sizeof(crossings[selected]));
        crossings[selected].last_time=network.quotes[selected].time;
        request_save(); panel=DETAIL;
        say(r->enabled?"已启用 · 从下一次新报价建立基线":"提醒已关闭"); break;
    }
    case A_REMOVE: panel=REMOVE; break;
    case A_CONFIRM_REMOVE:
        memmove(&config.watch[selected],&config.watch[selected+1],(config.count-selected-1)*sizeof(config.watch[0]));
        memset(&config.watch[--config.count],0,sizeof(config.watch[0]));
        reset_observations(); request_save(); page=0; panel=HOME; watch_page=0; say("已从自选移除"); break;
    case A_INPUT_CANCEL: panel=edit_parent; break;
    case A_INPUT_APPLY: {
        const char *text=lv_textarea_get_text(input_text);
        if((edit_field==EDIT_SSID&&strlen(text)>32)||(edit_field==EDIT_PASSWORD&&strlen(text)>63)) {
            say("内容过长，请缩短后重试"); return;
        }
        if(edit_field==EDIT_SSID) snprintf(draft.ssid,sizeof(draft.ssid),"%s",text);
        else if(edit_field==EDIT_PASSWORD) snprintf(draft.password,sizeof(draft.password),"%s",text);
        else if(edit_field==EDIT_CODE) snprintf(code_draft,sizeof(code_draft),"%s",text);
        else snprintf(price_draft,sizeof(price_draft),"%s",text);
        panel=edit_parent; break;
    }
    case A_PREVIEW: preview_sound=true; break;
    case A_ACK: active_alerts=0; alarm_until=0; stop_sound=true; say("提醒已确认 · 可在详情中重新启用"); break;
    }
    redraw=true;
}
static void ap_event(lv_event_t *e)
{
    unsigned index=lv_dropdown_get_selected(lv_event_get_target_obj(e));
    if(index && index<=network.ap_count) { snprintf(draft.ssid,sizeof(draft.ssid),"%s",network.aps[index-1].ssid); redraw=true; }
}
static void catalog_event(lv_event_t *e)
{
    static const char *codes[]={"","510300","159915","510500","588000","518880","513100"};
    unsigned index=lv_dropdown_get_selected(lv_event_get_target_obj(e));
    if(index<sizeof(codes)/sizeof(codes[0])) { snprintf(code_draft,sizeof(code_draft),"%s",codes[index]); redraw=true; }
}
static void slider_event(lv_event_t *e)
{
    int value=lv_slider_get_value(lv_event_get_target_obj(e));
    if((intptr_t)lv_event_get_user_data(e)==0) {
        draft.brightness=value; atomic_store(&requested_brightness,value);
        lv_label_set_text_fmt(brightness_label,"屏幕亮度 %d%%",value);
    } else { draft.volume=value; lv_label_set_text_fmt(volume_label,"提醒音量 %d%%",value); }
}
static void toggle_event(lv_event_t *e)
{
    bool on=lv_obj_has_state(lv_event_get_target_obj(e),LV_STATE_CHECKED);
    if((intptr_t)lv_event_get_user_data(e)==0) draft.light=on; else draft.vibration=on;
}
static void toggle(int y,const char *text,bool on,int id)
{
    label(root,text,24,y+6,300,&lv_font_trader_20,ink);
    lv_obj_t *o=lv_switch_create(root); lv_obj_set_pos(o,390,y); lv_obj_set_size(o,64,34); lv_obj_set_ext_click_area(o,12);
    if(on) lv_obj_add_state(o,LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(o,accent,LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_add_event_cb(o,toggle_event,LV_EVENT_VALUE_CHANGED,(void *)(intptr_t)id);
}
static void slider(int y,int value,int max,int id)
{
    lv_obj_t *o=lv_slider_create(root); lv_obj_set_pos(o,42,y); lv_obj_set_size(o,390,18);
    lv_slider_set_range(o,id?0:10,max); lv_slider_set_value(o,value,LV_ANIM_OFF);
    lv_obj_set_style_pad_all(o,13,LV_PART_KNOB); lv_obj_set_ext_click_area(o,22);
    lv_obj_set_style_bg_color(o,accent,LV_PART_KNOB);
    lv_obj_set_style_bg_color(o,accent,LV_PART_INDICATOR); lv_obj_add_event_cb(o,slider_event,LV_EVENT_VALUE_CHANGED,(void *)(intptr_t)id);
}
static void quote_time(const tl_quote_t *q,char *out,size_t size)
{
    if(!q->time) { snprintf(out,size,"--:--:--"); return; }
    time_t t=q->time+28800; struct tm tm;
#ifdef _WIN32
    gmtime_s(&tm,&t);
#else
    gmtime_r(&t,&tm);
#endif
    strftime(out,size,"%Y-%m-%d %H:%M:%S",&tm);
}
static tl_quality_t quality(unsigned i) { return tl_quality(&network.quotes[i],time(NULL),network.clock_ok,network.connected&&network.fetched_ok[i]); }
#define WATCH_ROWS 4
static void draw_quote_rows(bool rules)
{
    int max_page=config.count?(config.count-1)/WATCH_ROWS:0;if(watch_page>max_page)watch_page=max_page;
    for(int row=0;row<WATCH_ROWS;row++) {
        unsigned i=(unsigned)(watch_page*WATCH_ROWS+row);if(i>=config.count)break;
        const tl_quote_t *q=&network.quotes[i];const tl_watch_t *rule=&config.watch[i];
        lv_obj_t *o=button(root,16,108+row*68,448,62,NULL,A_DETAIL+i);
        lv_obj_set_style_border_width(o,0,0);
        label(o,q->source?q->name:tl_seed_name(rule->code),12,6,rules?420:240,&lv_font_trader_20,ink);
        char text[120],value[24];
        if(rules) {
            tl_price_text(rule->threshold,value,sizeof(value));
            snprintf(text,sizeof(text),"%s%s   %s",rule->below?"跌破 ":"突破 ",rule->threshold?value:"未设置",rule->fired?"已触发":rule->enabled?"监测中":"未启用");
            label(o,text,12,36,420,&lv_font_trader_16,rule->fired?up:muted);
        } else {
            if(q->source)tl_price_text(q->price,value,sizeof(value));else strcpy(value,"--");
            lv_color_t tone=!q->source||q->price==q->previous?muted:q->price>q->previous?up:down;
            lv_obj_t *p=label(o,value,264,5,172,&lv_font_montserrat_24,ink);lv_obj_set_style_text_align(p,LV_TEXT_ALIGN_RIGHT,0);
            snprintf(text,sizeof(text),"%s  %s%s",rule->code,tl_quality_text(quality(i)),rule->enabled?" · 提醒":"");
            label(o,text,12,36,314,&lv_font_trader_16,muted);
            if(q->source)snprintf(text,sizeof(text),"%+.2f%%",((double)q->price/q->previous-1)*100);else strcpy(text,"--");
            p=label(o,text,336,36,100,&lv_font_trader_16,tone);lv_obj_set_style_text_align(p,LV_TEXT_ALIGN_RIGHT,0);
        }
    }
    if(!config.count)label(root,"还没有自选，点击下方添加 ETF",32,190,416,&lv_font_trader_20,muted);
    lv_obj_t *prev=button(root,16,385,64,36,"上页",A_PREV),*next=button(root,166,385,64,36,"下页",A_NEXT);
    if(!watch_page)lv_obj_add_state(prev,LV_STATE_DISABLED);
    if(watch_page==max_page)lv_obj_add_state(next,LV_STATE_DISABLED);
    char text[24];snprintf(text,sizeof(text),"%d / %d",watch_page+1,max_page+1);label(root,text,94,393,70,&lv_font_trader_16,muted);
    button(root,320,385,144,36,"添加 ETF",A_ADD);
}
static void draw_detail(bool embedded)
{
    if(selected>=config.count) selected=0;
    if(!config.count) { label(root,"添加自选后查看报价详情",36,180,408,&lv_font_trader_20,muted); return; }
    const tl_quote_t *q=&network.quotes[selected];
    int top=embedded?82:100;
    char text[128],price[24],stamp[32]; quote_time(q,stamp,sizeof(stamp));
    snprintf(text,sizeof(text),"%s · %s",config.watch[selected].code,q->source?q->name:"正在核验 ETF");
    label(root,text,24,top,432,&lv_font_trader_20,ink);
    if(q->source) tl_price_text(q->price,price,sizeof(price)); else strcpy(price,"--");
    label(root,price,24,top+33,260,&lv_font_montserrat_48,ink);
    if(q->source) snprintf(text,sizeof(text),"%+.2f%%",((double)q->price/q->previous-1)*100); else strcpy(text,"--");
    label(root,text,306,top+51,150,&lv_font_trader_20,q->price>=q->previous?up:down);
    snprintf(text,sizeof(text),"%s  %s",tl_source_text(q->source),stamp); label(root,text,24,top+91,432,&lv_font_trader_16,muted);
    label(root,tl_quality_text(quality(selected)),24,top+118,432,&lv_font_trader_16,accent);
    if(history[selected].count>=2) {
        lv_obj_t *chart=lv_chart_create(root); lv_obj_set_pos(chart,28,top+151); lv_obj_set_size(chart,424,95);
        lv_obj_set_style_bg_color(chart,card,0); lv_obj_set_style_border_width(chart,0,0); lv_obj_set_style_radius(chart,12,0);
        lv_chart_set_type(chart,LV_CHART_TYPE_LINE); lv_chart_set_point_count(chart,history[selected].count);
        lv_chart_set_div_line_count(chart,2,0); lv_obj_set_style_size(chart,0,0,LV_PART_INDICATOR);
        int32_t lo=INT32_MAX,hi=0;
        for(unsigned i=0;i<history[selected].count;i++) { int32_t v=history[selected].values[i]; if(v<lo)lo=v; if(v>hi)hi=v; }
        lv_chart_set_axis_range(chart,LV_CHART_AXIS_PRIMARY_Y,lo-1,hi+1);
        lv_chart_series_t *series=lv_chart_add_series(chart,accent,LV_CHART_AXIS_PRIMARY_Y);
        for(unsigned i=0;i<history[selected].count;i++) lv_chart_set_next_value(chart,series,history[selected].values[i]);
    } else label(root,"收到两次新报价后显示观察曲线",30,top+181,420,&lv_font_trader_16,muted);
    snprintf(text,sizeof(text),"本次观察 %u 点 · 非历史 K 线",history[selected].count); label(root,text,24,top+254,432,&lv_font_trader_16,muted);
    if(!embedded) { button(root,24,408,208,48,"到价提醒",A_RULE); button(root,248,408,208,48,"移出自选",A_REMOVE); }
    else button(root,312,384,144,38,"查看详情",A_DETAIL+selected);
}
static void index_stamp(int64_t value,char *out,size_t capacity)
{
    if(!value){snprintf(out,capacity,"等待报价");return;}
    time_t t=value+28800;struct tm tm;
#ifdef _WIN32
    gmtime_s(&tm,&t);
#else
    gmtime_r(&t,&tm);
#endif
    strftime(out,capacity,"%m/%d %H:%M",&tm);
}
static void draw_global(void)
{
    for(unsigned cell=0;cell<6;cell++) {
        unsigned i=global_page*6+cell;const tl_index_quote_t *q=&network.indices[i];
        int x=16+(cell%2)*228,y=94+(cell/2)*94;
        lv_obj_t *o=lv_obj_create(root);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,220,86);
        lv_obj_set_style_radius(o,8,0);lv_obj_set_style_bg_color(o,card,0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
        label(o,tl_indices[i].name,12,7,196,&lv_font_trader_20,ink);
        char text[48],stamp[24];bool valid=q->source!=0;
        if(valid)snprintf(text,sizeof(text),"%.2f",q->price/100.0);else strcpy(text,"--");
        label(o,text,12,31,196,&lv_font_montserrat_24,ink);
        index_stamp(q->time,stamp,sizeof(stamp));
        if(!network.connected||!network.index_ok[i])snprintf(stamp,sizeof(stamp),q->source?"离线 / 缓存":"获取中 / 暂无");
        else if(!network.clock_ok)snprintf(stamp,sizeof(stamp),"等待校时");
        else if(time(NULL)-q->time>4*86400)snprintf(stamp,sizeof(stamp),"旧报价");
        label(o,stamp,12,63,115,&lv_font_trader_16,muted);
        if(valid)snprintf(text,sizeof(text),"%+.2f%%",((double)q->price/q->previous-1)*100);else strcpy(text,"--");
        lv_color_t tone=!valid||q->price==q->previous?muted:q->price>q->previous?up:down;
        lv_obj_t *change=label(o,text,130,63,78,&lv_font_trader_16,tone);lv_obj_set_style_text_align(change,LV_TEXT_ALIGN_RIGHT,0);
    }
    lv_obj_t *a=button(root,16,382,86,38,"亚太",A_PREV),*b=button(root,112,382,86,38,"欧美",A_NEXT);
    lv_obj_t *selected_tab=global_page?b:a;
    lv_obj_set_style_bg_color(selected_tab,lv_color_mix(accent,card,45),0);
    lv_obj_set_style_text_color(lv_obj_get_child(selected_tab,0),accent,0);
    label(root,"北京时间 · 公开延时报价",212,393,252,&lv_font_trader_16,muted);
}
static void draw_home(void)
{
    label(root,"TRADER LAB",16,18,236,&lv_font_montserrat_24,ink);
    button(root,306,10,74,44,"刷新",A_REFRESH);button(root,390,10,74,44,"设置",A_SETTINGS);
    label(root,page?"全球指数":"ETF 自选",16,63,158,&lv_font_trader_20,ink);
    char text[96];
    if(!network.connected)snprintf(text,sizeof(text),"未联网 · 显示缓存");
    else if(!network.clock_ok)snprintf(text,sizeof(text),"已联网 · 等待校时");
    else if(page)snprintf(text,sizeof(text),"%s市场 · 已联网",global_page?"欧美":"亚太");
    else snprintf(text,sizeof(text),"%u 只 · %s",config.count,network.fetching?"更新中":tl_market_session(time(NULL))>0?"交易时段":"休市报价");
    lv_obj_t *info=label(root,text,176,68,288,&lv_font_trader_16,muted);lv_obj_set_style_text_align(info,LV_TEXT_ALIGN_RIGHT,0);
    if(page)draw_global();else draw_quote_rows(false);
    for(int i=0;i<2;i++) {
        lv_obj_t *b=button(root,16+i*228,430,220,42,i?"全球指数":"ETF 自选",A_NAV+i);
        lv_obj_set_style_border_width(b,0,0);
        if(page==i){lv_obj_set_style_bg_color(b,accent,0);lv_obj_set_style_text_color(lv_obj_get_child(b,0),config.light?lv_color_white():lv_color_hex(0x0c1628),0);}
    }
}
static void draw_alert_overlay(void)
{
    lv_obj_t *shade=lv_obj_create(root); lv_obj_remove_style_all(shade); lv_obj_set_size(shade,480,480);
    lv_obj_set_style_bg_color(shade,lv_color_black(),0); lv_obj_set_style_bg_opa(shade,LV_OPA_60,0);
    lv_obj_remove_flag(shade,LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(shade,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *box=lv_obj_create(shade); lv_obj_set_pos(box,16,80); lv_obj_set_size(box,448,372);
    lv_obj_set_style_bg_color(box,card,0); lv_obj_set_style_bg_opa(box,LV_OPA_COVER,0);
    lv_obj_set_style_border_color(box,up,0); lv_obj_set_style_border_width(box,2,0); lv_obj_set_style_radius(box,22,0);
    lv_obj_set_style_pad_all(box,12,0); lv_obj_remove_flag(box,LV_OBJ_FLAG_SCROLLABLE);
    label(box,"到价提醒",8,5,396,&lv_font_trader_20,up);
    lv_obj_t *list=lv_obj_create(box); lv_obj_set_pos(list,0,42); lv_obj_set_size(list,420,236);
    lv_obj_set_style_bg_color(list,card,0); lv_obj_set_style_border_width(list,0,0); lv_obj_set_style_pad_all(list,8,0);
    unsigned row=0;
    for(unsigned i=0;i<config.count;i++) if(active_alerts&(UINT64_C(1)<<i)) {
        char text[128],price[24]; tl_price_text(config.watch[i].threshold,price,sizeof(price));
        snprintf(text,sizeof(text),"%s %s %s",config.watch[i].code,config.watch[i].below?"已跌破":"已突破",price);
        label(list,text,0,row++*42,392,&lv_font_trader_20,ink);
    }
    button(box,4,293,408,48,"知道了 · 关闭本次提醒",A_ACK);
}
static void draw_ui(void)
{
    colors(); status_label=NULL; input_text=NULL;
    lv_indev_t *input=bsp_display_get_input_dev();
    if(input&&lv_indev_get_state(input)==LV_INDEV_STATE_PRESSED) suppress_touch=true;
    if(root) lv_obj_delete(root);
    root=lv_obj_create(lv_screen_active()); lv_obj_remove_style_all(root); lv_obj_set_size(root,480,480);
    lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0); lv_obj_set_style_bg_color(root,bg,0); lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLLABLE);
    if(panel==HOME) draw_home();
    else if(panel==DETAIL) { heading("报价详情"); draw_detail(false); }
    else if(panel==ALERTS){heading("到价提醒");lv_label_set_text(status_label,"点击 ETF 设置目标价格与提醒方向");draw_quote_rows(true);}
    else if(panel==SETTINGS) {
        heading("Trader-Lab 设置");
        button(root,24,108,432,60,"Wi-Fi 网络",A_WIFI);button(root,24,180,432,60,"显示与提醒",A_DISPLAY);button(root,24,252,432,60,"自选到价提醒",A_ALERTS);
        label(root,network.connected?"网络已连接":"网络未连接",24,328,432,&lv_font_trader_20,accent);
        
        label(root,"V1.0 / 0.1.0-dev · 独立联网",24,384,432,&lv_font_trader_16,muted);
        label(root,"交易日历覆盖 2026 年",24,418,432,&lv_font_trader_16,muted);
    } else if(panel==WIFI) {
        heading("连接 Wi-Fi");
        char options[384]="选择扫描到的网络";
        for(unsigned i=0;i<network.ap_count;i++) { strncat(options,"\n",sizeof(options)-strlen(options)-1); strncat(options,network.aps[i].ssid,sizeof(options)-strlen(options)-1); }
        lv_obj_t *d=lv_dropdown_create(root); lv_obj_set_pos(d,24,108); lv_obj_set_size(d,320,48); lv_dropdown_set_options(d,options);
        lv_obj_set_style_text_font(d,&lv_font_trader_16,0); lv_obj_set_style_text_font(lv_dropdown_get_list(d),&lv_font_trader_16,0);
        lv_obj_set_style_bg_color(d,card,0); lv_obj_set_style_text_color(d,ink,0);
        lv_obj_set_style_bg_color(lv_dropdown_get_list(d),card,0); lv_obj_set_style_text_color(lv_dropdown_get_list(d),ink,0);
        lv_obj_add_event_cb(d,ap_event,LV_EVENT_VALUE_CHANGED,NULL); button(root,358,108,98,48,network.scanning?"扫描中":"扫描",A_SCAN);
        label(root,"网络名称",24,181,410,&lv_font_trader_16,muted);
        button(root,24,208,432,50,draft.ssid[0]?draft.ssid:"点击输入名称",A_SSID);
        label(root,"密码仅保存在本设备",24,281,410,&lv_font_trader_16,muted);
        button(root,24,308,432,50,draft.password[0]?"已填写密码 · 点击修改":"输入密码 / 开放网络可留空",A_PASSWORD);
        char connection[96];
        if(network.connected) snprintf(connection,sizeof(connection),"已连接 · %s",network.ip);
        else snprintf(connection,sizeof(connection),"%s",network.message);
        label(root,connection,24,376,432,&lv_font_trader_16,accent);
        button(root,24,418,432,48,"保存并连接",A_SAVE_WIFI);
    } else if(panel==DISPLAY) {
        heading("显示与提醒");
        brightness_label=label(root,"",24,106,432,&lv_font_trader_20,ink);
        lv_label_set_text_fmt(brightness_label,"屏幕亮度 %u%%",draft.brightness); slider(157,draft.brightness,100,0);
        volume_label=label(root,"",24,205,432,&lv_font_trader_20,ink);
        lv_label_set_text_fmt(volume_label,"提醒音量 %u%%",draft.volume); slider(254,draft.volume,80,1);
        toggle(295,"日间主题",draft.light,0); toggle(347,"提醒时振动",draft.vibration,1);
        button(root,24,418,208,48,"试听旋律",A_PREVIEW); button(root,248,418,208,48,"保存",A_SAVE_DISPLAY);
    } else if(panel==ADD) {
        heading("添加 ETF"); label(root,"输入六位代码，联网后识别名称",24,114,432,&lv_font_trader_20,ink);
        button(root,24,164,432,60,code_draft[0]?code_draft:"点击输入代码",A_CODE);
        label(root,"或从示例目录选择",24,258,432,&lv_font_trader_16,muted);
        lv_obj_t *d=lv_dropdown_create(root); lv_obj_set_pos(d,24,294); lv_obj_set_size(d,432,52);
        lv_dropdown_set_options(d,"选择示例 ETF\n510300 沪深300 ETF\n159915 创业板 ETF\n510500 中证500 ETF\n588000 科创50 ETF\n518880 黄金 ETF\n513100 纳指 ETF");
        lv_obj_set_style_text_font(d,&lv_font_trader_20,0); lv_obj_set_style_text_font(lv_dropdown_get_list(d),&lv_font_trader_20,0);
        lv_obj_set_style_bg_color(d,card,0); lv_obj_set_style_text_color(d,ink,0);
        lv_obj_set_style_bg_color(lv_dropdown_get_list(d),card,0); lv_obj_set_style_text_color(lv_dropdown_get_list(d),ink,0);
        lv_obj_add_event_cb(d,catalog_event,LV_EVENT_VALUE_CHANGED,NULL);
        label(root,"代码可编辑 · 示例仅便于测试",24,371,432,&lv_font_trader_16,muted);
        button(root,24,418,432,48,"加入自选",A_ADD_SAVE);
    } else if(panel==RULE) {
        heading("设置到价提醒");
        label(root,config.watch[selected].code,24,110,432,&lv_font_montserrat_24,ink);
        button(root,24,158,432,50,draft.watch[selected].below?"方向：向下跌破":"方向：向上突破",A_DIRECTION);
        button(root,24,228,432,56,price_draft[0]?price_draft:"输入目标价格（元）",A_THRESHOLD);
        button(root,24,304,432,48,draft.watch[selected].enabled?"提醒：启用":"提醒：关闭",A_ENABLE);
        label(root,"新报价跨越时提醒一次；重启不补报",24,369,432,&lv_font_trader_16,muted);
        button(root,24,418,432,48,"保存提醒",A_SAVE_RULE);
    } else if(panel==REMOVE) {
        heading("移出自选"); label(root,"移除该 ETF 及它的到价提醒？",24,185,432,&lv_font_trader_20,ink);
        label(root,config.watch[selected].code,24,238,432,&lv_font_montserrat_32,accent);
        button(root,24,374,432,60,"确认移除",A_CONFIRM_REMOVE);
    } else if(panel==INPUT) {
        static const char *titles[]={"输入网络名称","输入 Wi-Fi 密码","输入六位 ETF 代码","输入目标价格（元）"};
        label(root,titles[edit_field],24,26,432,&lv_font_trader_20,ink);
        status_label=label(root,"",24,58,432,&lv_font_trader_16,muted);
        input_text=lv_textarea_create(root); lv_obj_set_pos(input_text,24,86); lv_obj_set_size(input_text,432,68);
        lv_textarea_set_one_line(input_text,true); lv_obj_set_style_text_font(input_text,&lv_font_trader_20,0);
        lv_obj_set_style_bg_color(input_text,card,0); lv_obj_set_style_text_color(input_text,ink,0);
        lv_obj_set_style_border_color(input_text,ink,LV_PART_CURSOR);
        lv_textarea_set_max_length(input_text,edit_field==EDIT_SSID?32:edit_field==EDIT_PASSWORD?63:edit_field==EDIT_CODE?6:12);
        if(edit_field>=EDIT_CODE) lv_textarea_set_accepted_chars(input_text,edit_field==EDIT_CODE?"0123456789":"0123456789.");
        if(edit_field==EDIT_PASSWORD) lv_textarea_set_password_mode(input_text,true);
        lv_textarea_set_text(input_text,edit_field==EDIT_SSID?draft.ssid:edit_field==EDIT_PASSWORD?draft.password:edit_field==EDIT_CODE?code_draft:price_draft);
        lv_obj_t *keyboard=lv_keyboard_create(root); lv_obj_set_align(keyboard,LV_ALIGN_TOP_LEFT);
        lv_obj_set_pos(keyboard,0,186); lv_obj_set_size(keyboard,480,222);
        lv_obj_set_style_text_font(keyboard,&lv_font_trader_20,0);
        lv_obj_set_style_bg_color(keyboard,bg,0); lv_obj_set_style_bg_color(keyboard,card,LV_PART_ITEMS);
        lv_obj_set_style_text_color(keyboard,ink,LV_PART_ITEMS); lv_obj_set_style_shadow_width(keyboard,0,LV_PART_ITEMS);
        lv_obj_set_style_bg_color(keyboard,muted,LV_PART_ITEMS|LV_STATE_CHECKED);
        lv_obj_set_style_text_color(keyboard,bg,LV_PART_ITEMS|LV_STATE_CHECKED);
        if(edit_field>=EDIT_CODE) lv_keyboard_set_mode(keyboard,LV_KEYBOARD_MODE_NUMBER);
        lv_keyboard_set_textarea(keyboard,input_text);
        lv_obj_add_event_cb(keyboard,action,LV_EVENT_READY,(void *)(intptr_t)A_INPUT_APPLY);
        lv_obj_add_event_cb(keyboard,action,LV_EVENT_CANCEL,(void *)(intptr_t)A_INPUT_CANCEL);
        button(root,24,422,208,44,"取消",A_INPUT_CANCEL); button(root,248,422,208,44,"确定",A_INPUT_APPLY);
    }
    if(active_alerts) draw_alert_overlay();
    atomic_store(&live_page,page); redraw=false;
}
#ifndef TL_HOST
static void load_config(void)
{
    tl_defaults(&config); nvs_handle_t h;
    if(nvs_open("traderlab",NVS_READONLY,&h)!=ESP_OK) return;
    tl_config_t saved;size_t len=sizeof(saved);
    if(nvs_get_blob(h,"config_v2",&saved,&len)==ESP_OK&&len==sizeof(saved)&&tl_config_valid(&saved))config=saved;
    else {
        tl_config_v1_t old;len=sizeof(old);
        if(nvs_get_blob(h,"config_v1",&old,&len)==ESP_OK&&len==sizeof(old)&&tl_migrate_v1(&old,&saved))config=saved;
        save_requested=true;
    }
    nvs_close(h);
}
static esp_err_t save_config(const tl_config_t *value)
{
    nvs_handle_t h; esp_err_t err=nvs_open("traderlab",NVS_READWRITE,&h);
    if(err==ESP_OK) { err=nvs_set_blob(h,"config_v2",value,sizeof(*value)); if(err==ESP_OK)err=nvs_commit(h); nvs_close(h); }
    ESP_LOGI("tl_config","save=%s",esp_err_to_name(err)); return err;
}
#endif
static void observe(void)
{
    int64_t now=esp_timer_get_time();
    for(unsigned i=0;i<config.count;i++) {
        const tl_quote_t *q=&network.quotes[i]; tl_quality_t state=quality(i);
        bool eligible=state==TL_CURRENT;
        if(eligible && q->time>history[i].time) {
            if(q->source!=history[i].source || q->time-history[i].time>120) history[i].count=0;
            if(history[i].count==TL_HISTORY) { memmove(history[i].values,history[i].values+1,(TL_HISTORY-1)*sizeof(int32_t)); history[i].count--; }
            history[i].values[history[i].count++]=q->price; history[i].time=q->time; history[i].source=q->source;
        }
        if(tl_crossing(&crossings[i],&config.watch[i],q,eligible)) {
            config.watch[i].fired=1; config.watch[i].enabled=0; save_requested=true;
            active_alerts|=UINT64_C(1)<<i; alarm_started=now; alarm_until=now+30000000; next_ring=now; redraw=true;
            ESP_LOGI("tl_alert","crossing code=%s direction=%s",config.watch[i].code,config.watch[i].below?"below":"above");
        }
    }
}

#ifndef TL_HOST
static esp_err_t screen_begin(const esp_iris_media_desc_t *request,esp_iris_media_desc_t *actual,uint32_t *size,void *context)
{
    (void)request; (void)context; if(capture)return ESP_ERR_INVALID_STATE;
    uint8_t *bytes=heap_caps_malloc(480*480*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT); if(!bytes)return ESP_ERR_NO_MEM;
    if(!bsp_display_lock(500)) {free(bytes);return ESP_ERR_TIMEOUT;}
    lv_display_t *d=bsp_display_get(); lv_draw_buf_t *b=lv_display_get_buf_active(d);
    bool full=b && b->header.cf==LV_COLOR_FORMAT_RGB565 && b->header.w==480 && b->header.h==480 && b->header.stride==960 && lv_display_get_render_mode(d)==LV_DISPLAY_RENDER_MODE_FULL;
    if(full)for(size_t i=0;i<480*480*2;i+=2){bytes[i]=b->data[i+1];bytes[i+1]=b->data[i];}
    bsp_display_unlock(); if(!full){free(bytes);return ESP_ERR_NOT_SUPPORTED;}
    capture=bytes; *size=480*480*2; *actual=(esp_iris_media_desc_t){.width=480,.height=480,.stride=960,.format=ESP_IRIS_PIXEL_FORMAT_RGB565};return ESP_OK;
}
static esp_err_t screen_read(uint32_t offset,uint8_t *out,size_t capacity,size_t *used,void *ctx)
{
    (void)ctx;if(!capture||offset>=480*480*2)return ESP_ERR_INVALID_ARG;
    *used=480*480*2-offset;if(*used>capacity)*used=capacity;memcpy(out,capture+offset,*used);return ESP_OK;
}
static void screen_end(void *ctx){(void)ctx;free(capture);capture=NULL;}
static esp_err_t diagnostic_rpc(const esp_iris_rpc_request_t *r,uint8_t *response,size_t capacity,size_t *used,void *ctx)
{
    (void)ctx;
    if(r->payload_size==11&&!memcmp(r->payload,"screen-test",11))atomic_store(&screen_test,true);
    else if(r->payload_size==4&&!memcmp(r->payload,"stop",4))atomic_store(&screen_test,false);
    else if(r->payload_size==2&&r->payload[0]=='p'&&r->payload[1]>='0'&&r->payload[1]<='1')atomic_store(&requested_page,r->payload[1]-'0');
    else if(r->payload_size==7&&!memcmp(r->payload,"refresh",7))tl_network_refresh();
    else if(r->payload_size!=6||memcmp(r->payload,"status",6))return ESP_ERR_INVALID_ARG;
    tl_net_view_t *v=heap_caps_malloc(sizeof(*v),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!v)return ESP_ERR_NO_MEM;tl_network_snapshot(v);
    unsigned quotes=0,indices=0;for(unsigned i=0;i<TL_MAX_WATCH;i++)if(v->quotes[i].source)quotes++;
    for(unsigned i=0;i<TL_GLOBAL_COUNT;i++)if(v->indices[i].source&&v->index_ok[i])indices++;
    int n=snprintf((char *)response,capacity,"{\"wifi\":%s,\"clock\":%s,\"quotes\":%u,\"indices\":%u,\"global_requests\":%u,\"global_failures\":%u,\"requests\":%u,\"failures\":%u,\"http\":%d,\"page\":%u,\"angle\":%u,\"test_steps\":%u,\"audio_error\":%s,\"free_internal\":%u,\"accel_mg\":[%d,%d,%d]}",
        v->connected?"true":"false",v->clock_ok?"true":"false",quotes,indices,v->global_requests,v->global_failures,v->requests,v->failures,v->http_status,atomic_load(&live_page),atomic_load(&live_angle),atomic_load(&screen_steps),tl_audio_error()?"true":"false",(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),atomic_load(&imu_x),atomic_load(&imu_y),atomic_load(&imu_z));
    free(v);if(n<0||(size_t)n>=capacity)return ESP_ERR_INVALID_SIZE;
    *used=(size_t)n;return ESP_OK;
}
void app_main(void)
{
    unsigned previous_stage=boot_trace.magic==0x544c4231?boot_trace.stage:0;
    unsigned previous_network_stage=tl_network_boot_stage();
    boot_trace.magic=0x544c4231;boot_trace.stage=1;
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_boot_probe());boot_trace.stage=2;
    ESP_ERROR_CHECK(nvs_flash_init());boot_trace.stage=3;
    load_config(); draft=config;
    /* Initialize the radio before USB and LVGL create concurrent activity. */
    boot_trace.stage=6;bool net_ready=tl_network_start(&config);boot_trace.stage=7;
    iris_ota_support_start();boot_trace.stage=4;
    ESP_LOGI("tl_boot","previous_stage=%u network_stage=%u",previous_stage,previous_network_stage);
    bool audio_ready=tl_audio_start();boot_trace.stage=8;
    atomic_store(&requested_brightness,config.brightness);
    ESP_ERROR_CHECK(bsp_display_start()?ESP_OK:ESP_FAIL);boot_trace.stage=5;
    ESP_ERROR_CHECK(bsp_display_brightness_set(config.brightness));
    if(bsp_display_lock(1000)){draw_ui();bsp_display_unlock();}
    esp_err_t imu=bsp_imu_init(); if(imu==ESP_OK)imu=bsp_imu_start(NULL); bool imu_ready=imu==ESP_OK;
    boot_trace.stage=9;motor_ready=bsp_motor_init()==ESP_OK;
    const esp_iris_screen_backend_t backend={.begin=screen_begin,.read=screen_read,.end=screen_end};
    ESP_ERROR_CHECK(esp_iris_screen_register(&backend)); ESP_ERROR_CHECK(esp_iris_rpc_register(0x5452,1,diagnostic_rpc,NULL));
    if(!net_ready||!audio_ready) { if(bsp_display_lock(100)){say("服务启动异常，请重启后查看日志");bsp_display_unlock();} }
    ESP_LOGI("tl_start","network=%d audio=%d imu=%d previous=%u net_step=%u",net_ready,audio_ready,imu_ready,previous_stage,previous_network_stage);
    boot_trace.stage=10;ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_mark_healthy());boot_trace.stage=11;
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_task_wdt_add(NULL));
    int angle=0,candidate=-1,stable=-1,brightness=config.brightness;
    int64_t candidate_at=0,last_observe=0,test_started=0,last_log=0,imu_retry=0;
    bool motor_on=false;
    tl_quality_t prior_quality[TL_MAX_WATCH]={0};
    tl_net_view_t *snapshot=heap_caps_calloc(1,sizeof(*snapshot),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);ESP_ERROR_CHECK(snapshot?ESP_OK:ESP_ERR_NO_MEM);
    for(;;) {
        int64_t now=esp_timer_get_time(); int desired_angle=angle,desired_page=-1;
        if(imu_ready&&now>=imu_retry){
            float x,y,z;
            if(bsp_imu_get_accel(&x,&y,&z)==ESP_OK){
                atomic_store(&imu_x,x*1000);atomic_store(&imu_y,y*1000);atomic_store(&imu_z,z*1000);
                int orientation=-1;
                if(fabsf(z)<.72f){if(fabsf(y)>.72f&&fabsf(x)<.5f)orientation=y>0?0:2;else if(fabsf(x)>.72f&&fabsf(y)<.5f)orientation=x>0?1:3;}
                if(orientation!=candidate){candidate=orientation;candidate_at=now;}
                if(candidate>=0&&candidate!=stable&&now-candidate_at>=400000){stable=candidate;desired_angle=stable*90;desired_page=stable%2;}
            }else{candidate=-1;imu_retry=now+1000000;}
        }
        if(atomic_load(&screen_test)){
            if(!test_started)test_started=now;
            unsigned step=(now-test_started)/1000000;
            if(step<24){static const int levels[]={20,70,40,100};desired_angle=step%4*90;desired_page=step%2;atomic_store(&requested_brightness,levels[step%4]);atomic_store(&screen_steps,step+1);}
            else atomic_store(&screen_test,false);
        }else if(test_started){test_started=0;desired_angle=stable>=0?stable*90:0;if(bsp_display_lock(100)){atomic_store(&requested_brightness,config.brightness);bsp_display_unlock();}}
        int command=atomic_exchange(&requested_page,-1);if(command>=0)desired_page=command;
        if(desired_angle!=angle&&bsp_display_set_rotation((bsp_display_rotation_t)desired_angle)==ESP_OK){angle=desired_angle;atomic_store(&live_angle,angle);}
        int wanted=atomic_load(&requested_brightness);if(wanted!=brightness&&bsp_display_brightness_set(wanted)==ESP_OK)brightness=wanted;
        tl_network_snapshot(snapshot);
        bool save=false,apply=false,ring=false,motor=false,stop=false;unsigned volume=0;tl_config_t saved;
        if(bsp_display_lock(100)){
            lv_indev_t *in=bsp_display_get_input_dev();bool touching=in&&lv_indev_get_state(in)==LV_INDEV_STATE_PRESSED;
            if(!touching)suppress_touch=false;
            network=*snapshot;
            for(unsigned i=0;i<config.count;i++) {
                if(strcmp(network.quotes[i].code,config.watch[i].code)) {
                    memset(&network.quotes[i],0,sizeof(network.quotes[i])); network.fetched_ok[i]=false;
                }
                tl_quality_t current=quality(i);
                if(current!=prior_quality[i]) { prior_quality[i]=current; if(panel==HOME||panel==DETAIL)redraw=true; }
            }
            if(desired_page>=0&&panel==HOME&&!active_alerts&&page!=desired_page){page=desired_page;redraw=true;}
            if(now-last_observe>=500000){
                last_observe=now;observe();
                if(seen_generation!=network.generation){seen_generation=network.generation;if(panel==HOME||panel==DETAIL)redraw=true;else if(panel==WIFI&&!touching)redraw=true;}
            }
            if(preview_sound){ring=true;volume=draft.volume;preview_sound=false;}
            if(active_alerts&&now<alarm_until){
                if(now>=next_ring){next_ring=now+3000000;ring=true;volume=config.volume;}
                int64_t phase=(now-alarm_started)%3000000;motor=config.vibration&&(phase<180000||(phase>=340000&&phase<520000));
            }
            if(stop_sound){stop_sound=false;stop=true;}
            if(save_requested){saved=config;save_requested=false;save=true;apply=network_apply;network_apply=false;}
            if(redraw&&!touching)draw_ui();
            bsp_display_unlock();
        }
        if(stop)tl_audio_stop();else if(ring)tl_audio_play(volume);
        if(motor_ready&&motor!=motor_on){bsp_motor_set(motor);motor_on=motor;}
        if(save){
            esp_err_t err=save_config(&saved);if(apply)tl_network_configure(&saved);
            if(err!=ESP_OK&&bsp_display_lock(100)){say("保存失败，重启后可能恢复旧配置");bsp_display_unlock();}
        }
        if(now-last_log>=30000000){last_log=now;ESP_LOGI("tl_status","wifi=%d clock=%d requests=%u failed=%u internal=%u",snapshot->connected,snapshot->clock_ok,snapshot->requests,snapshot->failures,(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));}
        (void)esp_task_wdt_reset();vTaskDelay(pdMS_TO_TICKS(25));
    }
}
#endif
