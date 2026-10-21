// SPDX-License-Identifier: GPL-3.0-only
#include "ui_focus.h"
#include "ui_text.h"
#include "project_identity.h"
#include <stdio.h>
#ifdef ESP_PLATFORM
#include "esp_log.h"
#define DIAL_LOG(...) ESP_LOGI("tt_dial",__VA_ARGS__)
#else
#define DIAL_LOG(...) ((void)0)
#endif

typedef struct {
    uint32_t background,ink,accent,track,leaf,chip;
} palette_t;
static const palette_t palettes[]={
    {0xf6f8ef,0x304d3d,0x5b8664,0xdce6d5,0xc3d4b4,0xeaf1e3}, /* Running */
    {0xf9f5ec,0x66553d,0xaa844c,0xe8ddc8,0xd7c8a9,0xf2e8d5}, /* Paused */
    {0xfff8e6,0x66502d,0xb88c38,0xead9ae,0xdccb9c,0xf7ebc9}, /* Finished */
    {0xf5f6f1,0x586556,0x87977b,0xe0e5d9,0xd1d9c4,0xebeee5}  /* Ready */
};
typedef struct {
    lv_obj_t *title,*state,*time,*preset,*hint,*ring,*dial,*percent,*hit,*hold_hint;
    int palette;
    bool alarm,enabled,pressed,held,cancelled;
    uint32_t pressed_at;
    int pressed_phase;
    tt_face_t face;
    lv_event_cb_t toggle,stop;
} focus_t;

static lv_obj_t *plain(lv_obj_t *parent,int x,int y,int w,int h)
{
    lv_obj_t *o=lv_obj_create(parent); lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE); return o;
}
static lv_obj_t *label(lv_obj_t *parent,const char *text,int x,int y,int w,int h,const lv_font_t *font)
{
    lv_obj_t *o=lv_label_create(parent); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_label_set_long_mode(o,LV_LABEL_LONG_DOT); tt_label_set_text(o,text);
    lv_obj_set_style_text_font(o,font,0); lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0); return o;
}
static void line(lv_layer_t *layer,int x,int y,int x2,int y2,int width,uint32_t color)
{
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d);
    d.p1=(lv_point_precise_t){x,y}; d.p2=(lv_point_precise_t){x2,y2};
    d.width=width; d.color=lv_color_hex(tt_theme_color(LV_STYLE_LINE_COLOR,color)); d.round_start=d.round_end=1; lv_draw_line(layer,&d);
}
static void sprig(lv_layer_t *layer,int x,int y,int side,uint32_t color)
{
    line(layer,x,y,x+side*8,y-60,2,color);
    for (int i=0;i<3;++i) {
        int cy=y-12-i*17,cx=x+side*(2+i*2);
        line(layer,cx,cy,cx-side*12,cy-12,7,color);
        line(layer,cx+side,cy-8,cx+side*13,cy-19,6,color);
    }
}
static void backdrop(lv_event_t *e)
{
    focus_t *v=lv_event_get_user_data(e); const palette_t *p=&palettes[v->palette<0?0:v->palette];
    lv_area_t a; lv_obj_get_coords(lv_event_get_target_obj(e),&a);
    lv_layer_t *l=lv_event_get_layer(e); int x=a.x1,y=a.y1;
    sprig(l,x+39,y+340,1,p->leaf); sprig(l,x+443,y+164,-1,p->leaf);
    /* Fine dial marks provide scale without adding competing numbers. */
    for (int i=0;i<60;++i) {
        int angle=i*6,r=i%5?161:158;
        int cx=lv_trigo_cos(angle),sy=lv_trigo_sin(angle);
        line(l,x+240+cx*r/LV_TRIGO_SIN_MAX,y+229+sy*r/LV_TRIGO_SIN_MAX,
             x+240+cx*164/LV_TRIGO_SIN_MAX,y+229+sy*164/LV_TRIGO_SIN_MAX,
             i%5?1:2,i%5?p->track:p->leaf);
    }
}
static void emblem(lv_event_t *e)
{
    focus_t *v=lv_event_get_user_data(e); const palette_t *p=&palettes[v->palette<0?0:v->palette];
    lv_area_t a; lv_obj_get_coords(lv_event_get_target_obj(e),&a);
    lv_layer_t *l=lv_event_get_layer(e); int x=a.x1+140,y=a.y1+31;
    if (v->alarm) {
        line(l,x-8,y+8,x-2,y+14,3,p->accent); line(l,x-2,y+14,x+9,y+1,3,p->accent);
    } else {
        line(l,x-8,y,x+8,y,2,p->accent); line(l,x-8,y+20,x+8,y+20,2,p->accent);
        line(l,x-6,y+3,x+6,y+17,2,p->accent); line(l,x+6,y+3,x-6,y+17,2,p->accent);
        line(l,x-3,y+17,x+3,y+17,2,p->accent);
    }
}
static bool dial_inside(focus_t *v,lv_indev_t *input)
{
    if (!input) return false;
    lv_point_t point; lv_indev_get_point(input,&point);
    lv_area_t area; lv_obj_get_coords(v->hit,&area);
    int32_t dx=2*point.x-area.x1-area.x2,dy=2*point.y-area.y1-area.y2;
    int32_t diameter=area.x2-area.x1+1;
    return dx*dx+dy*dy<=diameter*diameter;
}
static void dial_gesture(lv_event_t *e)
{
    focus_t *v=lv_event_get_user_data(e);
    lv_event_code_t code=lv_event_get_code(e);
    if (code==LV_EVENT_PRESSED) {
        v->pressed=v->enabled && dial_inside(v,lv_event_get_indev(e)); v->held=v->cancelled=false;
        v->pressed_at=lv_tick_get(); v->pressed_phase=v->palette;
        if (v->pressed) {
            lv_point_t point={0}; lv_indev_t *input=lv_event_get_indev(e);
            if (input) lv_indev_get_point(input,&point);
            DIAL_LOG("press phase=%d x=%ld y=%ld",v->palette,(long)point.x,(long)point.y);
        } else lv_obj_remove_state(v->hit,LV_STATE_PRESSED);
    } else if (code==LV_EVENT_PRESS_LOST) {
        v->pressed=false; v->cancelled=true;
        lv_obj_remove_state(v->hit,LV_STATE_PRESSED);
        DIAL_LOG("cancel: target lost");
    } else if (v->pressed && (code==LV_EVENT_PRESSING || code==LV_EVENT_RELEASED)) {
        if (!dial_inside(v,lv_event_get_indev(e))) v->cancelled=true;
        if (!v->held && v->palette!=v->pressed_phase) v->cancelled=true;
        uint32_t elapsed=lv_tick_elaps(v->pressed_at);
        /* Keep native short/long-click events out of this handler: its 400ms
         * default makes an ordinary slower tap destructive. Only this dial
         * uses an 800ms hold; scrolling and other controls are unchanged. */
        if (!v->cancelled && !v->held && elapsed>=800) {
            v->held=true; v->stop(e); DIAL_LOG("hold: stop/reset after %lu ms",(unsigned long)elapsed);
            lv_obj_remove_state(v->hit,LV_STATE_PRESSED);
        }
        if (v->cancelled) lv_obj_remove_state(v->hit,LV_STATE_PRESSED);
        if (code==LV_EVENT_RELEASED) {
            if (!v->cancelled && !v->held) {
                if (v->palette==2) v->stop(e); else v->toggle(e);
            }
            DIAL_LOG("release: %lu ms phase=%d held=%d cancelled=%d",(unsigned long)elapsed,v->palette,v->held,v->cancelled);
            v->pressed=false;
        }
    }
}
static void deleted(lv_event_t *e) { lv_free(lv_event_get_user_data(e)); }
lv_obj_t *tt_focus_create(lv_obj_t *parent,lv_event_cb_t toggle,lv_event_cb_t stop)
{
    lv_obj_t *root=plain(parent,0,0,480,480);
    lv_obj_add_flag(root,LV_OBJ_FLAG_CLICKABLE); /* No touches pass to the hidden garden. */
    lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);
    lv_obj_set_style_bg_main_opa(root,250,0); lv_obj_set_style_bg_grad_opa(root,15,0);
    lv_obj_set_style_bg_grad_dir(root,LV_GRAD_DIR_VER,0);
    lv_obj_set_style_bg_main_stop(root,80,0); lv_obj_set_style_bg_grad_stop(root,230,0);
    focus_t *v=lv_malloc(sizeof(*v)); LV_ASSERT_MALLOC(v); *v=(focus_t){.palette=0,.toggle=toggle,.stop=stop};
    lv_obj_set_user_data(root,v);
    lv_obj_add_event_cb(root,backdrop,LV_EVENT_DRAW_MAIN,v);
    lv_obj_add_event_cb(root,deleted,LV_EVENT_DELETE,v);
    v->title=label(root,"",24,17,310,32,&tt_font_24);
    lv_obj_set_style_text_align(v->title,LV_TEXT_ALIGN_LEFT,0);
    lv_obj_t *brand=label(root,TT_PRODUCT_NAME "  /  " TT_PRODUCT_VERSION,25,49,240,20,&tt_font_16);
    lv_obj_set_style_text_align(brand,LV_TEXT_ALIGN_LEFT,0);
    tt_theme_bind(brand,LV_STYLE_TEXT_COLOR,0x80927d,0);
    v->dial=plain(root,100,89,280,280);
    lv_obj_set_style_radius(v->dial,LV_RADIUS_CIRCLE,0); tt_theme_bind(v->dial,LV_STYLE_BG_COLOR,0xfffdf5,0);
    lv_obj_set_style_bg_opa(v->dial,232,0); lv_obj_set_style_border_width(v->dial,1,0);
    tt_theme_bind(v->dial,LV_STYLE_BORDER_COLOR,0xffffff,0);
    lv_obj_set_style_shadow_width(v->dial,28,0); lv_obj_set_style_shadow_opa(v->dial,20,0);
    tt_theme_bind(v->dial,LV_STYLE_SHADOW_COLOR,0x617757,0); lv_obj_set_style_shadow_offset_y(v->dial,8,0);
    lv_obj_add_event_cb(v->dial,emblem,LV_EVENT_DRAW_MAIN,v);
    v->ring=lv_arc_create(root); lv_obj_remove_style_all(v->ring);
    lv_obj_set_pos(v->ring,88,77); lv_obj_set_size(v->ring,304,304);
    lv_arc_set_rotation(v->ring,270); lv_arc_set_bg_angles(v->ring,0,360); lv_arc_set_range(v->ring,0,1000);
    lv_obj_set_style_arc_width(v->ring,6,LV_PART_MAIN);
    lv_obj_set_style_arc_width(v->ring,7,LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(v->ring,true,LV_PART_INDICATOR);
    lv_obj_remove_flag(v->ring,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    v->state=label(root,"",143,147,194,28,&tt_font_16);
    lv_obj_set_style_bg_opa(v->state,LV_OPA_COVER,0); lv_obj_set_style_radius(v->state,14,0);
    lv_obj_set_style_pad_ver(v->state,4,0);
    v->time=label(root,"00:00",104,181,272,96,&tt_font_84);
    v->preset=label(root,"",126,281,228,24,&tt_font_16);
    v->percent=label(root,"",132,328,216,22,&tt_font_16);
    /* Include the outer tick marks and a 12px finger margin. Capture the
     * press so jitter cannot retarget a label; leaving the circle cancels. */
    lv_obj_t *hit=plain(root,64,53,352,352); v->hit=hit;
    lv_obj_set_style_radius(hit,LV_RADIUS_CIRCLE,0);
    lv_obj_add_flag(hit,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_style_border_width(hit,2,LV_STATE_PRESSED);
    lv_obj_set_style_border_opa(hit,180,LV_STATE_PRESSED);
    lv_obj_add_event_cb(hit,dial_gesture,LV_EVENT_PRESSED,v);
    lv_obj_add_event_cb(hit,dial_gesture,LV_EVENT_PRESSING,v);
    lv_obj_add_event_cb(hit,dial_gesture,LV_EVENT_RELEASED,v);
    lv_obj_add_event_cb(hit,dial_gesture,LV_EVENT_PRESS_LOST,v);
    lv_obj_t *help=plain(root,20,405,440,64);
    tt_theme_bind(help,LV_STYLE_BG_COLOR,0xfffdf5,0);
    lv_obj_set_style_bg_opa(help,232,0); lv_obj_set_style_radius(help,16,0);
    v->hint=label(help,"",0,5,440,24,&tt_font_18);
    v->hold_hint=label(help,"",0,34,440,22,&tt_font_16);
    v->palette=-1; lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN); return root;
}
void tt_focus_render(lv_obj_t *root,const tt_timer_t *t,tt_face_t face,const tt_settings_t *s,bool alarm)
{
    focus_t *v=lv_obj_get_user_data(root);
    int phase=alarm?2:t->state==TT_PAUSED?1:t->state==TT_RUNNING?0:3;
    const palette_t *p=&palettes[phase];
    if (v->palette!=phase) {
        v->palette=phase; v->alarm=alarm;
        tt_theme_bind(root,LV_STYLE_BG_COLOR,p->background,0);
        tt_theme_bind(root,LV_STYLE_BG_GRAD_COLOR,p->background,0);
        tt_theme_bind(root,LV_STYLE_TEXT_COLOR,p->ink,0);
        tt_theme_bind(v->ring,LV_STYLE_ARC_COLOR,p->track,LV_PART_MAIN);
        tt_theme_bind(v->ring,LV_STYLE_ARC_COLOR,p->accent,LV_PART_INDICATOR);
        tt_theme_bind(v->state,LV_STYLE_BG_COLOR,p->chip,0); tt_theme_bind(v->state,LV_STYLE_TEXT_COLOR,p->accent,0);
        tt_theme_bind(v->preset,LV_STYLE_TEXT_COLOR,p->accent,0);
        tt_theme_bind(v->percent,LV_STYLE_TEXT_COLOR,p->accent,0);
        tt_theme_bind(v->hint,LV_STYLE_TEXT_COLOR,p->accent,0);
        tt_theme_bind(v->hit,LV_STYLE_BORDER_COLOR,p->accent,LV_STATE_PRESSED);
        tt_theme_bind(v->hold_hint,LV_STYLE_TEXT_COLOR,p->accent,0);
        lv_obj_invalidate(root); lv_obj_invalidate(v->dial);
    }
    tt_face_t active=face>TT_HOME && face<TT_FRONT?face:t->active;
    if (active<=TT_HOME || active>=TT_FRONT) active=TT_RIGHT;
    bool idle=t->state==TT_IDLE; uint32_t duration=idle?s->timer.seconds[active]:t->duration_s;
    if (v->face!=active) { v->cancelled=true; v->face=active; }
    v->enabled=duration>0 || alarm;
    lv_obj_set_state(v->hit,LV_STATE_DISABLED,!v->enabled);
    unsigned seconds=idle?duration:(unsigned)((t->remaining_ms+999)/1000);
    int remaining=idle?(duration?1000:0):duration?(int)(t->remaining_ms/duration):0;
    int ring_value=alarm?1000:remaining;
    /* LVGL 9.5 maps values to whole degrees, but invalidates its knob even
     * when that degree is unchanged. Keep the exact same visible progress
     * without redrawing the entire weather scene for sub-degree updates. */
    if (lv_arc_get_angle_end(v->ring)!=lv_map(ring_value,0,1000,0,360))
        lv_arc_set_value(v->ring,ring_value);
    const lv_font_t *font=seconds>=6000?&tt_font_52:&tt_font_84;
    if (lv_obj_get_style_text_font(v->time,0)!=font) lv_obj_set_style_text_font(v->time,font,0);
    lv_obj_set_y(v->time,seconds>=6000?199:181);
    char value[100]; snprintf(value,sizeof(value),"%02u:%02u",seconds/60,seconds%60); tt_label_set_text(v->time,value);
#define T(text) tt_text(s->language,text)
    tt_label_set_text(v->title,T(alarm?"Time is up!":"Countdown"));
    static const char *states[]={"Stopped","Running","Paused","Finished"};
    tt_label_set_text(v->state,T(alarm?"Take a break":!duration?"This face is disabled":
        states[t->state]));
    static const char *faces[]={"Clock","Right","Upside down","Left","Flat","Screen down","Hold still"};
    snprintf(value,sizeof(value),"%s  ·  %02lu:%02lu",T(faces[active]),(unsigned long)(duration/60),(unsigned long)(duration%60)); tt_label_set_text(v->preset,value);
    if (alarm) tt_label_set_text(v->percent,T("Session complete"));
    else if (idle) tt_label_set_text(v->percent,T(duration?"Ready when you are":"Choose another face"));
    else { snprintf(value,sizeof(value),"%s  %d%%",T("Remaining"),(remaining+9)/10); tt_label_set_text(v->percent,value); }
    tt_label_set_text(v->hint,T(alarm?"Tap dial to dismiss":!duration?"Set a duration in Settings > Timers":
        idle?"Tap dial to start":t->state==TT_PAUSED?"Tap dial to resume":"Tap dial to pause"));
    tt_label_set_text(v->hold_hint,T(alarm?"Flip once to dismiss; flip again for a new timer":
        duration?"Hold 0.8s to stop and reset":"Choose another face"));
#undef T
}
