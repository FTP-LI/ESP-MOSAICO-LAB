// SPDX-License-Identifier: GPL-3.0-only
#include "ui.h"
#include "ui_theme.h"
#include "ui_text.h"
#include "ui_network.h"
#include "ui_location.h"
#include "ui_garden.h"
#include "ui_pet.h"
#include "ui_focus.h"
#include "ui_brand.h"
#include "ui_power.h"
#include "project_identity.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { PAGE_LANGUAGE, PAGE_NETWORK, PAGE_COMPANION, PAGE_DISPLAY, PAGE_SOUND, PAGE_WEATHER,
       PAGE_MOTION, PAGE_TIMERS, PAGE_SYSTEM, PAGE_POWER, PAGE_TIMER_POLICY, PAGE_THEME };
static tt_ui_submit_t submit_command;
static void *submit_context;
static atomic_bool editing;
static atomic_int save_result, pet_result;
static atomic_bool pet_open;
static tt_pet_book_t pet_book;
static bool pet_pending;
static tt_pet_action_t pet_action;
static lv_obj_t *care_button;
static lv_obj_t *achievement;
static uint32_t achievement_until;
static bool progress_seen;
static tt_settings_t cached, draft;
static uint8_t language;
static bool wizard, saving, feedback_error, network_attempted, rebuild_pending;
static char last_notice[112];
static uint32_t notice_since;
static int page;
static lv_obj_t *home, *editor, *body, *nav, *keyboard, *feedback, *next_button, *back_button, *save_cover;
static lv_obj_t *title, *subtitle, *hero, *time_text, *mode, *date_text, *detail;
static lv_obj_t *focus_layer,*home_content;
static bool focus_active, focus_idle, alarm_editor;
static int home_palette=-1,detail_style=-1;
static tt_weather_kind_t current_weather;
static lv_obj_t *temperature, *weather_icon, *weather_detail, *weather_location, *weather_credit, *status, *pet;
static lv_obj_t *settings_button;
static lv_obj_t *zone, *brightness, *brightness_text, *vibration, *sound, *volume, *reverse, *policy;
static lv_obj_t *ringtone,*alarm_duration,*alarm_enabled,*alarm_volume_text,*preview_button;
static lv_obj_t *pet_sprites[TT_PET_COUNT];
static lv_obj_t *time_wheels[3],*timer_enabled,*calibration_status,*pet_buttons[TT_PET_COUNT];
static int timer_direction;
static lv_obj_t *theme_modes[3],*theme_wheels[4];
static tt_theme_config_t theme_before;
static time_t theme_now;
static const char *states[] = {"Stopped","Running","Paused","Finished"};
#define T(s) tt_text(language, s)

static void build_editor(void);
static void home_language(void);
static void send(tt_command_type_t type);
static void open_settings(lv_event_t *e);
static void open_power(lv_event_t *e) { open_settings(e); page=PAGE_POWER; rebuild_pending=true; }
static bool collect(void);
static void feedback_test(lv_event_t *e) { (void)e; if (collect()) send(TT_CMD_FEEDBACK); }
static void feedback_stop(lv_event_t *e) { (void)e; if (page==PAGE_SOUND) send(TT_CMD_FEEDBACK_STOP); }
static void alarm_volume_change(lv_event_t *e)
{
    (void)e;
    lv_label_set_text_fmt(alarm_volume_text,"%s  %ld%%",T("Alarm volume"),(long)lv_slider_get_value(volume));
}
static void panel(lv_obj_t *obj, uint32_t color)
{
    lv_obj_set_style_radius(obj,12,0);
    tt_theme_bind(obj,LV_STYLE_BG_COLOR,color,0);
    lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);
    tt_theme_bind(obj,LV_STYLE_BORDER_COLOR,0xd8dac6,0);
    lv_obj_set_style_border_width(obj,1,0);
    tt_theme_bind(obj,LV_STYLE_TEXT_COLOR,TT_INK,0);
    lv_obj_set_style_shadow_width(obj,0,0);
    lv_obj_set_style_pad_all(obj,10,0);
    tt_theme_bind(obj,LV_STYLE_BG_COLOR,0xd2a766,LV_PART_SCROLLBAR);
}
static lv_obj_t *label(lv_obj_t *parent,const char *text)
{
    lv_obj_t *obj=lv_label_create(parent);
    tt_label_set_text(obj,text); lv_obj_set_width(obj,LV_PCT(100));
    lv_obj_set_style_text_align(obj,LV_TEXT_ALIGN_CENTER,0);
    tt_theme_bind(obj,LV_STYLE_TEXT_COLOR,TT_INK,0);
    return obj;
}
static void caption(lv_obj_t *button,const char *text)
{
    tt_label_set_text(lv_obj_get_child(button,0),text);
}
static lv_obj_t *button(lv_obj_t *parent,const char *text,lv_event_cb_t cb,intptr_t value)
{
    lv_obj_t *obj=lv_button_create(parent); panel(obj,0xe5ecd7); lv_obj_set_size(obj,196,44);
    tt_theme_bind(obj,LV_STYLE_BG_COLOR,0xbce294,LV_STATE_PRESSED);
    tt_theme_bind(obj,LV_STYLE_BG_COLOR,0xbce294,LV_STATE_CHECKED);
    tt_theme_bind(obj,LV_STYLE_BORDER_COLOR,TT_GREEN,LV_STATE_CHECKED);
    lv_obj_set_style_transform_width(obj,0,LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(obj,0,LV_STATE_PRESSED);
    lv_obj_set_style_opa(obj,LV_OPA_50,LV_STATE_DISABLED);
    if (cb) lv_obj_add_event_cb(obj,cb,LV_EVENT_CLICKED,(void *)value);
    lv_obj_t *l=label(obj,text); lv_label_set_long_mode(l,LV_LABEL_LONG_DOT); lv_obj_center(l);
    return obj;
}
static void error(const char *text)
{
    feedback_error=true; tt_label_set_text(feedback,T(text));
    tt_theme_bind(feedback,LV_STYLE_TEXT_COLOR,0xb94349,0);
}
static void send(tt_command_type_t type)
{
    tt_command_t c={.type=type,.settings=draft};
    if (!submit_command(&c,submit_context) && atomic_load(&editing)) error("Busy; please try again");
}
static void toggle(lv_event_t *e) { (void)e; send(focus_idle?TT_CMD_START:TT_CMD_TOGGLE); }
static void stop(lv_event_t *e) { (void)e; send(TT_CMD_STOP); }
static void calibrate(lv_event_t *e) { if (!saving) send((tt_command_type_t)(intptr_t)lv_event_get_user_data(e)); }
static void keyboard_hide(void)
{
    lv_keyboard_set_textarea(keyboard,NULL); lv_obj_add_flag(keyboard,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(body,page==PAGE_TIMERS?298:266);
}
static void keyboard_event(lv_event_t *e)
{
    if (lv_event_get_code(e)==LV_EVENT_READY || lv_event_get_code(e)==LV_EVENT_CANCEL) keyboard_hide();
}
static void heading(const char *text)
{
    lv_obj_t *l=label(body,T(text)); lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_LEFT,0);
    tt_theme_bind(l,LV_STYLE_TEXT_COLOR,TT_GREEN,0);
}
static lv_obj_t *dropdown(const char *name,const char *options,int selected)
{
    heading(name); lv_obj_t *obj=lv_dropdown_create(body); panel(obj,TT_CREAM);
    lv_obj_set_width(obj,LV_PCT(100)); lv_dropdown_set_options(obj,options); lv_dropdown_set_selected(obj,selected);
    lv_obj_t *list=lv_dropdown_get_list(obj); panel(list,TT_CREAM);
    lv_obj_set_style_text_font(list,&tt_font_18,0);
    tt_theme_dropdown(list);
    return obj;
}
static lv_obj_t *check(const char *text,bool checked)
{
    lv_obj_t *obj=lv_checkbox_create(body); lv_checkbox_set_text(obj,T(text)); tt_theme_check(obj);
    tt_theme_bind(obj,LV_STYLE_BG_COLOR,TT_GREEN,LV_PART_INDICATOR|LV_STATE_CHECKED);
    tt_theme_bind(obj,LV_STYLE_BORDER_COLOR,0xd8dac6,LV_PART_INDICATOR);
    if (checked) lv_obj_add_state(obj,LV_STATE_CHECKED);
    return obj;
}
static bool collect(void)
{
    if (page==PAGE_DISPLAY) {
        int offset=(int)lv_dropdown_get_selected(zone)-12;
        snprintf(draft.timezone,sizeof(draft.timezone),"UTC%+d",-offset);
        draft.brightness=(uint8_t)lv_slider_get_value(brightness);
    }
    if (page==PAGE_THEME) {
        draft.theme.night_start=(uint16_t)(lv_roller_get_selected(theme_wheels[0])*60u+lv_roller_get_selected(theme_wheels[1]));
        draft.theme.night_end=(uint16_t)(lv_roller_get_selected(theme_wheels[2])*60u+lv_roller_get_selected(theme_wheels[3]));
        if (!tt_theme_config_valid(&draft.theme)) { error("Choose different start and end times"); return false; }
    }
    if (page==PAGE_SOUND) {
        draft.vibration=lv_obj_has_state(vibration,LV_STATE_CHECKED);
        draft.sound=lv_obj_has_state(sound,LV_STATE_CHECKED);
        draft.alarm.enabled=lv_obj_has_state(alarm_enabled,LV_STATE_CHECKED);
        draft.alarm.volume=(uint8_t)lv_slider_get_value(volume);
        draft.alarm.ringtone=(uint8_t)lv_dropdown_get_selected(ringtone);
        draft.alarm.duration_s=(uint8_t[]){15,30,60}[lv_dropdown_get_selected(alarm_duration)];
    }
    if (page==PAGE_MOTION) draft.timer.reverse_rotation=lv_obj_has_state(reverse,LV_STATE_CHECKED);
    if (page==PAGE_TIMERS) {
        uint32_t seconds=lv_roller_get_selected(time_wheels[0])*3600u+
            lv_roller_get_selected(time_wheels[1])*60u+lv_roller_get_selected(time_wheels[2]);
        if (lv_obj_has_state(timer_enabled,LV_STATE_CHECKED) && seconds==0) {
            error("Choose at least 1 second"); return false;
        }
        draft.timer.seconds[timer_direction+1]=lv_obj_has_state(timer_enabled,LV_STATE_CHECKED)?seconds:0;
    }
    if (page==PAGE_TIMER_POLICY) draft.timer.policy=(tt_policy_t)lv_dropdown_get_selected(policy);
    return true;
}
static void select_direction(lv_event_t *e)
{
    if (!collect()) return;
    timer_direction=(int)(intptr_t)lv_event_get_user_data(e); rebuild_pending=true;
}
static void quick_time(lv_event_t *e)
{
    unsigned minutes=(unsigned)(intptr_t)lv_event_get_user_data(e);
    lv_roller_set_selected(time_wheels[0],minutes/60,LV_ANIM_OFF);
    lv_roller_set_selected(time_wheels[1],minutes%60,LV_ANIM_OFF);
    lv_roller_set_selected(time_wheels[2],0,LV_ANIM_OFF);
    lv_obj_add_state(timer_enabled,LV_STATE_CHECKED);
    lv_obj_send_event(time_wheels[0],LV_EVENT_VALUE_CHANGED,NULL);
}
static void time_wheel_changed(lv_event_t *e)
{
    (void)e;
    bool full_day=lv_roller_get_selected(time_wheels[0])==24;
    for (int i=1;i<3;++i) {
        if (full_day) {
            lv_roller_set_selected(time_wheels[i],0,LV_ANIM_OFF);
            lv_obj_add_state(time_wheels[i],LV_STATE_DISABLED);
        } else lv_obj_remove_state(time_wheels[i],LV_STATE_DISABLED);
    }
    feedback_error=false; tt_theme_bind(feedback,LV_STYLE_TEXT_COLOR,TT_MUTED,0);
    tt_label_set_text(feedback,T(full_day?"Maximum 24 hours":"Swipe the numbers to set the time"));
}
static void language_change(lv_event_t *e)
{
    if (saving || !collect()) return;
    language=page==PAGE_LANGUAGE?(uint8_t)(intptr_t)lv_event_get_user_data(e):(uint8_t)lv_dropdown_get_selected(lv_event_get_target_obj(e));
    draft.language=language; rebuild_pending=true; /* Rebuild after event dispatch completes. */
}
static void pet_change(lv_event_t *e)
{
    if (saving) return;
    draft.pet=(uint8_t)(intptr_t)lv_event_get_user_data(e);
    for (int i=0;i<TT_PET_COUNT;++i) {
        if (i==draft.pet) lv_obj_add_state(pet_buttons[i],LV_STATE_CHECKED);
        else lv_obj_remove_state(pet_buttons[i],LV_STATE_CHECKED);
        tt_sprite_set(pet_sprites[i],(tt_pet_t)i,TT_ANIM_IDLE,false);
        if (i==draft.pet) tt_sprite_set(pet_sprites[i],(tt_pet_t)i,TT_ANIM_PLAY,false);
    }
}
static void brightness_change(lv_event_t *e)
{
    (void)e;
    draft.brightness=(uint8_t)lv_slider_get_value(brightness);
    lv_label_set_text_fmt(brightness_text,"%s  %u%%",T("Screen brightness"),(unsigned)draft.brightness);
}
static void theme_changed(lv_event_t *e)
{
    (void)e;
    bool automatic=draft.theme.mode==TT_THEME_AUTO;
    static const unsigned modes[]={TT_THEME_DAY,TT_THEME_NIGHT,TT_THEME_AUTO};
    for (int i=0;i<3;++i) lv_obj_set_state(theme_modes[i],LV_STATE_CHECKED,draft.theme.mode==modes[i]);
    for (int i=0;i<4;++i) lv_obj_set_state(theme_wheels[i],LV_STATE_DISABLED,!automatic);
    if (collect()) {
        feedback_error=false; tt_label_set_text(feedback,"");
        tt_theme_time(theme_now,&draft.theme);
    }
}
static void theme_mode_changed(lv_event_t *e)
{
    if (saving) return;
    draft.theme.mode=(uint8_t)(intptr_t)lv_event_get_user_data(e); theme_changed(NULL);
}
static void leave(void)
{
    feedback_stop(NULL);
    network_attempted|=tt_network_ui_attempted(); tt_network_ui_close();
    tt_location_ui_close();
    keyboard_hide(); language=cached.language; home_language(); atomic_store(&editing,false); lv_screen_load(home);
}
static void back(lv_event_t *e)
{
    (void)e; if (saving) return;
    feedback_stop(NULL);
    if (page==PAGE_THEME) {
        draft.theme=theme_before; page=PAGE_DISPLAY; rebuild_pending=true;
    } else if (wizard) {
        if (!collect() || page==PAGE_LANGUAGE) return;
        --page; rebuild_pending=true;
    } else if (page==PAGE_TIMER_POLICY) {
        if (!collect()) return;
        page=PAGE_TIMERS; rebuild_pending=true;
    } else if (page==PAGE_COMPANION || page==PAGE_DISPLAY || page==PAGE_SOUND || page==PAGE_WEATHER) {
        if (!collect()) return;
        /* Back cancels the brightness preview in both the panel and the draft,
         * so a later Save on the parent page cannot restore the rejected value. */
        if (page==PAGE_DISPLAY) draft.brightness=cached.brightness;
        page=PAGE_SYSTEM; rebuild_pending=true;
    } else {
        network_attempted|=tt_network_ui_attempted();
        if (network_attempted) { tt_command_t c={.type=TT_CMD_NETWORK,.settings=cached}; (void)submit_command(&c,submit_context); }
        leave();
    }
}
static void next(lv_event_t *e)
{
    (void)e; if (saving || !collect()) return;
    feedback_stop(NULL);
    if (wizard && page==PAGE_THEME) { page=PAGE_DISPLAY; rebuild_pending=true; return; }
    if (wizard && page<PAGE_MOTION) { ++page; rebuild_pending=true; return; }
    draft.timer.home_axis=cached.timer.home_axis; draft.timer.right_axis=cached.timer.right_axis;
    draft.timer.calibrated=cached.timer.calibrated; draft.setup_complete=true;
    tt_command_t c={.type=TT_CMD_SAVE,.settings=draft};
    if (!tt_settings_valid(&draft)) { error("Invalid settings"); return; }
    atomic_store(&save_result,0);
    if (!submit_command(&c,submit_context)) { error("Busy; please try again"); return; }
    saving=true; feedback_error=false; caption(next_button,T("Saving..."));
    lv_obj_add_state(next_button,LV_STATE_DISABLED); lv_obj_add_state(back_button,LV_STATE_DISABLED); keyboard_hide();
    save_cover=lv_obj_create(editor); lv_obj_remove_style_all(save_cover); lv_obj_set_pos(save_cover,0,0); lv_obj_set_size(save_cover,480,480);
}
static void tab(lv_event_t *e)
{
    if (saving || !collect()) return;
    feedback_stop(NULL);
    page=(int)(intptr_t)lv_event_get_user_data(e);
    if (page==PAGE_THEME) theme_before=draft.theme;
    rebuild_pending=true;
}
static void open_settings(lv_event_t *e)
{
    (void)e; draft=cached; wizard=false; saving=false; network_attempted=false; page=PAGE_SYSTEM;
    atomic_store(&editing,true); build_editor(); lv_screen_load(editor);
}
static void build_editor(void)
{
    network_attempted|=tt_network_ui_attempted(); tt_network_ui_close();
    tt_location_ui_close();
    keyboard_hide(); lv_obj_clean(body);
    zone=brightness=vibration=reverse=policy=NULL; calibration_status=NULL;
    feedback_error=false; tt_label_set_text(feedback,""); tt_theme_bind(feedback,LV_STYLE_TEXT_COLOR,TT_MUTED,0);
    lv_obj_remove_state(next_button,LV_STATE_DISABLED); lv_obj_remove_state(back_button,LV_STATE_DISABLED);
    if (wizard && page==PAGE_LANGUAGE) lv_obj_add_state(back_button,LV_STATE_DISABLED);
    caption(back_button,T("Back")); caption(next_button,T(wizard?(page==PAGE_MOTION?"Finish":"Next"):"Save"));
    if (wizard && page==PAGE_THEME) caption(next_button,T("Apply"));
    static const char *titles[]={"Version","Connect to your world","Choose your companion","Display preferences",
        "Timer alarm","Weather location","Ready, set, flip"};
    static const char *subtitles[]={"Flip timer / weather clock / pet companion","Wi-Fi for time and weather",
        "Tap a friend and watch them play","Brightness and local time","Flip or tap to dismiss; stops at the time limit",
        "Locate or choose your city","Four upright sides and one flat position"};
    bool branded=wizard && page==PAGE_LANGUAGE;
    if (branded) lv_label_set_text_fmt(title,"%s %s",T("Version"),TT_PRODUCT_VERSION);
    else tt_label_set_text(title,T(page==PAGE_THEME?"Theme and schedule":page==PAGE_TIMERS?"Timer presets":page==PAGE_TIMER_POLICY?"When returned upright":page==PAGE_POWER?"Battery and power":wizard || (page>=PAGE_COMPANION && page<=PAGE_WEATHER)?titles[page]:"Your little settings"));
    tt_label_set_text(subtitle,T(page==PAGE_THEME?"Choose your look, follow your local time":wizard || page==PAGE_WEATHER || page==PAGE_SOUND?subtitles[page]:"A little care for your everyday"));
    lv_obj_set_width(title,440);
    lv_obj_set_y(title,page==PAGE_TIMERS?12:branded?33:24);
    if (page==PAGE_TIMERS) lv_obj_add_flag(subtitle,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(subtitle,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(nav,page==PAGE_TIMERS?54:94);
    lv_obj_set_y(body,page==PAGE_TIMERS?96:132);
    lv_obj_set_height(body,page==PAGE_TIMERS?298:266);
    if (page==PAGE_TIMERS) lv_obj_remove_flag(body,LV_OBJ_FLAG_SCROLLABLE);
    else lv_obj_add_flag(body,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(title,branded?&tt_font_16:&tt_font_24,0);
    tt_theme_bind(title,LV_STYLE_TEXT_COLOR,branded?TT_MUTED:TT_INK,0);
    lv_obj_clean(nav);
    static const int tabs[]={PAGE_NETWORK,PAGE_SYSTEM,PAGE_TIMERS,PAGE_MOTION};
    static const char *names[]={"Network","System","Timers","Calibrate"};
    for (int i=0;i<(wizard?7:4);++i) {
        lv_obj_t *b;
        if (wizard) {
            b=lv_obj_create(nav); lv_obj_remove_style_all(b); lv_obj_set_size(b,52,5);
            lv_obj_set_style_bg_opa(b,LV_OPA_COVER,0); tt_theme_bind(b,LV_STYLE_BG_COLOR,i<=page?TT_GREEN:0xe8d6ae,0);
        } else {
            b=button(nav,T(names[i]),tab,tabs[i]); lv_obj_set_size(b,100,34);
            if (page==tabs[i] || (tabs[i]==PAGE_SYSTEM && page==PAGE_THEME) || (tabs[i]==PAGE_TIMERS && page==PAGE_TIMER_POLICY) ||
                (tabs[i]==PAGE_SYSTEM && page>=PAGE_COMPANION && page<=PAGE_WEATHER))
                lv_obj_add_state(b,LV_STATE_CHECKED);
        }
        lv_obj_set_pos(b,i*(wizard?62:108),wizard?16:0);
    }
    lv_obj_set_style_text_font(body,&tt_font_18,0);
    lv_obj_set_flex_flow(body,LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(body,10,0);
    if (page==PAGE_LANGUAGE) {
        lv_obj_set_style_pad_row(body,6,0);
        lv_obj_t *row=lv_obj_create(body); lv_obj_remove_style_all(row); lv_obj_set_size(row,LV_PCT(100),54);
        lv_obj_t *zh=button(row,"简体中文",language_change,TT_ZH),*en=button(row,"English",language_change,TT_EN);
        lv_obj_set_size(zh,190,50); lv_obj_set_pos(en,204,0); lv_obj_set_size(en,190,50);
        lv_obj_add_state(language==TT_ZH?zh:en,LV_STATE_CHECKED);
        row=lv_obj_create(body); lv_obj_remove_style_all(row); lv_obj_set_size(row,LV_PCT(100),154);
        lv_obj_remove_flag(row,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *art=lv_image_create(row); lv_image_set_src(art,&tt_art_brand); lv_obj_center(art);
        lv_obj_remove_flag(art,LV_OBJ_FLAG_CLICKABLE);
        label(body,T("You can change this later in Settings."));
    } else if (page==PAGE_NETWORK) {
        tt_network_ui_create(body,&draft,submit_command,submit_context);
    } else if (page==PAGE_SYSTEM) {
        lv_obj_t *lang=dropdown("Language","简体中文\nEnglish",draft.language);
        lv_obj_add_event_cb(lang,language_change,LV_EVENT_VALUE_CHANGED,NULL);
        lv_obj_t *row=lv_obj_create(body); lv_obj_remove_style_all(row); lv_obj_set_size(row,LV_PCT(100),112);
        const char *labels[]={"Companion","Display","Timer alarm","Weather"};
        const int pages[]={PAGE_COMPANION,PAGE_DISPLAY,PAGE_SOUND,PAGE_WEATHER};
        for (int i=0;i<4;++i) {
            lv_obj_t *b=button(row,T(labels[i]),tab,pages[i]);
            lv_obj_set_pos(b,(i%2)*202,(i/2)*56); lv_obj_set_size(b,192,48);
        }
        lv_obj_set_width(button(body,T("Battery and power"),tab,PAGE_POWER),LV_PCT(100));
    } else if (page==PAGE_COMPANION) {
        heading("Your focus companion");
        draft.pet=TT_DAIMAO;
        lv_obj_t *row=lv_obj_create(body); lv_obj_remove_style_all(row); lv_obj_set_size(row,LV_PCT(100),160);
        lv_obj_remove_flag(row,LV_OBJ_FLAG_SCROLLABLE);
        pet_sprites[0]=tt_sprite_create(row,146,0,TT_DAIMAO);
        lv_obj_add_event_cb(pet_sprites[0],pet_change,LV_EVENT_CLICKED,NULL);
        pet_buttons[0]=button(row,T("Dai Meow"),pet_change,0);
        lv_obj_set_pos(pet_buttons[0],80,112); lv_obj_set_size(pet_buttons[0],240,42);
        lv_obj_add_state(pet_buttons[0],LV_STATE_CHECKED);
        tt_sprite_set(pet_sprites[0],TT_DAIMAO,TT_ANIM_PLAY,false);
        label(body,T("No daily chores. Your friend grows with your focus."));
    } else if (page==PAGE_POWER) {
        tt_power_details(body,language);
    } else if (page==PAGE_DISPLAY) {
        lv_obj_set_style_pad_row(body,6,0);
        brightness_text=label(body,"");
        lv_obj_t *row=lv_obj_create(body); lv_obj_remove_style_all(row); lv_obj_set_size(row,LV_PCT(100),76);
        lv_obj_remove_flag(row,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
        brightness=lv_slider_create(row); lv_obj_set_size(brightness,LV_PCT(90),20); lv_obj_center(brightness);
        /* A 64px touch band stays inside its own row; the 44px knob is easier
         * to see and grab without stealing touches from the timezone field. */
        lv_obj_set_ext_click_area(brightness,22);
        lv_obj_remove_flag(brightness,LV_OBJ_FLAG_ADV_HITTEST);
        lv_obj_set_style_pad_all(brightness,12,LV_PART_KNOB);
        lv_slider_set_range(brightness,10,100); lv_slider_set_value(brightness,draft.brightness,LV_ANIM_OFF);
        tt_theme_bind(brightness,LV_STYLE_BG_COLOR,0xcfe5f2,0);
        tt_theme_bind(brightness,LV_STYLE_BG_COLOR,TT_GREEN,LV_PART_INDICATOR);
        tt_theme_bind(brightness,LV_STYLE_BG_COLOR,0xffc85b,LV_PART_KNOB);
        lv_obj_add_event_cb(brightness,brightness_change,LV_EVENT_VALUE_CHANGED,NULL); brightness_change(NULL);
        const char *sign=strpbrk(draft.timezone,"+-"); int offset=sign?-atoi(sign):0;
        zone=dropdown("Time zone","UTC-12\nUTC-11\nUTC-10\nUTC-9\nUTC-8\nUTC-7\nUTC-6\nUTC-5\nUTC-4\nUTC-3\nUTC-2\nUTC-1\nUTC\nUTC+1\nUTC+2\nUTC+3\nUTC+4\nUTC+5\nUTC+6\nUTC+7\nUTC+8\nUTC+9\nUTC+10\nUTC+11\nUTC+12\nUTC+13\nUTC+14",offset+12);
        lv_obj_set_height(zone,40);
        lv_obj_t *themes=button(body,T("Theme and schedule  >"),tab,PAGE_THEME);
        lv_obj_set_width(themes,LV_PCT(100));
    } else if (page==PAGE_THEME) {
        lv_obj_remove_flag(body,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_row(body,6,0);
        lv_obj_t *row=lv_obj_create(body); lv_obj_remove_style_all(row); lv_obj_set_size(row,LV_PCT(100),42);
        static const char *names[]={"Day theme","Night theme","Automatic theme"};
        static const unsigned modes[]={TT_THEME_DAY,TT_THEME_NIGHT,TT_THEME_AUTO};
        for (int i=0;i<3;++i) {
            theme_modes[i]=button(row,T(names[i]),theme_mode_changed,modes[i]);
            lv_obj_set_pos(theme_modes[i],i*136,0); lv_obj_set_size(theme_modes[i],126,42);
            lv_obj_set_style_pad_all(theme_modes[i],4,0);
        }
        row=lv_obj_create(body); lv_obj_remove_style_all(row); lv_obj_set_size(row,LV_PCT(100),156);
        lv_obj_remove_flag(row,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
        for (int group=0;group<2;++group) {
            lv_obj_t *box=lv_obj_create(row); lv_obj_remove_style_all(box);
            lv_obj_set_pos(box,group*208,0); lv_obj_set_size(box,198,156);
            lv_obj_remove_flag(box,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
            lv_obj_t *name=label(box,T(group?"Day starts":"Night starts"));
            lv_obj_set_style_text_font(name,&tt_font_16,0);
            unsigned minutes=group?draft.theme.night_end:draft.theme.night_start;
            for (int part=0;part<2;++part) {
                char options[200]; unsigned count=part?60:24; size_t used=0;
                for (unsigned n=0;n<count;++n) used+=(size_t)snprintf(options+used,sizeof(options)-used,"%s%02u",n?"\n":"",n);
                lv_obj_t *wheel=lv_roller_create(box); theme_wheels[group*2+part]=wheel;
                panel(wheel,TT_CREAM); lv_obj_set_style_pad_all(wheel,0,0);
                lv_obj_set_style_text_font(wheel,&tt_font_18,0); lv_obj_set_style_text_line_space(wheel,10,0);
                lv_obj_set_style_text_font(wheel,&tt_font_24,LV_PART_SELECTED);
                tt_theme_bind(wheel,LV_STYLE_TEXT_COLOR,TT_MUTED,0);
                tt_theme_bind(wheel,LV_STYLE_TEXT_COLOR,TT_INK,LV_PART_SELECTED);
                tt_theme_bind(wheel,LV_STYLE_BG_COLOR,0xdce9ca,LV_PART_SELECTED);
                lv_obj_set_style_bg_opa(wheel,255,LV_PART_SELECTED); lv_obj_set_style_radius(wheel,10,LV_PART_SELECTED);
                lv_obj_set_style_opa(wheel,LV_OPA_50,LV_STATE_DISABLED);
                lv_roller_set_options(wheel,options,LV_ROLLER_MODE_NORMAL); lv_roller_set_visible_row_count(wheel,3);
                lv_obj_set_pos(wheel,part*98,28); lv_obj_set_size(wheel,92,96);
                lv_roller_set_selected(wheel,part?minutes%60:minutes/60,LV_ANIM_OFF);
                lv_obj_add_event_cb(wheel,theme_changed,LV_EVENT_VALUE_CHANGED,NULL);
                lv_obj_t *unit=label(box,T(part?"Minutes":"Hours")); lv_obj_set_width(unit,92);
                lv_obj_set_pos(unit,part*98,130); lv_obj_set_style_text_font(unit,&tt_font_16,0);
            }
        }
        lv_obj_t *hint=label(body,T("Preview now; save to keep. Back cancels."));
        lv_obj_set_style_text_font(hint,&tt_font_16,0); theme_changed(NULL);
    } else if (page==PAGE_SOUND) {
        lv_obj_set_style_text_font(body,&tt_font_16,0); lv_obj_set_style_pad_row(body,6,0);
        lv_obj_t *row=lv_obj_create(body); lv_obj_remove_style_all(row); lv_obj_set_size(row,LV_PCT(100),26);
        alarm_enabled=check("Ring",draft.alarm.enabled); lv_obj_set_parent(alarm_enabled,row);
        vibration=check("Vibrate",draft.vibration); lv_obj_set_parent(vibration,row); lv_obj_set_x(vibration,212);
        row=lv_obj_create(body); lv_obj_remove_style_all(row); lv_obj_set_size(row,LV_PCT(100),68);
        const char *names[]={"Ringtone","Repeat for"};
        const char *options[]={T("Clear bells\nDigital alarm\nBright melody\nDing-dong\nWind chimes\nWooden notes\nMorning light\nGame complete\nRhythmic beeps"),T("15 seconds\n30 seconds\n60 seconds")};
        for (int i=0;i<2;++i) {
            lv_obj_t *l=label(row,T(names[i])); lv_obj_set_width(l,198); lv_obj_set_x(l,i*212);
            lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_LEFT,0);
            lv_obj_t *d=lv_dropdown_create(row); panel(d,TT_CREAM); lv_obj_set_size(d,198,40); lv_obj_set_pos(d,i*212,24);
            lv_dropdown_set_options(d,options[i]);
            lv_dropdown_set_selected(d,i?(draft.alarm.duration_s==15?0:draft.alarm.duration_s==30?1:2):draft.alarm.ringtone);
            lv_obj_t *list=lv_dropdown_get_list(d); panel(list,TT_CREAM); lv_obj_set_style_text_font(list,&tt_font_18,0);
            tt_theme_dropdown(list);
            lv_obj_set_style_max_height(list,210,0);
            if (i) alarm_duration=d; else ringtone=d;
        }
        alarm_volume_text=label(body,"");
        row=lv_obj_create(body); lv_obj_remove_style_all(row); lv_obj_set_size(row,LV_PCT(100),24);
        volume=lv_slider_create(row); lv_obj_set_size(volume,LV_PCT(90),12); lv_obj_center(volume);
        lv_slider_set_range(volume,0,80); lv_slider_set_value(volume,draft.alarm.volume,LV_ANIM_OFF);
        tt_theme_bind(volume,LV_STYLE_BG_COLOR,0xcfe5f2,0);
        tt_theme_bind(volume,LV_STYLE_BG_COLOR,TT_GREEN,LV_PART_INDICATOR);
        tt_theme_bind(volume,LV_STYLE_BG_COLOR,0xffc85b,LV_PART_KNOB);
        lv_obj_add_event_cb(volume,alarm_volume_change,LV_EVENT_VALUE_CHANGED,NULL); alarm_volume_change(NULL);
        row=lv_obj_create(body); lv_obj_remove_style_all(row); lv_obj_set_size(row,LV_PCT(100),38);
        preview_button=button(row,T("Preview 3 seconds"),feedback_test,0); lv_obj_set_size(preview_button,198,38);
        lv_obj_t *stop_preview=button(row,T("Stop preview"),feedback_stop,0); lv_obj_set_pos(stop_preview,212,0); lv_obj_set_size(stop_preview,198,38);
        sound=check("Quiet interaction sounds",draft.sound);
    } else if (page==PAGE_WEATHER) {
        lv_obj_set_style_text_font(body,&tt_font_16,0); lv_obj_set_style_pad_row(body,6,0);
        tt_location_ui_create(body,&draft,submit_command,submit_context);
    } else if (page==PAGE_MOTION) {
        lv_obj_set_style_text_font(body,&tt_font_16,0); lv_obj_set_style_pad_row(body,6,0);
        label(body,T("1. Place upright, then tap below.")); button(body,T("Set upright"),calibrate,TT_CMD_HOME);
        label(body,T("2. Turn clockwise 90 degrees, then tap.")); button(body,T("Set right turn"),calibrate,TT_CMD_RIGHT);
        calibration_status=label(body,T(cached.timer.calibrated?"Calibration complete":"Calibration needed for flip timers"));
        tt_theme_bind(calibration_status,LV_STYLE_TEXT_COLOR,TT_GREEN,0);
        reverse=check("Reverse screen rotation",draft.timer.reverse_rotation);
        label(body,T("Flat keeps the last rotation and current timer; no new timer starts."));
    } else if (page==PAGE_TIMER_POLICY) {
        policy=dropdown("When returned upright",T("Pause; restart on flip\nContinue in background\nPause; resume same face"),draft.timer.policy);
        label(body,T("Flat keeps the last rotation and current timer; no new timer starts."));
    } else if (page==PAGE_TIMERS) {
        lv_obj_set_style_pad_row(body,6,0);
        lv_obj_t *row=lv_obj_create(body); lv_obj_remove_style_all(row); lv_obj_set_size(row,LV_PCT(100),32);
        lv_obj_remove_flag(row,LV_OBJ_FLAG_SCROLLABLE);
        static const char *directions[]={"Right","Inverted","Left"};
        for (int i=0;i<3;++i) {
            lv_obj_t *b=button(row,T(directions[i]),select_direction,i); lv_obj_set_pos(b,i*136,0); lv_obj_set_size(b,126,32);
            lv_obj_set_style_pad_all(b,2,0); lv_obj_set_style_text_font(b,&tt_font_16,0);
            if (i==timer_direction) lv_obj_add_state(b,LV_STATE_CHECKED);
        }
        timer_enabled=check("Enable this side",draft.timer.seconds[timer_direction+1]>0);
        lv_obj_set_style_text_font(timer_enabled,&tt_font_16,0);
        lv_obj_set_style_pad_all(timer_enabled,0,0);
        row=lv_obj_create(body); lv_obj_remove_style_all(row); lv_obj_set_size(row,LV_PCT(100),132);
        lv_obj_remove_flag(row,LV_OBJ_FLAG_SCROLLABLE);
        uint32_t sec=draft.timer.seconds[timer_direction+1];
        const char *units[]={"Hours","Minutes","Seconds"};
        const unsigned values[]={sec/3600,sec/60%60,sec%60};
        for (int i=0;i<3;++i) {
            lv_obj_t *l=label(row,T(units[i])); lv_obj_set_pos(l,i*136,0); lv_obj_set_width(l,126);
            lv_obj_set_style_text_font(l,&tt_font_16,0);
            char options[181]; size_t used=0;
            unsigned count=i?60:25;
            for (unsigned n=0;n<count;++n) used+=(size_t)snprintf(options+used,sizeof(options)-used,"%s%02u",n?"\n":"",n);
            lv_obj_t *wheel=lv_roller_create(row); time_wheels[i]=wheel;
            panel(wheel,TT_CREAM); lv_obj_set_style_pad_all(wheel,0,0);
            lv_obj_set_style_text_font(wheel,&tt_font_24,0);
            lv_obj_set_style_text_line_space(wheel,10,0);
            tt_theme_bind(wheel,LV_STYLE_TEXT_COLOR,0xa5ac99,0);
            lv_obj_set_style_text_font(wheel,&tt_font_52,LV_PART_SELECTED);
            tt_theme_bind(wheel,LV_STYLE_TEXT_COLOR,TT_INK,LV_PART_SELECTED);
            tt_theme_bind(wheel,LV_STYLE_BG_COLOR,0xdce9ca,LV_PART_SELECTED);
            lv_obj_set_style_bg_opa(wheel,LV_OPA_COVER,LV_PART_SELECTED);
            lv_obj_set_style_radius(wheel,10,LV_PART_SELECTED);
            lv_obj_set_style_opa(wheel,LV_OPA_50,LV_STATE_DISABLED);
            lv_obj_set_style_anim_duration(wheel,180,0);
            lv_roller_set_options(wheel,options,LV_ROLLER_MODE_NORMAL);
            lv_roller_set_visible_row_count(wheel,3);
            lv_obj_set_pos(wheel,i*136,22); lv_obj_set_size(wheel,126,108);
            lv_roller_set_selected(wheel,values[i],LV_ANIM_OFF);
            lv_obj_remove_flag(wheel,LV_OBJ_FLAG_SCROLL_CHAIN_VER);
            lv_obj_add_event_cb(wheel,time_wheel_changed,LV_EVENT_VALUE_CHANGED,NULL);
        }
        time_wheel_changed(NULL);
        row=lv_obj_create(body); lv_obj_remove_style_all(row); lv_obj_set_size(row,LV_PCT(100),32);
        lv_obj_remove_flag(row,LV_OBJ_FLAG_SCROLLABLE);
        const int quick[]={5,15,25,60};
        for (int i=0;i<4;++i) {
            char text[20]; snprintf(text,sizeof(text),"%d %s",quick[i],T("min"));
            lv_obj_t *b=button(row,text,quick_time,quick[i]); lv_obj_set_pos(b,i*101,0); lv_obj_set_size(b,96,32);
            lv_obj_set_style_pad_all(b,2,0); lv_obj_set_style_text_font(b,&tt_font_16,0);
        }
        lv_obj_t *behavior=button(body,T("Upright behavior  >"),tab,PAGE_TIMER_POLICY);
        lv_obj_set_size(behavior,LV_PCT(100),28); lv_obj_set_style_pad_all(behavior,2,0);
        lv_obj_set_style_text_font(behavior,&tt_font_16,0);
    }
    lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);
}
static lv_obj_t *screen(void)
{
    lv_obj_t *obj=lv_obj_create(NULL); lv_obj_set_style_text_font(obj,&tt_font_18,0);
    lv_obj_set_style_pad_all(obj,0,0); tt_theme_bind(obj,LV_STYLE_TEXT_COLOR,TT_INK,0);
    lv_obj_remove_flag(obj,LV_OBJ_FLAG_SCROLLABLE); return obj;
}
static lv_obj_t *at_label(lv_obj_t *parent,const char *text,int x,int y,int w,int h)
{
    lv_obj_t *obj=label(parent,text); lv_obj_set_pos(obj,x,y); lv_obj_set_size(obj,w,h); lv_label_set_long_mode(obj,LV_LABEL_LONG_DOT); return obj;
}
static void home_language(void) { caption(settings_button,T("Settings")); caption(care_button,T("Pet care")); }
void tt_ui_interaction_snapshot(const tt_interaction_view_t *view)
{
    if (atomic_load(&editing) && page==PAGE_SOUND && !rebuild_pending) {
        caption(preview_button,T(view->alarm_active?"Previewing...":"Preview 3 seconds"));
        if (view->audio_error) error("Audio unavailable");
    }
}
static void pet_request(tt_pet_action_t action)
{
    if (pet_pending || tt_garden_busy(atomic_load(&pet_open)?tt_pet_ui_garden():pet)) return;
    tt_command_t command={.type=TT_CMD_PET,.settings=cached,.pet_action=action};
    pet_action=action; atomic_store(&pet_result,0);
    if (submit_command(&command,submit_context)) pet_pending=true;
    else atomic_store(&pet_result,TT_PET_WAIT+1);
}
static void pet_cuddle(void) { pet_request(TT_PET_CUDDLE); }
static void pet_back(lv_event_t *e)
{
    (void)e; if (pet_pending) return;
    atomic_store(&pet_open,false); lv_screen_load(home);
}
static void open_pet(lv_event_t *e)
{
    (void)e; if (pet_pending || focus_active) return;
    atomic_store(&pet_open,true);
    tt_pet_ui_open((tt_pet_t)cached.pet,cached.language,pet_back,pet_request);
    tt_garden_weather(tt_pet_ui_garden(),current_weather);
    tt_pet_ui_update(&pet_book.pets[cached.pet],&pet_book.focus,false);
}
void tt_ui_pet_result(tt_pet_result_t result) { atomic_store(&pet_result,result+1); }
void tt_ui_pet_snapshot(const tt_pet_book_t *book)
{
    uint32_t unlocked=progress_seen?(book->focus.badges&~pet_book.focus.badges):0; progress_seen=true;
    if (unlocked) {
        unsigned i=0; while (!(unlocked&(1u<<i))) ++i;
        lv_label_set_text_fmt(achievement,"%s\n%s",T("Achievement unlocked"),T(tt_focus_badge_title(i)));
        achievement_until=lv_tick_get()+5000;
    }
    pet_book=*book;
    tt_garden_set(pet,(tt_pet_t)cached.pet,0,cached.language,&pet_book.pets[cached.pet]);
    if (atomic_load(&pet_open)) tt_pet_ui_update(&pet_book.pets[cached.pet],&pet_book.focus,pet_pending);
    int result=atomic_exchange(&pet_result,0);
    if (result) {
        pet_pending=false; tt_pet_result_t value=(tt_pet_result_t)(result-1);
        const tt_pet_state_t *state=&pet_book.pets[cached.pet];
        if (atomic_load(&pet_open)) tt_pet_ui_feedback(value,pet_action,state);
        else if (value==TT_PET_OK) tt_garden_react(pet,pet_action,tt_pet_result_text(value,pet_action,state->resting));
        else tt_garden_say(pet,tt_pet_result_text(value,pet_action,state->resting));
    }
}
void tt_ui_init(tt_ui_submit_t submit,void *context,const tt_settings_t *settings)
{
    submit_command=submit; submit_context=context; cached=draft=*settings; language=settings->language;
    atomic_store(&save_result,0); saving=false; rebuild_pending=false;
    tt_pet_default(&pet_book); atomic_store(&pet_open,false); atomic_store(&pet_result,0); pet_pending=false;
    current_weather=TT_WEATHER_UNKNOWN;
    focus_active=focus_idle=alarm_editor=false;
    home_palette=detail_style=-1;
    progress_seen=false; achievement_until=0;
    theme_now=time(NULL); tt_theme_time(theme_now,&settings->theme);
    home=screen(); tt_theme_backdrop(home);
    pet=tt_garden_fullscreen(home,(tt_pet_t)settings->pet,language,pet_cuddle);
    home_content=lv_obj_create(home); lv_obj_remove_style_all(home_content); lv_obj_set_size(home_content,480,480);
    lv_obj_remove_flag(home_content,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    /* A soft fading veil keeps text readable without splitting the landscape into cards. */
    hero=lv_obj_create(home_content); lv_obj_remove_style_all(hero); lv_obj_set_pos(hero,0,0); lv_obj_set_size(hero,480,242);
    lv_obj_remove_flag(hero,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    tt_theme_bind(hero,LV_STYLE_BG_COLOR,0xf5fbf4,0); tt_theme_bind(hero,LV_STYLE_BG_GRAD_COLOR,0xf5fbf4,0);
    lv_obj_set_style_bg_opa(hero,LV_OPA_COVER,0); lv_obj_set_style_bg_main_opa(hero,204,0);
    lv_obj_set_style_bg_grad_opa(hero,0,0); lv_obj_set_style_bg_grad_dir(hero,LV_GRAD_DIR_VER,0);
    mode=at_label(home_content,TT_PRODUCT_TITLE,24,22,250,28); lv_obj_set_style_text_font(mode,&tt_font_18,0);
    lv_obj_set_style_text_align(mode,LV_TEXT_ALIGN_LEFT,0);
    tt_theme_bind(mode,LV_STYLE_TEXT_COLOR,TT_GREEN,0);
    time_text=at_label(home_content,"--:--",24,80,268,66); lv_obj_set_style_text_font(time_text,&tt_font_52,0);
    lv_obj_set_style_text_align(time_text,LV_TEXT_ALIGN_LEFT,0);
    date_text=at_label(home_content,"",26,151,264,24); lv_obj_set_style_text_font(date_text,&tt_font_16,0);
    lv_obj_set_style_text_align(date_text,LV_TEXT_ALIGN_LEFT,0);
    detail=at_label(home_content,"",24,190,432,54); lv_obj_set_style_text_font(detail,&tt_font_24,0);
    lv_obj_set_style_text_align(detail,LV_TEXT_ALIGN_LEFT,0);
    lv_label_set_long_mode(detail,LV_LABEL_LONG_WRAP);
    tt_theme_bind(detail,LV_STYLE_TEXT_COLOR,TT_INK,0);
    tt_theme_bind(detail,LV_STYLE_BG_COLOR,TT_CREAM,0); lv_obj_set_style_bg_opa(detail,210,0);
    lv_obj_set_style_radius(detail,15,0); lv_obj_set_style_pad_all(detail,10,0);
    temperature=at_label(home_content,"-- C",294,119,154,32); lv_obj_set_style_text_font(temperature,&tt_font_24,0);
    weather_icon=tt_theme_weather(home_content,346,64);
    weather_detail=at_label(home_content,"",290,155,166,22); lv_obj_set_style_text_font(weather_detail,&tt_font_16,0);
    weather_location=at_label(home_content,"",24,51,300,20); lv_obj_set_style_text_font(weather_location,&tt_font_16,0);
    lv_obj_set_style_text_align(weather_location,LV_TEXT_ALIGN_LEFT,0);
    weather_credit=at_label(home_content,"",24,448,432,24); lv_obj_set_style_text_font(weather_credit,&tt_font_16,0);
    tt_theme_bind(weather_credit,LV_STYLE_BG_COLOR,TT_CREAM,0); lv_obj_set_style_bg_opa(weather_credit,160,0);
    lv_obj_set_style_radius(weather_credit,12,0);
    tt_theme_bind(weather_credit,LV_STYLE_TEXT_COLOR,TT_INK,0);
    status=at_label(home_content,"",24,195,432,44); lv_obj_set_style_text_font(status,&tt_font_16,0);
    lv_obj_set_style_text_align(status,LV_TEXT_ALIGN_LEFT,0);
    lv_label_set_long_mode(status,LV_LABEL_LONG_WRAP);
    tt_theme_bind(status,LV_STYLE_TEXT_COLOR,TT_MUTED,0);
    care_button=button(home_content,T("Pet care"),open_pet,0); lv_obj_set_pos(care_button,350,402); lv_obj_set_size(care_button,106,36);
    tt_theme_bind(care_button,LV_STYLE_BG_COLOR,TT_CREAM,0); lv_obj_set_style_bg_opa(care_button,210,0);
    lv_obj_set_style_border_width(care_button,0,0); lv_obj_set_style_text_font(care_button,&tt_font_16,0);

    focus_layer=tt_focus_create(home,toggle,stop);
    settings_button=button(home,T("Settings"),open_settings,0); lv_obj_set_pos(settings_button,380,18); lv_obj_set_size(settings_button,80,38);
    tt_theme_bind(settings_button,LV_STYLE_BG_COLOR,TT_CREAM,0);
    lv_obj_set_style_bg_opa(settings_button,210,0); lv_obj_set_style_border_width(settings_button,0,0);
    lv_obj_set_style_text_font(settings_button,&tt_font_16,0);
    lv_obj_set_style_pad_hor(settings_button,4,0);
    tt_power_badge(home,open_power);
    achievement=at_label(home,"",20,12,334,56); lv_obj_set_style_text_font(achievement,&tt_font_16,0);
    tt_theme_bind(achievement,LV_STYLE_BG_COLOR,TT_GREEN,0); lv_obj_set_style_bg_opa(achievement,255,0);
    tt_theme_bind(achievement,LV_STYLE_TEXT_COLOR,TT_CREAM,0); lv_obj_set_style_radius(achievement,10,0);
    lv_obj_set_style_pad_all(achievement,5,0); lv_obj_add_flag(achievement,LV_OBJ_FLAG_HIDDEN);
    editor=screen(); tt_theme_backdrop(editor);
    title=at_label(editor,"",20,24,440,32); lv_obj_set_style_text_font(title,&tt_font_24,0);
    subtitle=at_label(editor,"",20,61,440,24); tt_theme_bind(subtitle,LV_STYLE_TEXT_COLOR,TT_MUTED,0);
    nav=lv_obj_create(editor); lv_obj_remove_style_all(nav); lv_obj_set_pos(nav,24,94); lv_obj_set_size(nav,432,36);
    body=lv_obj_create(editor); panel(body,TT_PAPER); lv_obj_set_pos(body,24,132); lv_obj_set_size(body,432,266);
    feedback=at_label(editor,"",24,398,432,24);
    back_button=button(editor,T("Back"),back,0); lv_obj_set_pos(back_button,24,424); lv_obj_set_size(back_button,204,40);
    next_button=button(editor,T("Next"),next,0); lv_obj_set_pos(next_button,252,424); lv_obj_set_size(next_button,204,40);
    keyboard=lv_keyboard_create(editor); panel(keyboard,TT_WOOD);
    lv_obj_set_style_text_font(keyboard,&lv_font_montserrat_14,LV_PART_ITEMS);
    tt_theme_bind(keyboard,LV_STYLE_BG_COLOR,TT_CREAM,LV_PART_ITEMS); tt_theme_bind(keyboard,LV_STYLE_TEXT_COLOR,TT_INK,LV_PART_ITEMS);
    tt_theme_bind(keyboard,LV_STYLE_BG_COLOR,0xc4e7ac,LV_PART_ITEMS|LV_STATE_CHECKED);
    tt_theme_bind(keyboard,LV_STYLE_BG_COLOR,0xc4e7ac,LV_PART_ITEMS|LV_STATE_PRESSED);
    lv_obj_set_size(keyboard,480,194); lv_obj_align(keyboard,LV_ALIGN_BOTTOM_MID,0,0); lv_obj_add_event_cb(keyboard,keyboard_event,LV_EVENT_ALL,NULL);
    keyboard_hide(); wizard=!settings->setup_complete; page=PAGE_LANGUAGE; atomic_store(&editing,wizard);
    if (wizard) { build_editor(); lv_screen_load(editor); } else lv_screen_load(home);
}
bool tt_ui_editing(void) { return atomic_load(&editing); }
unsigned tt_ui_display_brightness(unsigned normal,bool alert)
{
    return !alert && atomic_load(&editing) && page==PAGE_DISPLAY &&
        lv_screen_active()==editor ? draft.brightness : normal;
}
bool tt_ui_pet_open(void) { return atomic_load(&pet_open); }
void tt_ui_save_result(bool success) { atomic_store(&save_result,success?1:-1); }
void tt_ui_render(const tt_timer_t *t,tt_face_t face,const tt_settings_t *settings,const tt_services_view_t *s,const char *notice,bool alarm)
{
    cached=*settings;
    tt_power_ui_update(&s->power,settings->language);
    alarm=alarm || t->state==TT_FINISHED;
    bool timer_face=face>TT_HOME && face<TT_FRONT;
    current_weather=s->weather_valid?tt_weather_kind(s->weather_code):TT_WEATHER_UNKNOWN;
    time_t scene_now=s->scene_time?s->scene_time:time(NULL); struct tm scene_clock;
#ifdef _WIN32
    localtime_s(&scene_clock,&scene_now);
#else
    localtime_r(&scene_now,&scene_clock);
#endif
    theme_now=scene_now;
    const tt_theme_config_t *theme=atomic_load(&editing) && tt_theme_config_valid(&draft.theme)?&draft.theme:&settings->theme;
    tt_theme_time(scene_now,theme);
    tt_scene_t scene=tt_scene_for_time(scene_clock.tm_mon+1,scene_clock.tm_hour,settings->precise_location && settings->latitude_e6<0);
    if (tt_theme_is_night()) scene.daylight=TT_NIGHT;
    else if (theme->mode==TT_THEME_DAY || scene.daylight==TT_NIGHT) scene.daylight=TT_DAY;
    tt_garden_weather(pet,current_weather);
    tt_garden_environment(pet,scene);
    bool night=tt_theme_is_night();
    /* LVGL style setters invalidate even when the value is unchanged. */
    if (home_palette!=(int)night) {
        home_palette=(int)night;
        tt_theme_bind(hero,LV_STYLE_BG_COLOR,night?0x101e34:0xf5fbf4,0);
        tt_theme_bind(hero,LV_STYLE_BG_GRAD_COLOR,night?0x101e34:0xf5fbf4,0);
        lv_obj_t *sky_labels[]={mode,time_text,date_text,temperature,weather_detail,weather_location,status};
        for (unsigned i=0;i<sizeof(sky_labels)/sizeof(sky_labels[0]);++i)
            tt_theme_bind(sky_labels[i],LV_STYLE_TEXT_COLOR,night?0xdce3da:TT_INK,0);
    }
    if (night && current_weather==TT_WEATHER_CLEAR) lv_obj_add_flag(weather_icon,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(weather_icon,LV_OBJ_FLAG_HIDDEN);

    if (atomic_load(&pet_open)) {
        if (timer_face || alarm) { atomic_store(&pet_open,false); lv_screen_load(home); }
        else { tt_garden_weather(tt_pet_ui_garden(),current_weather); tt_garden_environment(tt_pet_ui_garden(),scene); return; }
    }
    /* An alarm interrupts a settings form without discarding its unsaved fields. */
    if (alarm && atomic_load(&editing) && !alarm_editor) { alarm_editor=true; lv_screen_load(home); }
    if (!alarm && alarm_editor) { alarm_editor=false; lv_screen_load(editor); }
    if (atomic_load(&editing) && !alarm) {
        int result=atomic_exchange(&save_result,0);
        if (saving && result) {
            saving=false; if (save_cover) { lv_obj_delete(save_cover); save_cover=NULL; }
            if (result>0) leave();
            else { lv_obj_remove_state(next_button,LV_STATE_DISABLED); lv_obj_remove_state(back_button,LV_STATE_DISABLED);
                caption(next_button,T(wizard?"Finish":"Save")); error("Could not save settings"); }
        }
        if (rebuild_pending) { rebuild_pending=false; build_editor(); }
        if (atomic_load(&editing)) {
            if (page==PAGE_WEATHER) tt_location_ui_update(&s->lookup);
            if (page==PAGE_NETWORK) {
                bool connected=s->connected && draft.ssid[0] && !strcmp(s->ssid,draft.ssid);
                tt_network_ui_update(s);
                if (wizard && !saving) caption(next_button,T(connected?"Next":"Continue offline"));
            }
            if (page==PAGE_MOTION) {
                tt_label_set_text(calibration_status,T(cached.timer.calibrated?"Calibration complete":"Calibration needed for flip timers"));
                if (!feedback_error) tt_label_set_text(feedback,T(notice));
                if (wizard && !saving) caption(next_button,T(cached.timer.calibrated?"Finish":"Calibrate later"));
            }
            return;
        }
    }
    if (language!=settings->language) { language=settings->language; home_language(); }
    focus_active=timer_face || alarm; focus_idle=t->state==TT_IDLE;
    if (focus_active) {
        lv_obj_remove_flag(focus_layer,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(home_content,LV_OBJ_FLAG_HIDDEN);
        tt_garden_companion_visible(pet,false);
        lv_obj_add_flag(hero,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(care_button,LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(focus_layer,LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(home_content,LV_OBJ_FLAG_HIDDEN);
        tt_garden_companion_visible(pet,true); lv_obj_remove_flag(hero,LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(care_button,LV_OBJ_FLAG_HIDDEN);
    }
    if (alarm && lv_tick_get()<achievement_until) lv_obj_remove_flag(achievement,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(achievement,LV_OBJ_FLAG_HIDDEN);
    if (alarm) lv_obj_add_flag(settings_button,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(settings_button,LV_OBJ_FLAG_HIDDEN);
    time_t now=scene_now; struct tm local;
#ifdef _WIN32
    localtime_s(&local,&now);
#else
    localtime_r(&now,&local);
#endif
    char value[128],date[64];
    if (!focus_active) {
        snprintf(value,sizeof(value),"--:--"); snprintf(date,sizeof(date),"%s",T("Waiting for time sync"));
        if (now>=1700000000) {
            strftime(value,sizeof(value),"%H:%M",&local);
            if (language==TT_ZH) { const char *days[]={"日","一","二","三","四","五","六"};
                snprintf(date,sizeof(date),"%d月%d日  星期%s",local.tm_mon+1,local.tm_mday,days[local.tm_wday]);
            } else strftime(date,sizeof(date),"%a, %d %b %Y",&local);
        }
        tt_label_set_text(date_text,date); tt_label_set_text(time_text,value);
    } else {
        tt_focus_render(focus_layer,t,face,settings,alarm);
    }
    if (t->state==TT_RUNNING || t->state==TT_PAUSED) {
        unsigned seconds=(unsigned)((t->remaining_ms+999)/1000);
        snprintf(value,sizeof(value),"%s  ·  %02u:%02u",T(states[t->state]),seconds/60,seconds%60); tt_label_set_text(detail,value);
    } else tt_label_set_text(detail,T(!settings->timer.calibrated?"Open Settings to calibrate":"Flip to start your preset"));
    bool show_remaining=t->state==TT_RUNNING || t->state==TT_PAUSED;
    int appearance=(night?2:0)+(show_remaining?1:0);
    if (detail_style!=appearance) {
        detail_style=appearance;
        lv_obj_set_style_text_font(detail,show_remaining?&tt_font_24:&tt_font_16,0);
        lv_obj_set_style_bg_opa(detail,show_remaining?220:0,0);
        lv_obj_set_style_pad_all(detail,show_remaining?10:0,0);
        tt_theme_bind(detail,LV_STYLE_TEXT_COLOR,night && !show_remaining?0xdce3da:TT_INK,0);
    }

    tt_garden_set(pet,(tt_pet_t)settings->pet,alarm?2:t->state==TT_RUNNING?1:0,language,&pet_book.pets[settings->pet]);
    if (s->weather_valid) {
        snprintf(value,sizeof(value),"%.1f C",(double)s->temperature); tt_label_set_text(temperature,value); tt_theme_weather_set(weather_icon,s->weather_code);
        snprintf(value,sizeof(value),"%s  %d%%",T(tt_weather_text(s->weather_code)),s->humidity); tt_label_set_text(weather_detail,value);
        tt_label_set_text(weather_location,T(s->location));
        bool stale=!s->simulated && (!s->connected||now-s->updated>1800);
        snprintf(value,sizeof(value),"%sOpen-Meteo%s%s",s->simulated?T("Sample weather / "):"",stale?" / ":"",stale?T("cached"):""); tt_label_set_text(weather_credit,value);
    } else { tt_label_set_text(temperature,"-- C"); tt_theme_weather_set(weather_icon,-1);
        tt_label_set_text(weather_detail,T("No weather yet")); tt_label_set_text(weather_location,T(s->message)); tt_label_set_text(weather_credit,""); }
    if (strcmp(last_notice,notice)) {
        snprintf(last_notice,sizeof(last_notice),"%s",notice); notice_since=lv_tick_get();
    }
    bool important=strstr(notice,"unavailable") || strstr(notice,"Could not") || !settings->timer.calibrated;
    tt_label_set_text(status,*notice && (important || lv_tick_elaps(notice_since)<5000)?T(notice):
        T(alarm?"A well-earned rest":t->state==TT_RUNNING?"Let's focus!":"Tap ground to walk; tap buddy to pet"));
    bool show_status=*notice && (important || lv_tick_elaps(notice_since)<5000);
    if (show_status) { lv_obj_add_flag(detail,LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(status,LV_OBJ_FLAG_HIDDEN); }
    else { lv_obj_remove_flag(detail,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(status,LV_OBJ_FLAG_HIDDEN); }
}

