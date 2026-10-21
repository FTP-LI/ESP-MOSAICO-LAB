// SPDX-License-Identifier: GPL-3.0-only
#include "ui_atmosphere.h"
#include "ui_power.h"
typedef struct { lv_obj_t *obj; lv_timer_t *timer; tt_weather_kind_t weather; tt_scene_t scene,previous_scene; tt_weather_kind_t previous_weather; uint32_t changed; bool cached_tint; } air_t;
typedef struct { lv_layer_t *layer; unsigned opacity; } air_layer_t;
static int sine(uint32_t t,unsigned period,int amplitude)
{ return lv_trigo_sin((int)(t%period)*360/(int)period)*amplitude/LV_TRIGO_SIN_MAX; }
static void rect(air_layer_t *l,int x,int y,int w,int h,int r,uint32_t color,int opacity)
{
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d); d.bg_color=lv_color_hex(color);
    d.bg_opa=(lv_opa_t)(opacity*l->opacity/255); d.radius=r; lv_area_t a={x,y,x+w-1,y+h-1}; lv_draw_rect(l->layer,&d,&a);
}
static void line(air_layer_t *l,int x,int y,int x2,int y2,int width,uint32_t color,int opacity)
{
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d); d.p1=(lv_point_precise_t){x,y}; d.p2=(lv_point_precise_t){x2,y2};
    d.width=width; d.color=lv_color_hex(color); d.opa=(lv_opa_t)(opacity*l->opacity/255); d.round_start=d.round_end=1; lv_draw_line(l->layer,&d);
}
static void gradient(air_layer_t *l,lv_area_t a,uint32_t color,int top,int bottom)
{
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d); d.bg_opa=255; d.bg_color=lv_color_hex(color);
    d.bg_grad.dir=LV_GRAD_DIR_VER; d.bg_grad.stops_count=2;
    d.bg_grad.stops[0]=(lv_grad_stop_t){.color=lv_color_hex(color),.opa=(lv_opa_t)(top*l->opacity/255),.frac=0};
    d.bg_grad.stops[1]=(lv_grad_stop_t){.color=lv_color_hex(color),.opa=(lv_opa_t)(bottom*l->opacity/255),.frac=255};
    lv_draw_rect(l->layer,&d,&a);
}
static void draw_scene(const air_t *p,air_layer_t *l,lv_area_t a)
{
    uint32_t t=lv_tick_get();
    int w=lv_area_get_width(&a),h=lv_area_get_height(&a),x=a.x1,y=a.y1;
    if (!p->cached_tint) {
        if (p->scene.season==TT_AUTUMN) gradient(l,a,0xbd733c,12,40);
        if (p->scene.season==TT_WINTER && p->weather!=TT_WEATHER_SNOW) gradient(l,a,0xdce9f4,18,35);
        if (p->scene.daylight==TT_NIGHT) gradient(l,a,0x091630,205,158);
        else if (p->scene.daylight==TT_DUSK) gradient(l,a,0xd57863,98,22);
        else if (p->scene.daylight==TT_DAWN) gradient(l,a,0xffd6ad,75,12);
    }
    if (p->scene.daylight==TT_NIGHT && (p->weather==TT_WEATHER_CLEAR || p->weather==TT_WEATHER_CLOUDY)) {
        for (int i=0;i<22;++i) {
            int sx=(i*97+31)%w,sy=(i*53+11)%(h/2+1);
            rect(l,x+sx,y+sy,2,2,1,0xffefc5,110+sine(t+(unsigned)i*310,4200,70));
        }
        rect(l,x+w-125,y+88,29,29,15,0xffedc3,220);
        rect(l,x+w-116,y+83,27,27,14,0x223350,255);
    }
    if (p->weather==TT_WEATHER_RAIN || p->weather==TT_WEATHER_STORM) {
        for (int i=0;i<42;++i) {
            int px=(i*89+(int)(t/38))%w,py=(i*59+(int)(t/(i%3+3)))%(h+32)-32;
            line(l,x+px,y+py,x+px-5,y+py+14+i%9,1,0xe0eff7,60+i%4*18);
        }
        for (int i=0;i<6;++i) {
            unsigned age=(t+(unsigned)i*283)%1700; int r=(int)(age/170)+2;
            if (age<1100) {
                lv_draw_arc_dsc_t d; lv_draw_arc_dsc_init(&d); d.center=(lv_point_t){x+(i*71+44)%w,y+h-24-i%3*17};
                d.radius=(uint16_t)r; d.width=1; d.start_angle=10; d.end_angle=170; d.color=lv_color_hex(0xe2eef0);
                d.opa=(lv_opa_t)((1100-age)*l->opacity/12/255); lv_draw_arc(l->layer,&d);
            }
        }
        /* A restrained lightning glow, never rapid flashing. */
        if (p->weather==TT_WEATHER_STORM && t%14000<180)
            rect(l,x,y,w,h,0,0xe4edff,(int)(180-t%14000)/8);
    } else if (p->weather==TT_WEATHER_SNOW) {
        for (int i=0;i<34;++i) {
            int px=(i*83+sine(t+i*331,6500,14)+w)%w,py=(i*61+(int)(t/(22+i%4*7)))%(h+8)-8;
            int r=2+i%3; rect(l,x+px,y+py,r,r,r,0xfffcf3,115+i%4*30);
        }
    } else if (p->weather==TT_WEATHER_FOG) {
        for (int i=0;i<4;++i) {
            int cy=h/3+i*h/6+sine(t+i*700,14000,10);
            lv_area_t band={x,y+cy-24,x+w-1,y+cy+24};
            gradient(l,band,0xe8efe9,0,32);
            band.y1+=48; band.y2+=48; gradient(l,band,0xe8efe9,32,0);
        }
    } else {
        /* Slow drifting petals/leaves; fewer and dimmer after sunset. */
        for (int i=0;i<7;++i) {
            int px=(i*109+(int)(t/(p->scene.season==TT_AUTUMN?45:80)))%w;
            int py=(i*67+(int)(t/95)+sine(t+i*220,4300,8))%h;
            uint32_t color=p->scene.season==TT_SPRING?0xf4c9d1:p->scene.season==TT_AUTUMN?0xd0a052:0xffecc1;
            rect(l,x+px,y+py,p->scene.season==TT_AUTUMN?7:3,3,2,color,p->scene.daylight==TT_NIGHT?70:120);
        }
    }
}
static void draw(lv_event_t *e)
{
    air_t *p=lv_event_get_user_data(e); lv_area_t a; lv_obj_get_coords(p->obj,&a);
    unsigned age=lv_tick_get()-p->changed, blend=age>=800?255:age*255/800;
    air_layer_t layer={.layer=lv_event_get_layer(e),.opacity=255-blend};
    if (blend<255) {
        air_t prior=*p; prior.weather=p->previous_weather; prior.scene=p->previous_scene;
        draw_scene(&prior,&layer,a);
    }
    layer.opacity=blend; if (blend) draw_scene(p,&layer,a);
}
static void tick(lv_timer_t *timer) { air_t *p=lv_timer_get_user_data(timer); lv_timer_set_period(timer,tt_visual_period(true)); if (lv_obj_is_visible(p->obj)) lv_obj_invalidate(p->obj); }
static void deleted(lv_event_t *e) { air_t *p=lv_event_get_user_data(e); lv_timer_delete(p->timer); lv_free(p); }
lv_obj_t *tt_atmosphere_create(lv_obj_t *parent,int width,int height)
{
    lv_obj_t *o=lv_obj_create(parent); lv_obj_remove_style_all(o); lv_obj_set_size(o,width,height);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    air_t *p=lv_malloc(sizeof(*p)); LV_ASSERT_MALLOC(p); *p=(air_t){.obj=o,.scene={TT_SPRING,TT_DAY}};
    lv_obj_set_user_data(o,p); lv_obj_add_event_cb(o,draw,LV_EVENT_DRAW_MAIN,p);
    lv_obj_add_event_cb(o,deleted,LV_EVENT_DELETE,p); p->timer=lv_timer_create(tick,50,p); return o;
}
void tt_atmosphere_set(lv_obj_t *o,tt_weather_kind_t weather,tt_scene_t scene)
{
    air_t *p=lv_obj_get_user_data(o);
    if (p->weather!=weather || p->scene.season!=scene.season || p->scene.daylight!=scene.daylight) {
        p->previous_weather=p->weather; p->previous_scene=p->scene; p->changed=lv_tick_get();
        p->weather=weather; p->scene=scene;
    }
}
void tt_atmosphere_cached_tint(lv_obj_t *o,bool cached)
{
    air_t *p=lv_obj_get_user_data(o);
    if (p->cached_tint==cached) return;
    p->cached_tint=cached; lv_obj_invalidate(o);
}
