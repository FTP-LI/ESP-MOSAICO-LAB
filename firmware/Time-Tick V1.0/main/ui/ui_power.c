// SPDX-License-Identifier: GPL-3.0-only
#include "ui_power.h"
#include "ui_text.h"
#include <stdio.h>
#include <string.h>
static tt_power_view_t current;
static lv_obj_t *badge_text,*fill,*details;
static uint8_t lang;
static unsigned activity_idle_ms;
void tt_power_ui_activity(unsigned idle_ms,bool keep_active)
{
    activity_idle_ms=keep_active?0:idle_ms;
}
unsigned tt_visual_period(bool atmosphere)
{
    unsigned idle=lv_display_get_inactive_time(NULL);
    if (activity_idle_ms<idle) idle=activity_idle_ms;
    return tt_power_visual_period(&current,atmosphere,idle);
}
static const char *state_text(void)
{
    if (!current.valid) return "Battery unavailable";
    const char *names[]={"Battery unavailable","No battery detected","On battery","Charging","Fully charged","Battery idle"};
    return names[current.state];
}
static void refresh(void)
{
    char text[240];
    if (badge_text) {
        if (current.valid && current.state!=TT_POWER_ABSENT) snprintf(text,sizeof(text),"%u%%%s",current.percent,current.state==TT_POWER_CHARGING?"+":"");
        else snprintf(text,sizeof(text),"--");
        tt_label_set_text(badge_text,text);
        int w=current.valid && current.state!=TT_POWER_ABSENT?(int)(current.percent*19/100):0;
        lv_obj_set_width(fill,w);
        tt_theme_bind(fill,LV_STYLE_BG_COLOR,current.valid && current.percent<=15?0xb77537:TT_GREEN,0);
    }
    if (details) {
        const char *state=tt_text(lang,state_text());
        if (current.valid && current.state!=TT_POWER_ABSENT)
            snprintf(text,sizeof(text),"%s%s\n%u%%  ·  %.3f V\n%s: %+d mA",current.simulated?tt_text(lang,"Simulated battery / "):"",state,current.percent,current.voltage_mv/1000.0,tt_text(lang,"Battery current"),current.current_ma);
        else snprintf(text,sizeof(text),"%s%s",current.simulated?tt_text(lang,"Simulated battery / "):"",state);
        tt_label_set_text(details,text);
    }
}
static void clear_details(lv_event_t *e) { (void)e; details=NULL; }
static void clear_badge(lv_event_t *e) { (void)e; badge_text=fill=NULL; }
lv_obj_t *tt_power_badge(lv_obj_t *parent,lv_event_cb_t open)
{
    lv_obj_t *b=lv_button_create(parent); lv_obj_remove_style_all(b);
    lv_obj_set_pos(b,280,18); lv_obj_set_size(b,92,38);
    tt_theme_bind(b,LV_STYLE_BG_COLOR,TT_CREAM,0); lv_obj_set_style_bg_opa(b,210,0); lv_obj_set_style_radius(b,12,0);
    lv_obj_t *outline=lv_obj_create(b); lv_obj_remove_style_all(outline); lv_obj_set_pos(outline,7,13); lv_obj_set_size(outline,25,13);
    lv_obj_set_style_border_width(outline,1,0); tt_theme_bind(outline,LV_STYLE_BORDER_COLOR,TT_GREEN,0); lv_obj_set_style_radius(outline,3,0);
    lv_obj_remove_flag(outline,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    fill=lv_obj_create(outline); lv_obj_remove_style_all(fill); lv_obj_set_pos(fill,2,2); lv_obj_set_size(fill,0,7);
    lv_obj_set_style_bg_opa(fill,255,0); lv_obj_remove_flag(fill,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    badge_text=lv_label_create(b); lv_obj_set_pos(badge_text,37,9); lv_obj_set_style_text_font(badge_text,&tt_font_16,0);
    tt_theme_bind(badge_text,LV_STYLE_TEXT_COLOR,TT_INK,0);
    lv_obj_add_event_cb(b,open,LV_EVENT_CLICKED,NULL); lv_obj_add_event_cb(b,clear_badge,LV_EVENT_DELETE,NULL); refresh(); return b;
}
void tt_power_details(lv_obj_t *parent,uint8_t language)
{
    lang=language;
    details=lv_label_create(parent); lv_obj_set_width(details,LV_PCT(100)); lv_obj_set_style_text_font(details,&tt_font_24,0);
    lv_obj_add_event_cb(details,clear_details,LV_EVENT_DELETE,NULL); refresh();
    lv_obj_t *note=lv_label_create(parent); lv_obj_set_width(note,LV_PCT(100));
    lv_obj_set_style_text_font(note,&tt_font_16,0);
    lv_label_set_text(note,tt_text(lang,"Gauge read every 10 seconds. On battery: gentler animation; dim after 1 minute idle. Touch or flip to restore. Timers keep running."));
}
void tt_power_ui_update(const tt_power_view_t *power,uint8_t language)
{
    if (lang==language && !memcmp(&current,power,sizeof(current))) return;
    current=*power; lang=language; refresh();
}
