// SPDX-License-Identifier: GPL-3.0-only
#include "ui_pet.h"
#include "ui_text.h"
#include <stdio.h>
static lv_obj_t *screen,*garden,*care,*journal,*heading,*summary,*bond,*xp,*message;
static lv_obj_t *stats[3],*actions[4],*journal_button,*journal_text,*badges[6],*badge_bars[6],*badge_cards[6],*badge_symbols[6];
static uint8_t language;
static tt_pet_t species;
static tt_pet_ui_action_t on_action;
static bool showing_journal;
static uint32_t message_until;
#define T(s) tt_text(language,s)
static lv_obj_t *label(lv_obj_t *parent,const char *text,int x,int y,int w)
{
    lv_obj_t *o=lv_label_create(parent); lv_label_set_text(o,text); lv_obj_set_pos(o,x,y); lv_obj_set_width(o,w);
    lv_label_set_long_mode(o,LV_LABEL_LONG_DOT); return o;
}
static lv_obj_t *plain(lv_obj_t *parent,int x,int y,int w,int h)
{
    lv_obj_t *o=lv_obj_create(parent); lv_obj_remove_style_all(o); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE); return o;
}
static lv_obj_t *button(lv_obj_t *parent,const char *text,int x,int y,int w,lv_event_cb_t cb,intptr_t id)
{
    lv_obj_t *o=lv_button_create(parent); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,42);
    lv_obj_set_style_radius(o,12,0); tt_theme_bind(o,LV_STYLE_BG_COLOR,0xe5ecd7,0);
    tt_theme_bind(o,LV_STYLE_TEXT_COLOR,TT_INK,0); lv_obj_set_style_shadow_width(o,0,0);
    lv_obj_set_style_pad_all(o,4,0); lv_obj_set_style_border_width(o,1,0); tt_theme_bind(o,LV_STYLE_BORDER_COLOR,0xc8d3b8,0);
    tt_theme_bind(o,LV_STYLE_BG_COLOR,0xc8ddaf,LV_STATE_PRESSED);
    lv_obj_set_style_opa(o,LV_OPA_50,LV_STATE_DISABLED);
    lv_obj_t *l=label(o,text,0,0,w-8); lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0); lv_obj_center(l);
    lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,(void *)id); return o;
}
static void action(lv_event_t *e) { on_action((tt_pet_action_t)(intptr_t)lv_event_get_user_data(e)); }
static void cuddle(void) { on_action(TT_PET_CUDDLE); }
static void journal_toggle(lv_event_t *e)
{
    (void)e; showing_journal=!showing_journal;
    if (showing_journal) { lv_obj_add_flag(care,LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(journal,LV_OBJ_FLAG_HIDDEN); }
    else { lv_obj_add_flag(journal,LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(care,LV_OBJ_FLAG_HIDDEN); }
    lv_label_set_text(lv_obj_get_child(journal_button,0),T(showing_journal?"Care":"Growth journal"));
    lv_obj_center(lv_obj_get_child(journal_button,0));
}
void tt_pet_ui_open(tt_pet_t type,uint8_t lang,lv_event_cb_t back,tt_pet_ui_action_t callback)
{
    if (screen) lv_obj_delete(screen);
    language=lang; species=type; on_action=callback; showing_journal=false; message_until=0;
    screen=lv_obj_create(NULL); lv_obj_set_style_pad_all(screen,0,0); lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(screen,&tt_font_16,0); tt_theme_bind(screen,LV_STYLE_TEXT_COLOR,TT_INK,0);
    tt_theme_backdrop(screen);
    heading=label(screen,T("Pet care"),20,12,280); lv_obj_set_style_text_font(heading,&tt_font_24,0);
    summary=label(screen,"",20,42,440);
    care=plain(screen,0,68,480,350);
    garden=tt_garden_create(care,16,0,190,type,lang,cuddle);
    static const char *names[]={"Focus sessions","Focus minutes","Friendship"};
    for (int i=0;i<3;++i) {
        lv_obj_t *card=plain(care,16+i*152,198,144,57);
        tt_theme_bind(card,LV_STYLE_BG_COLOR,TT_CREAM,0); lv_obj_set_style_bg_opa(card,255,0);
        lv_obj_set_style_radius(card,10,0); label(card,T(names[i]),10,5,124);
        stats[i]=label(card,"",10,29,124); tt_theme_bind(stats[i],LV_STYLE_TEXT_COLOR,TT_GREEN,0);
    }
    for (int i=0;i<4;++i)
        actions[i]=button(care,tt_text(language,(const char *[]){"Feed","Play","Wash","Sleep"}[i]),16+i*114,302,106,action,i);
    bond=label(care,"",20,260,440);
    xp=lv_bar_create(care); lv_obj_set_pos(xp,20,286); lv_obj_set_size(xp,440,5);
    lv_bar_set_range(xp,0,100); tt_theme_bind(xp,LV_STYLE_BG_COLOR,0xe4e5d2,0);
    tt_theme_bind(xp,LV_STYLE_BG_COLOR,TT_GREEN,LV_PART_INDICATOR);
    journal=plain(screen,16,68,448,344); lv_obj_add_flag(journal,LV_OBJ_FLAG_HIDDEN);
    tt_theme_bind(journal,LV_STYLE_BG_COLOR,TT_CREAM,0); lv_obj_set_style_bg_opa(journal,LV_OPA_COVER,0);
    lv_obj_set_style_radius(journal,16,0);
    lv_obj_t *jt=label(journal,language==TT_ZH?"专注足迹":"Focus milestones",16,12,416);
    lv_obj_set_style_text_font(jt,&tt_font_24,0);
    journal_text=label(journal,"",16,42,416);
    const char *cn[]={"初见专注","扎根时刻","一小时的光","步履不停","小院成林","专注旅人"};
    const char *en[]={"First focus","Deep roots","One good hour","Little steps","Growing grove","Explorer"};
    const char *requirements_cn[]={"完成首段专注","单次 25 分钟","累计 60 分钟","完成 10 段","累计 5 小时","完成 50 段"};
    const char *requirements_en[]={"First session","25 min session","60 min total","10 sessions","5 hours total","50 sessions"};
    const char *symbols[]={LV_SYMBOL_OK,LV_SYMBOL_UP,LV_SYMBOL_REFRESH,LV_SYMBOL_RIGHT,LV_SYMBOL_HOME,LV_SYMBOL_GPS};
    for (int i=0;i<TT_FOCUS_BADGES;++i) {
        int x=16+(i%2)*214,y=76+(i/2)*78;
        badge_cards[i]=plain(journal,x,y,202,70);
        lv_obj_set_style_radius(badge_cards[i],12,0); lv_obj_set_style_bg_opa(badge_cards[i],255,0);
        lv_obj_t *seal=plain(badge_cards[i],9,11,34,34); lv_obj_set_style_radius(seal,17,0);
        tt_theme_bind(seal,LV_STYLE_BG_COLOR,0xe2d2ad,0); lv_obj_set_style_bg_opa(seal,255,0);
        badge_symbols[i]=label(seal,symbols[i],0,5,34); lv_obj_set_style_text_align(badge_symbols[i],LV_TEXT_ALIGN_CENTER,0);
        badges[i]=label(badge_cards[i],language==TT_ZH?cn[i]:en[i],51,9,144);
        lv_obj_t *req=label(badge_cards[i],language==TT_ZH?requirements_cn[i]:requirements_en[i],51,32,144);
        tt_theme_bind(req,LV_STYLE_TEXT_COLOR,TT_MUTED,0);
        badge_bars[i]=lv_bar_create(badge_cards[i]); lv_obj_set_pos(badge_bars[i],12,58); lv_obj_set_size(badge_bars[i],178,4);
        tt_theme_bind(badge_bars[i],LV_STYLE_BG_COLOR,0xdce3d2,0);
        tt_theme_bind(badge_bars[i],LV_STYLE_BG_COLOR,TT_GREEN,LV_PART_INDICATOR);
    }
    lv_obj_t *note=label(journal,T("Keep every milestone. No streaks to lose."),16,316,416);
    lv_label_set_long_mode(note,LV_LABEL_LONG_WRAP);
    message=label(screen,T("Just for fun; flip anytime to start a timer"),20,412,440);
    lv_obj_set_style_text_align(message,LV_TEXT_ALIGN_CENTER,0);
    button(screen,T("Back"),16,434,140,back,0);
    journal_button=button(screen,T("Growth journal"),168,434,296,journal_toggle,0);
    lv_screen_load(screen);
}
void tt_pet_ui_update(const tt_pet_state_t *p,const tt_focus_progress_t *f,bool pending)
{
    const char *name=tt_text(language,"Dai Meow");
    lv_label_set_text_fmt(summary,"%s  /  Lv.%u  /  %s",name,tt_pet_level(p),T(tt_pet_stage(p)));
    lv_label_set_text_fmt(stats[0],"%lu",(unsigned long)f->sessions);
    lv_label_set_text_fmt(stats[1],"%lu %s",(unsigned long)(f->seconds/60),T("min"));
    lv_label_set_text_fmt(stats[2],"%u%%",p->bond/10);
    for (int i=0;i<4;++i) {
        if (pending || tt_garden_busy(garden)) lv_obj_add_state(actions[i],LV_STATE_DISABLED); else lv_obj_remove_state(actions[i],LV_STATE_DISABLED);
    }
    lv_label_set_text(lv_obj_get_child(actions[3],0),T(p->resting?"Wake up":"Sleep")); lv_obj_center(lv_obj_get_child(actions[3],0));
    lv_label_set_text_fmt(bond,"%s %u%%     %s %lu / %s",T("Bond"),p->bond/10,T("Growth"),
                         (unsigned long)(p->xp>=900?100:p->xp%100),p->xp>=900?"MAX":"100");
    lv_bar_set_value(xp,p->xp>=900?100:(int)(p->xp%100),LV_ANIM_OFF);
    tt_garden_set(garden,species,0,language,p);
    lv_label_set_text_fmt(journal_text,"%s: %lu / %lu %s",T("Focus"),(unsigned long)f->sessions,(unsigned long)(f->seconds/60),T("min"));
    for (int i=0;i<TT_FOCUS_BADGES;++i) {
        bool unlocked=(f->badges&(1u<<i))!=0;
        tt_theme_bind(badges[i],LV_STYLE_TEXT_COLOR,unlocked?TT_GREEN:TT_INK,0);
        tt_theme_bind(badge_cards[i],LV_STYLE_BG_COLOR,unlocked?0xe2ecd1:0xf0efe4,0);
        lv_bar_set_value(badge_bars[i],tt_focus_badge_progress(f,i),LV_ANIM_OFF);
    }
    if (lv_tick_get()>=message_until) lv_label_set_text(message,showing_journal?"":T(pending?"Saving...":"Just for fun; flip anytime to start a timer"));
}
void tt_pet_ui_feedback(tt_pet_result_t result,tt_pet_action_t action,const tt_pet_state_t *p)
{
    const char *text=tt_pet_result_text(result,action,p->resting);
    lv_label_set_text(message,T(text)); message_until=lv_tick_get()+4000;
    tt_theme_bind(message,LV_STYLE_TEXT_COLOR,result==TT_PET_SAVE_FAILED?0xa94449:TT_GREEN,0);
    if (result==TT_PET_OK) tt_garden_react(garden,action,text);
}
lv_obj_t *tt_pet_ui_garden(void) { return garden; }
