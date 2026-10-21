// SPDX-License-Identifier: GPL-3.0-only
#include "ui_garden.h"
#include "ui_weather_art.h"
#include "ui_text.h"
#include "ui_pet_weather.h"
#include "ui_atmosphere.h"
#include "ui_scene_background.h"
#include <math.h>
#include "ui_power.h"

typedef struct {
    lv_obj_t *obj,*pet,*bubble,*background,*incoming,*weather_detail,*atmosphere;
    lv_timer_t *timer; tt_garden_touch_t touch;
    tt_pet_t type; uint8_t language; int mood,height,activity,width,min_y,max_y;
    bool fullscreen,companion_visible; tt_scene_t scene;
    tt_weather_kind_t weather;
    uint32_t fade_started; bool fading;
    float x,y,tx,ty,shelter_x,shelter_y,speed; bool moving,left;
    uint32_t seed,last,deadline,message_until;
    tt_pet_state_t state;
    lv_obj_t *cached_background;
    lv_image_dsc_t scene_cache;
    bool cache_valid;
    uint32_t scene_changed;
} garden_t;
static void invalidate_background(garden_t *g)
{
    g->cache_valid=false; g->scene_changed=lv_tick_get();
    lv_obj_add_flag(g->cached_background,LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(g->background,LV_OBJ_FLAG_HIDDEN);
    tt_atmosphere_cached_tint(g->atmosphere,false);
}
static uint32_t random_next(garden_t *g) { g->seed=g->seed*1664525u+1013904223u; return g->seed; }
static void bubble(garden_t *g,const char *text)
{
    tt_label_set_text(g->bubble,tt_text(g->language,text));
    if (g->moving || lv_tick_get()>=g->message_until) lv_obj_add_flag(g->bubble,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(g->bubble,LV_OBJ_FLAG_HIDDEN);
}
static int bounded(int n,int lo,int hi) { return n<lo?lo:n>hi?hi:n; }
static void weather_detail_visibility(garden_t *g)
{
    if (!g->companion_visible || (g->scene.daylight==TT_NIGHT &&
        (g->weather==TT_WEATHER_CLEAR || g->weather==TT_WEATHER_CLOUDY))) lv_obj_add_flag(g->weather_detail,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(g->weather_detail,LV_OBJ_FLAG_HIDDEN);
}
static void weather_detail(garden_t *g)
{
    tt_sprite_environment(g->pet,g->weather,g->scene);
    tt_atmosphere_set(g->atmosphere,g->weather,g->scene);
    bool shelter=g->weather==TT_WEATHER_RAIN || g->weather==TT_WEATHER_STORM;
    tt_pet_weather_update(g->weather_detail,(int)lroundf(shelter?g->shelter_x:g->x),
                          (int)lroundf(shelter?g->shelter_y:g->y),g->weather,
                          tt_sprite_animation(g->pet).clip,g->moving);
    weather_detail_visibility(g);
}
static float walk_speed(const garden_t *g)
{
    switch (g->weather) {
    case TT_WEATHER_CLEAR: return 64.f;
    case TT_WEATHER_CLOUDY: return 52.f;
    case TT_WEATHER_RAIN: return 42.f;
    case TT_WEATHER_SNOW: return 38.f;
    case TT_WEATHER_FOG: return 32.f;
    case TT_WEATHER_STORM: return 36.f;
    default: return 64.f;
    }
}
void tt_garden_walk_to(lv_obj_t *obj,int x,int y)
{
    garden_t *g=lv_obj_get_user_data(obj);
    if (g->state.resting || tt_sprite_busy(g->pet)) return;
    g->tx=(float)bounded(x,12,g->width-120); g->ty=(float)bounded(y,g->min_y,g->max_y); g->moving=true; g->message_until=0; lv_obj_add_flag(g->bubble,LV_OBJ_FLAG_HIDDEN);
    if (g->tx!=g->x) g->left=g->tx<g->x;
}
static void choose_activity(garden_t *g,uint32_t now)
{
    bool shelter=g->weather==TT_WEATHER_RAIN || g->weather==TT_WEATHER_STORM;
    if (shelter && (fabsf(g->x-g->shelter_x)>1.f || fabsf(g->y-g->shelter_y)>1.f)) {
        tt_garden_walk_to(g->obj,(int)lroundf(g->shelter_x),(int)lroundf(g->shelter_y));
        return;
    }
    unsigned roll=random_next(g)%6;
    unsigned walks=g->scene.daylight==TT_NIGHT?1:g->weather==TT_WEATHER_CLEAR?4:shelter?0:g->weather==TT_WEATHER_SNOW?1:3;
    bool quiet=g->weather==TT_WEATHER_RAIN || g->weather==TT_WEATHER_SNOW ||
        g->weather==TT_WEATHER_STORM;
    if (roll<walks) {
        int x=24+(int)(random_next(g)%(unsigned)(g->width-152));
        int y=g->min_y+(int)(random_next(g)%(unsigned)(g->max_y-g->min_y+1));
        if (quiet || g->weather==TT_WEATHER_FOG) {
            int radius=g->weather==TT_WEATHER_FOG?70:42;
            x=bounded(x,(int)g->x-radius,(int)g->x+radius);
            y=bounded(y,(int)g->y-8,(int)g->y+8);
        }
        tt_garden_walk_to(g->obj,x,y); g->activity=0;
    } else {
        bool nap=g->scene.daylight==TT_NIGHT || g->weather==TT_WEATHER_SNOW || g->weather==TT_WEATHER_STORM ||
            (g->weather!=TT_WEATHER_RAIN && g->weather!=TT_WEATHER_FOG && roll==5 &&
             (g->weather!=TT_WEATHER_CLEAR || g->type==TT_DAIMAO));
        g->activity=nap?2:1;
        tt_sprite_set(g->pet,g->type,nap?TT_ANIM_SLEEP:TT_ANIM_IDLE,false);
        g->deadline=now+(quiet?5000:2800)+random_next(g)%2200;
    }
}
static void tick(lv_timer_t *timer)
{
    garden_t *g=lv_timer_get_user_data(timer); uint32_t now=lv_tick_get();
    unsigned elapsed=now-g->last; g->last=now;
    lv_timer_set_period(timer,tt_visual_period(false));
    if (!lv_obj_is_visible(g->obj)) return;
    if (!g->cache_valid && !g->fading && now-g->scene_changed>=850 && lv_obj_is_visible(g->obj)) {
        if (tt_scene_background(&g->scene_cache,tt_art_weather[g->weather],g->height,g->weather,g->scene)) {
            lv_image_set_src(g->cached_background,&g->scene_cache);
            lv_obj_align(g->cached_background,LV_ALIGN_BOTTOM_MID,0,0);
            lv_obj_remove_flag(g->cached_background,LV_OBJ_FLAG_HIDDEN);
            tt_atmosphere_cached_tint(g->atmosphere,true); g->cache_valid=true;
        } else g->scene_changed=now; /* Bound retries if PSRAM is temporarily busy. */
    }
    if (g->fading) {
        unsigned age=now-g->fade_started;
        if (age>=650) {
            lv_image_set_src(g->background,tt_art_weather[g->weather]);
            lv_obj_add_flag(g->incoming,LV_OBJ_FLAG_HIDDEN); g->fading=false;
        } else lv_obj_set_style_image_opa(g->incoming,(lv_opa_t)(age*255/650),0);
    }
    if (!lv_obj_is_visible(g->obj) || !g->companion_visible) return;
    if (g->state.resting) {
        g->moving=false; tt_sprite_set(g->pet,g->type,TT_ANIM_SLEEP,false);
        weather_detail(g); bubble(g,"Sweet dreams"); return;
    }
    if (tt_sprite_busy(g->pet)) { weather_detail(g); return; }
    tt_animation_t animation=tt_sprite_animation(g->pet);
    if (tt_animation_duration(animation.clip)) {
        tt_sprite_set(g->pet,g->type,TT_ANIM_IDLE,false); g->deadline=now+1500;
    }
    if (g->moving) {
        float dx=g->tx-g->x,dy=g->ty-g->y,distance=sqrtf(dx*dx+dy*dy);
        /* A fixed 48 px gait cycle follows travelled distance, including diagonals. */
        float dt=(elapsed>100?100:elapsed)/1000.f;
        float target=fminf(walk_speed(g)*(g->scene.daylight==TT_NIGHT?.65f:1.f),sqrtf(360.f*distance));
        float change=180.f*dt;
        g->speed+=fmaxf(-change,fminf(change,target-g->speed));
        float step=g->speed*dt;
        if (step>distance) step=distance;
        tt_sprite_set(g->pet,g->type,TT_ANIM_WALK,g->left);
        if (distance>0.001f) { g->x+=dx*step/distance; g->y+=dy*step/distance; }
        tt_sprite_walk(g->pet,(uint32_t)lroundf(step*256.f));
        lv_obj_set_pos(g->pet,(int)lroundf(g->x),(int)lroundf(g->y));
        if (distance<=step+0.001f) {
            g->x=g->tx; g->y=g->ty; g->moving=false; g->speed=0; g->deadline=now+1800+random_next(g)%2200;
            tt_sprite_set(g->pet,g->type,TT_ANIM_IDLE,false);
        }
    } else if (now>=g->deadline) choose_activity(g,now);
    weather_detail(g);
    if (now>=g->message_until) bubble(g,g->mood==2?"A well-earned rest":g->moving?"Exploring the garden":
        g->activity==1?"Such a lovely day":g->activity==2?"A little daydream":"Here with you");
}
static void pet_touch(lv_event_t *e)
{
    garden_t *g=lv_event_get_user_data(e); if (g->touch && !tt_sprite_busy(g->pet)) g->touch();
}
static void ground_touch(lv_event_t *e)
{
    garden_t *g=lv_event_get_user_data(e);
    lv_indev_t *input=lv_indev_active(); if (!input || g->state.resting || tt_sprite_busy(g->pet)) return;
    lv_point_t p; lv_indev_get_point(input,&p); lv_area_t a; lv_obj_get_coords(g->obj,&a);
    tt_garden_walk_to(g->obj,p.x-a.x1-54,p.y-a.y1-80);
}
static void deleted(lv_event_t *e)
{
    garden_t *g=lv_event_get_user_data(e); lv_timer_delete(g->timer); tt_scene_background_free(&g->scene_cache); lv_free(g);
}
static lv_obj_t *create(lv_obj_t *parent,int x,int y,int height,tt_pet_t type,uint8_t lang,tt_garden_touch_t touch,bool full)
{
    lv_obj_t *obj=lv_obj_create(parent); lv_obj_remove_style_all(obj);
    int width=full?480:448;
    lv_obj_set_pos(obj,x,y); lv_obj_set_size(obj,width,height); lv_obj_remove_flag(obj,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj,full?0:16,0); lv_obj_set_style_clip_corner(obj,true,0);
    garden_t *g=lv_malloc(sizeof(*g)); LV_ASSERT_MALLOC(g);
    *g=(garden_t){.obj=obj,.type=type,.language=lang,.height=height,.width=width,.fullscreen=full,
        .companion_visible=true,.scene={TT_SPRING,TT_DAY},
        .min_y=full?260:54,.max_y=full?294:height-106,.x=160,.y=full?280:height-112,.touch=touch,
        .seed=0x7351u+lv_tick_get(),.last=lv_tick_get(),.deadline=lv_tick_get()+1400,
        .state={.food=700,.hygiene=800}};
    lv_obj_t *bg=lv_image_create(obj); g->background=bg; lv_image_set_src(bg,tt_art_weather[TT_WEATHER_UNKNOWN]);
    lv_obj_align(bg,LV_ALIGN_BOTTOM_MID,0,0); lv_obj_remove_flag(bg,LV_OBJ_FLAG_CLICKABLE);
    g->incoming=lv_image_create(obj); lv_obj_align(g->incoming,LV_ALIGN_BOTTOM_MID,0,0);
    lv_obj_remove_flag(g->incoming,LV_OBJ_FLAG_CLICKABLE); lv_obj_add_flag(g->incoming,LV_OBJ_FLAG_HIDDEN);
    g->cached_background=lv_image_create(obj);
    lv_obj_remove_flag(g->cached_background,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g->cached_background,LV_OBJ_FLAG_HIDDEN);
    g->atmosphere=tt_atmosphere_create(obj,width,height);
    g->weather_detail=tt_pet_weather_create(obj);
    g->pet=tt_sprite_create(obj,(int)g->x,(int)g->y,type); lv_obj_add_event_cb(g->pet,pet_touch,LV_EVENT_CLICKED,g);
    weather_detail(g);
    g->bubble=lv_label_create(obj); lv_obj_set_style_text_font(g->bubble,&tt_font_16,0);
    lv_obj_set_width(g->bubble,LV_SIZE_CONTENT); lv_obj_align(g->bubble,LV_ALIGN_TOP_MID,0,g->fullscreen?234:10);
    tt_theme_bind(g->bubble,LV_STYLE_TEXT_COLOR,TT_INK,0);
    tt_theme_bind(g->bubble,LV_STYLE_BG_COLOR,TT_CREAM,0); lv_obj_set_style_bg_opa(g->bubble,224,0);
    lv_obj_set_style_radius(g->bubble,12,0); lv_obj_set_style_pad_hor(g->bubble,12,0); lv_obj_set_style_pad_ver(g->bubble,5,0);
    bubble(g,"A little garden, a little joy");
    lv_obj_set_user_data(obj,g); lv_obj_add_event_cb(obj,ground_touch,LV_EVENT_CLICKED,g);
    lv_obj_add_event_cb(obj,deleted,LV_EVENT_DELETE,g); g->timer=lv_timer_create(tick,50,g); return obj;
}
lv_obj_t *tt_garden_create(lv_obj_t *parent,int x,int y,int height,tt_pet_t type,uint8_t lang,tt_garden_touch_t touch)
{ return create(parent,x,y,height,type,lang,touch,false); }
lv_obj_t *tt_garden_fullscreen(lv_obj_t *parent,tt_pet_t type,uint8_t lang,tt_garden_touch_t touch)
{ return create(parent,0,0,480,type,lang,touch,true); }
void tt_garden_weather(lv_obj_t *obj,tt_weather_kind_t weather)
{
    garden_t *g=lv_obj_get_user_data(obj);
    if ((unsigned)weather>=TT_WEATHER_KINDS) weather=TT_WEATHER_UNKNOWN;
    if (g->weather==weather) return;
    if (g->fading) return; /* Complete this blend, then take the latest requested weather. */
    invalidate_background(g);
    g->weather=weather;
    if (weather==TT_WEATHER_RAIN || weather==TT_WEATHER_STORM) {
        g->shelter_x=g->moving?g->tx:g->x; g->shelter_y=g->moving?g->ty:g->y;
    }
    /* Weather never cancels a touch-directed walk, a care action or explicit rest. */
    if (!g->moving && !tt_sprite_busy(g->pet) && lv_tick_get()>=g->message_until)
        g->deadline=lv_tick_get()+650;
    lv_image_set_src(g->incoming,tt_art_weather[weather]);
    lv_obj_align(g->incoming,LV_ALIGN_BOTTOM_MID,0,0);
    if (!lv_obj_is_visible(obj)) {
        lv_image_set_src(g->background,tt_art_weather[weather]); g->fading=false;
        lv_obj_add_flag(g->incoming,LV_OBJ_FLAG_HIDDEN);
    } else {
        g->fade_started=lv_tick_get(); g->fading=true;
        lv_obj_set_style_image_opa(g->incoming,0,0); lv_obj_remove_flag(g->incoming,LV_OBJ_FLAG_HIDDEN);
    }
}
void tt_garden_set(lv_obj_t *obj,tt_pet_t pet,int mood,uint8_t lang,const tt_pet_state_t *state)
{
    garden_t *g=lv_obj_get_user_data(obj);
    if (g->type!=pet || (g->state.resting && !state->resting)) {
        g->moving=false; g->activity=0; g->deadline=lv_tick_get()+500;
        tt_sprite_set(g->pet,pet,TT_ANIM_IDLE,false);
    }
    g->type=pet; g->mood=mood; g->language=lang; g->state=*state;
}
void tt_garden_react(lv_obj_t *obj,tt_pet_action_t action,const char *message)
{
    garden_t *g=lv_obj_get_user_data(obj);
    static const tt_animation_clip_t clips[]={TT_ANIM_EAT,TT_ANIM_PLAY,TT_ANIM_WASH,TT_ANIM_SLEEP,TT_ANIM_HAPPY};
    tt_animation_clip_t clip=clips[action<TT_PET_ACTIONS?action:0];
    if (action==TT_PET_REST && !g->state.resting) clip=TT_ANIM_HAPPY;
    g->moving=false; g->activity=0; g->deadline=g->message_until=lv_tick_get()+3000;
    tt_sprite_set(g->pet,g->type,clip,false); tt_sprite_heart(g->pet); bubble(g,message);
}
void tt_garden_position(lv_obj_t *obj,int *x,int *y)
{
    garden_t *g=lv_obj_get_user_data(obj); *x=(int)lroundf(g->x); *y=(int)lroundf(g->y);
}
void tt_garden_say(lv_obj_t *obj,const char *message)
{
    garden_t *g=lv_obj_get_user_data(obj); g->message_until=lv_tick_get()+3000; bubble(g,message);
}
bool tt_garden_busy(lv_obj_t *obj) { garden_t *g=lv_obj_get_user_data(obj); return tt_sprite_busy(g->pet); }
tt_animation_t tt_garden_animation(lv_obj_t *obj) { garden_t *g=lv_obj_get_user_data(obj); return tt_sprite_animation(g->pet); }
void tt_garden_environment(lv_obj_t *obj,tt_scene_t scene)
{
    garden_t *g=lv_obj_get_user_data(obj);
    if (g->scene.daylight!=scene.daylight || g->scene.season!=scene.season) invalidate_background(g);
    if (g->scene.daylight!=scene.daylight && !g->moving && !tt_sprite_busy(g->pet)) g->deadline=lv_tick_get()+650;
    g->scene=scene; tt_atmosphere_set(g->atmosphere,g->weather,scene);
    tt_sprite_environment(g->pet,g->weather,scene);
}
void tt_garden_companion_visible(lv_obj_t *obj,bool visible)
{
    garden_t *g=lv_obj_get_user_data(obj); g->companion_visible=visible;
    if (visible) lv_obj_remove_flag(g->pet,LV_OBJ_FLAG_HIDDEN);
    else { lv_obj_add_flag(g->pet,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(g->bubble,LV_OBJ_FLAG_HIDDEN); }
    /* Snapshots and animation ticks must agree: re-showing a night-hidden
     * decoration here caused a full redraw on every model update. */
    weather_detail_visibility(g);
}
