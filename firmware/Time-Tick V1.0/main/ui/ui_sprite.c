// SPDX-License-Identifier: GPL-3.0-only
#include "ui_sprite.h"
#include "ui_theme.h"
#include "ui_text.h"
#include "ui_daimao.h"
#include "ui_power.h"
#include <stdlib.h>
typedef struct {
    lv_obj_t *obj,*image,*heart;
    lv_timer_t *timer;
    tt_animation_t animation;
    uint32_t heart_started;
    bool heart_visible,environment;
    const lv_image_dsc_t *source;
    tt_weather_kind_t weather;
    tt_scene_t scene;
    lv_color_t light_from,light_to;
    lv_opa_t light_from_opa,light_to_opa;
} sprite_t;
static void light_blend(void *context,int32_t blend)
{
    sprite_t *p=context;
    lv_obj_set_style_image_recolor(p->image,lv_color_mix(p->light_to,p->light_from,(lv_opa_t)blend),0);
    lv_obj_set_style_image_recolor_opa(p->image,p->light_from_opa+
        ((int)p->light_to_opa-p->light_from_opa)*blend/255,0);
}
static void render(sprite_t *p)
{
    uint32_t now=lv_tick_get();
    tt_animation_clip_t clip=p->animation.clip;
    if (tt_animation_duration(clip) && !tt_animation_busy(&p->animation,now)) clip=TT_ANIM_IDLE;
    unsigned row=(unsigned)clip,frame=tt_animation_frame(&p->animation,now);
    if (clip==TT_ANIM_WALK && p->animation.left) row=8;
    if (clip==TT_ANIM_IDLE) {
        unsigned age=(now-p->animation.started)%14000;
        frame=age<3000?0:age<3120?1:age<3260?2:age<3380?1:age<6500?0:age<8000?3:age<11000?0:age<12000?4:5;
        if (p->environment && p->scene.daylight!=TT_NIGHT) {
            if (p->weather==TT_WEATHER_RAIN || p->weather==TT_WEATHER_STORM) { row=7; frame=(now/1100)%2; }
            else if (p->weather==TT_WEATHER_SNOW || p->scene.season==TT_WINTER || p->scene.season==TT_AUTUMN) { row=7; frame=2+(now/1400)%2; }
            else if (p->weather==TT_WEATHER_CLEAR && p->scene.season==TT_SUMMER) { row=7; frame=4+(now/700)%2; }
        }
    } else if (clip==TT_ANIM_SLEEP) {
        unsigned age=(now-p->animation.started)%18000;
        frame=age<4000?0:age<8000?1:age<11000?2:age<15000?3:age<16300?4:5;
    }
    const lv_image_dsc_t *source=tt_art_daimao[row][frame];
    if (source!=p->source) { p->source=source; lv_image_set_src(p->image,source); }
    int breath=lv_trigo_sin((int)(now%3600)/10)*2/LV_TRIGO_SIN_MAX;
    int dy=clip==TT_ANIM_WALK?(frame%3==1?-1:0):clip==TT_ANIM_IDLE || clip==TT_ANIM_SLEEP?-abs(breath):0;
    if (lv_obj_get_y(p->image)!=dy) lv_obj_set_y(p->image,dy);
}
static void tick(lv_timer_t *t)
{
    sprite_t *p=lv_timer_get_user_data(t);
    lv_timer_set_period(t,tt_visual_period(false));
    if (!lv_obj_is_visible(p->obj)) return;
    render(p);
    if (p->heart_visible) {
        uint32_t age=lv_tick_get()-p->heart_started;
        if (age>=2000) { p->heart_visible=false; lv_obj_add_flag(p->heart,LV_OBJ_FLAG_HIDDEN); }
        else lv_obj_set_y(p->heart,8-(int)(age/160));
    }
}
static void deleted(lv_event_t *e)
{
    sprite_t *p=lv_event_get_user_data(e); lv_anim_delete(p,light_blend); lv_timer_delete(p->timer); lv_free(p);
}
lv_obj_t *tt_sprite_create(lv_obj_t *parent,int x,int y,tt_pet_t type)
{
    (void)type;
    lv_obj_t *obj=lv_obj_create(parent); lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj,x,y); lv_obj_set_size(obj,108,104); lv_obj_remove_flag(obj,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(obj,LV_OBJ_FLAG_CLICKABLE);
    sprite_t *p=lv_malloc(sizeof(*p)); LV_ASSERT_MALLOC(p);
    *p=(sprite_t){.obj=obj}; lv_obj_set_user_data(obj,p);
    lv_obj_t *shadow=lv_obj_create(obj); lv_obj_remove_style_all(shadow);
    lv_obj_set_pos(shadow,26,91); lv_obj_set_size(shadow,58,8);
    lv_obj_remove_flag(shadow,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(shadow,lv_color_hex(0x364d32),0); lv_obj_set_style_bg_opa(shadow,LV_OPA_20,0);
    lv_obj_set_style_radius(shadow,LV_RADIUS_CIRCLE,0);
    p->image=lv_image_create(obj); lv_obj_set_pos(p->image,2,0); lv_obj_remove_flag(p->image,LV_OBJ_FLAG_CLICKABLE);
    p->heart=lv_label_create(obj); lv_label_set_text(p->heart,"♥");
    lv_obj_set_style_text_font(p->heart,&tt_font_18,0); lv_obj_set_style_text_color(p->heart,lv_color_hex(0xa94f53),0);
    lv_obj_set_pos(p->heart,90,8); lv_obj_add_flag(p->heart,LV_OBJ_FLAG_HIDDEN);
    p->timer=lv_timer_create(tick,50,p); lv_obj_add_event_cb(obj,deleted,LV_EVENT_DELETE,p); render(p); return obj;
}
void tt_sprite_set(lv_obj_t *obj,tt_pet_t type,tt_animation_clip_t clip,bool left)
{
    (void)type; sprite_t *p=lv_obj_get_user_data(obj);
    if (p->animation.clip==clip && p->animation.left==left && !(tt_animation_duration(clip) && !tt_sprite_busy(obj))) return;
    tt_animation_set(&p->animation,clip,left,lv_tick_get()); render(p);
}
void tt_sprite_walk(lv_obj_t *obj,uint32_t distance_q8)
{
    sprite_t *p=lv_obj_get_user_data(obj); tt_animation_walk(&p->animation,distance_q8); render(p);
}
void tt_sprite_heart(lv_obj_t *obj)
{
    sprite_t *p=lv_obj_get_user_data(obj); p->heart_started=lv_tick_get(); p->heart_visible=true;
    lv_obj_remove_flag(p->heart,LV_OBJ_FLAG_HIDDEN);
}
bool tt_sprite_busy(lv_obj_t *obj) { sprite_t *p=lv_obj_get_user_data(obj); return tt_animation_busy(&p->animation,lv_tick_get()); }
tt_animation_t tt_sprite_animation(lv_obj_t *obj) { sprite_t *p=lv_obj_get_user_data(obj); return p->animation; }
void tt_sprite_environment(lv_obj_t *obj,tt_weather_kind_t weather,tt_scene_t scene)
{
    sprite_t *p=lv_obj_get_user_data(obj);
    if (p->environment && p->weather==weather && p->scene.season==scene.season && p->scene.daylight==scene.daylight) return;
    bool lighting_changed=!p->environment || p->scene.daylight!=scene.daylight;
    if (lighting_changed) {
        lv_anim_delete(p,light_blend);
        p->light_from=lv_obj_get_style_image_recolor(p->image,0);
        p->light_from_opa=lv_obj_get_style_image_recolor_opa(p->image,0);
        p->light_to=lv_color_hex(scene.daylight==TT_NIGHT?0x0e203e:
            scene.daylight==TT_DUSK?0xc28070:0xf2d2ac);
        p->light_to_opa=scene.daylight==TT_NIGHT?150:scene.daylight==TT_DUSK?40:scene.daylight==TT_DAWN?24:0;
        if (!p->environment) light_blend(p,255);
        else {
            lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a,p);
            lv_anim_set_exec_cb(&a,light_blend); lv_anim_set_values(&a,0,255);
            lv_anim_set_duration(&a,650); lv_anim_set_path_cb(&a,lv_anim_path_ease_in_out); lv_anim_start(&a);
        }
    }
    p->environment=true; p->weather=weather; p->scene=scene; render(p);
}

