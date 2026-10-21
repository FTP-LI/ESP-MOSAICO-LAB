// SPDX-License-Identifier: GPL-3.0-only
#include "ui_pet_weather.h"
#include "ui_power.h"


typedef struct {
    tt_weather_kind_t previous,weather;
    tt_animation_clip_t clip;
    uint32_t changed,painted;
    bool moving;
} weather_pet_t;

static void shape(lv_layer_t *l,int x,int y,int w,int h,int radius,uint32_t color,lv_opa_t opacity)
{
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color=lv_color_hex(color); d.bg_opa=opacity; d.radius=radius;
    lv_area_t a={x,y,x+w-1,y+h-1}; lv_draw_rect(l,&d,&a);
}
static void line(lv_layer_t *l,int x,int y,int x2,int y2,int width,uint32_t color,lv_opa_t opacity)
{
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d);
    d.p1=(lv_point_precise_t){x,y}; d.p2=(lv_point_precise_t){x2,y2};
    d.width=width; d.color=lv_color_hex(color); d.opa=opacity;
    d.round_start=d.round_end=1; lv_draw_line(l,&d);
}
static int wave(uint32_t now,unsigned period,int amplitude)
{
    return lv_trigo_sin((int)(now%period)*360/(int)period)*amplitude/LV_TRIGO_SIN_MAX;
}
static void scene(lv_layer_t *l,int x,int y,const weather_pet_t *p,tt_weather_kind_t weather,lv_opa_t opacity)
{
    uint32_t now=lv_tick_get();
    if (!opacity) return;
    if (weather==TT_WEATHER_CLEAR) {
        /* A tiny butterfly flutters above the ears, never over the face. */
        int bx=x+66+wave(now,5200,32),by=y+12+wave(now,3100,5),wing=5+wave(now,360,3);
        shape(l,bx-wing,by-3,wing,9,4,0xf7d279,opacity);
        shape(l,bx+2,by-3,wing,9,4,0xffe6a7,opacity);
        line(l,bx+1,by,bx+1,by+6,2,0x806342,opacity);
    } else if (weather==TT_WEATHER_RAIN || weather==TT_WEATHER_STORM) {
        /* A garden parasol: shaft beside the body, canopy above the ears. */
        /* Dai Meow wears a pose-integrated rain cape in idle frames. */
    } else if (weather==TT_WEATHER_SNOW) {

        /* Soft breath puffs instead of a costume that could intersect limbs. */
        unsigned age=now%2400;
        if (age<1400 && !p->moving) {
            int rise=(int)(age/160);
            lv_opa_t mist=(lv_opa_t)(opacity*(1400-age)/2100);
            shape(l,x+109,y+94-rise,6+rise,4+rise/2,5,0xfaf7e8,mist);
        }
    } else if (weather==TT_WEATHER_CLOUDY) {
        int dx=wave(now,4600,6);
        lv_opa_t breeze=(lv_opa_t)(opacity*(110+wave(now,4600,60))/255);
        line(l,x+80+dx,y+11,x+106+dx,y+11,1,0xf6f2dc,breeze);
        line(l,x+89+dx,y+16,x+115+dx,y+16,1,0xe5eedf,breeze);
    }
}
static void draw(lv_event_t *e)
{
    weather_pet_t *p=lv_event_get_user_data(e);
    /* Hand-fed meals, play and bath props keep the foreground to themselves. */
    if (tt_animation_duration(p->clip)) return;
    lv_area_t a; lv_obj_get_coords(lv_event_get_target_obj(e),&a);
    lv_layer_t *l=lv_event_get_layer(e);
    unsigned age=lv_tick_get()-p->changed;
    unsigned blend=age>=650?255:age*255/650;
    if (blend<255) scene(l,a.x1,a.y1,p,p->previous,(lv_opa_t)(255-blend));
    scene(l,a.x1,a.y1,p,p->weather,(lv_opa_t)blend);
}
static void deleted(lv_event_t *e) { lv_free(lv_event_get_user_data(e)); }
lv_obj_t *tt_pet_weather_create(lv_obj_t *parent)
{
    lv_obj_t *obj=lv_obj_create(parent); lv_obj_remove_style_all(obj); lv_obj_set_size(obj,132,130);
    lv_obj_remove_flag(obj,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    weather_pet_t *p=lv_malloc(sizeof(*p)); LV_ASSERT_MALLOC(p); *p=(weather_pet_t){0};
    lv_obj_set_user_data(obj,p);
    lv_obj_add_event_cb(obj,draw,LV_EVENT_DRAW_MAIN,p); lv_obj_add_event_cb(obj,deleted,LV_EVENT_DELETE,p);
    return obj;
}
void tt_pet_weather_update(lv_obj_t *obj,int x,int y,tt_weather_kind_t weather,tt_animation_clip_t clip,bool moving)
{
    weather_pet_t *p=lv_obj_get_user_data(obj);
    bool changed=weather!=p->weather || clip!=p->clip || moving!=p->moving;
    if (weather!=p->weather) { p->previous=p->weather; p->weather=weather; p->changed=lv_tick_get(); }
    p->clip=clip; p->moving=moving;
    lv_obj_set_pos(obj,x-12,y-24);
    /* Rain/fog draw no prop, and care actions own the foreground. Avoid
     * turning an empty transparent object into a full-screen refresh. */
    uint32_t now=lv_tick_get();
    bool visible_effect=!tt_animation_duration(clip) &&
        (weather==TT_WEATHER_CLEAR || weather==TT_WEATHER_CLOUDY ||
         (weather==TT_WEATHER_SNOW && !moving) || now-p->changed<650);
    if (changed || (visible_effect && now-p->painted>=tt_visual_period(true))) {
        p->painted=now; lv_obj_invalidate(obj);
    }
}
