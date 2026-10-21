// SPDX-License-Identifier: GPL-3.0-only
#include "ui_location.h"
#include "ui_theme.h"
#include "ui_text.h"
#include <stdio.h>
#include <string.h>

static tt_settings_t *draft;
static tt_ui_submit_t submit;
static void *context;
static lv_obj_t *root,*summary,*modal,*status,*results,*input,*keys,*name_field,*lat_field,*lon_field;
static tt_location_view_t shown;
static uint32_t next_id,waiting_id,deadline;
static bool searching,advanced,close_pending;
#define T(s) tt_text(draft->language,s)
static void request(tt_location_kind_t kind,const char *query);

static void style(lv_obj_t *o,uint32_t color)
{
    tt_theme_bind(o,LV_STYLE_BG_COLOR,color,0);
    lv_obj_set_style_bg_opa(o,255,0); tt_theme_bind(o,LV_STYLE_BORDER_COLOR,0xd8dac6,0);
    lv_obj_set_style_border_width(o,1,0); lv_obj_set_style_radius(o,12,0);
    lv_obj_set_style_shadow_width(o,0,0); lv_obj_set_style_text_font(o,&tt_font_16,0);
    tt_theme_bind(o,LV_STYLE_TEXT_COLOR,TT_INK,0); lv_obj_set_style_pad_all(o,0,0);
}
static lv_obj_t *text(lv_obj_t *parent,const char *value,int x,int y,int w)
{
    lv_obj_t *o=lv_label_create(parent); lv_label_set_text(o,value);
    lv_obj_set_pos(o,x,y); lv_obj_set_width(o,w); return o;
}
static lv_obj_t *button(lv_obj_t *parent,const char *value,int x,int y,int w,int h,lv_event_cb_t cb,intptr_t data)
{
    lv_obj_t *o=lv_button_create(parent); style(o,0xe5ecd7); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    tt_theme_bind(o,LV_STYLE_BG_COLOR,0xbce294,LV_STATE_PRESSED);
    lv_obj_set_style_transform_width(o,0,LV_STATE_PRESSED); lv_obj_set_style_transform_height(o,0,LV_STATE_PRESSED);
    lv_obj_t *label=text(o,value,6,0,w-12); lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
    lv_label_set_long_mode(label,LV_LABEL_LONG_DOT); lv_obj_center(label);
    lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,(void *)data); return o;
}
static void update_summary(void)
{
    lv_label_set_text_fmt(summary,"%s\n%s",T("Current place"),
        draft->city[0]?draft->city:T("No place selected"));
}
static void dismiss(lv_event_t *e)
{
    (void)e; waiting_id=0;
    close_pending=true;
}
void tt_location_ui_close(void)
{
    waiting_id=0;
    if (modal) lv_obj_delete(modal);
    modal=NULL; root=NULL; summary=NULL; keys=NULL; input=NULL; advanced=false; close_pending=false;
}
static void hide_keys(void)
{
    if (!keys) return;
    lv_keyboard_set_textarea(keys,NULL); lv_obj_add_flag(keys,LV_OBJ_FLAG_HIDDEN);
    if (searching) lv_obj_set_height(results,230);
}
static void search(lv_event_t *e)
{
    (void)e;
    if (input) request(TT_LOCATION_SEARCH,lv_textarea_get_text(input));
}
static void key_event(lv_event_t *e)
{
    if (lv_event_get_code(e)==LV_EVENT_READY && searching) search(NULL);
    else if (lv_event_get_code(e)==LV_EVENT_READY || lv_event_get_code(e)==LV_EVENT_CANCEL) hide_keys();
}
static void focus(lv_event_t *e)
{
    lv_obj_t *field=lv_event_get_target_obj(e);
    lv_keyboard_set_mode(keys,lv_event_get_user_data(e)?LV_KEYBOARD_MODE_NUMBER:LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(keys,field); lv_obj_remove_flag(keys,LV_OBJ_FLAG_HIDDEN);
    if (searching) lv_obj_set_height(results,78);
}
static lv_obj_t *field(const char *value,int y,bool numeric)
{
    lv_obj_t *o=lv_textarea_create(modal); tt_theme_input(o); style(o,TT_CREAM); lv_obj_set_style_pad_all(o,8,0);
    lv_obj_set_pos(o,20,y); lv_obj_set_size(o,416,42); lv_textarea_set_one_line(o,true);
    lv_textarea_set_max_length(o,numeric?12:79); lv_textarea_set_text(o,value);
    if (numeric) lv_textarea_set_accepted_chars(o,"-0123456789.");
    lv_obj_add_event_cb(o,focus,LV_EVENT_FOCUSED,numeric?(void *)1:NULL); return o;
}
static void dialog(const char *title,bool search_mode)
{
    if (modal) lv_obj_delete(modal);
    waiting_id=0; searching=search_mode; advanced=false; input=NULL; keys=NULL; close_pending=false;
    modal=lv_obj_create(lv_obj_get_screen(root)); style(modal,TT_PAPER);
    lv_obj_set_pos(modal,12,12); lv_obj_set_size(modal,456,456);
    lv_obj_remove_flag(modal,LV_OBJ_FLAG_SCROLLABLE);
    button(modal,T("Back"),12,12,76,36,dismiss,0);
    text(modal,T(title),108,20,216);
    status=text(modal,"",20,60,416); lv_obj_set_height(status,48);
    results=lv_obj_create(modal); lv_obj_remove_style_all(results);
    lv_obj_set_pos(results,20,120); lv_obj_set_size(results,416,266);
    lv_obj_set_flex_flow(results,LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(results,8,0);
    lv_obj_set_style_text_font(results,&tt_font_16,0);
    if (search_mode) {
        input=field("",60,false); lv_obj_set_width(input,306);
        lv_textarea_set_placeholder_text(input,T("Pinyin / English, e.g. hangzhou"));
        button(modal,T("Search"),336,60,100,42,search,0);
        lv_obj_set_y(status,110); lv_obj_set_height(status,42);
        lv_obj_set_y(results,157); lv_obj_set_height(results,230);
    }
    keys=lv_keyboard_create(modal); style(keys,0xe7ddc2);
    lv_obj_set_pos(keys,0,246); lv_obj_set_size(keys,456,210);
    lv_obj_set_style_text_font(keys,&tt_font_16,LV_PART_ITEMS);
    tt_theme_bind(keys,LV_STYLE_BG_COLOR,TT_CREAM,LV_PART_ITEMS);
    tt_theme_bind(keys,LV_STYLE_BG_COLOR,0xc4e7ac,LV_PART_ITEMS|LV_STATE_PRESSED);
    tt_theme_bind(keys,LV_STYLE_TEXT_COLOR,TT_INK,LV_PART_ITEMS);
    lv_obj_add_flag(keys,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(keys,key_event,LV_EVENT_READY,NULL);
    lv_obj_add_event_cb(keys,key_event,LV_EVENT_CANCEL,NULL);
}
static void choose(lv_event_t *e)
{
    unsigned i=(unsigned)(uintptr_t)lv_event_get_user_data(e);
    if (i>=shown.count) return;
    const tt_location_t *p=&shown.places[i];
    snprintf(draft->city,sizeof(draft->city),"%s",p->name);
    draft->latitude_e6=p->latitude_e6; draft->longitude_e6=p->longitude_e6; draft->precise_location=true;
    update_summary(); dismiss(NULL);
}
static void draw_results(const tt_location_view_t *view)
{
    shown=*view; lv_obj_clean(results);
    const char *message=view->state==TT_LOCATION_LOADING?"Finding places...":
        view->state==TT_LOCATION_OFFLINE?"Connect Wi-Fi, then try again":
        view->state==TT_LOCATION_EMPTY?"No matches. Try pinyin or a nearby city":
        view->state==TT_LOCATION_READY?(view->kind==TT_LOCATION_IP?"Estimated from network. Tap to confirm":"Choose the matching city below"):
        "Lookup unavailable. Retry or choose another method";
    lv_label_set_text(status,T(message));
    for (unsigned i=0;i<view->count;++i) {
        const tt_location_t *p=&view->places[i];
        lv_obj_t *b=button(results,"",0,0,410,60,choose,(intptr_t)i);
        tt_theme_bind(b,LV_STYLE_BG_COLOR,TT_CREAM,0);
        lv_obj_t *name=lv_obj_get_child(b,0); lv_obj_set_align(name,LV_ALIGN_TOP_LEFT);
        lv_obj_set_pos(name,12,7); lv_obj_set_width(name,380); lv_obj_set_style_text_align(name,LV_TEXT_ALIGN_LEFT,0);
        lv_label_set_text(name,p->name);
        lv_obj_t *region=text(b,p->region,12,32,380); lv_label_set_long_mode(region,LV_LABEL_LONG_DOT);
        tt_theme_bind(region,LV_STYLE_TEXT_COLOR,TT_MUTED,0);
    }
}
static void request(tt_location_kind_t kind,const char *query)
{
    tt_command_t c={.type=TT_CMD_LOCATION};
    c.location=(tt_location_request_t){.id=++next_id,.kind=kind,.language=draft->language};
    if (!c.location.id) c.location.id=++next_id;
    snprintf(c.location.query,sizeof(c.location.query),"%s",query?query:"");
    char url[512];
    if (!tt_location_url(&c.location,url,sizeof(url))) {
        lv_label_set_text(status,T("Enter at least two letters, e.g. hangzhou")); return;
    }
    hide_keys(); waiting_id=c.location.id; deadline=lv_tick_get()+25000;
    tt_location_view_t view={.id=waiting_id,.kind=kind,.state=TT_LOCATION_LOADING};
    if (!submit(&c,context)) { view.state=TT_LOCATION_FAILED; waiting_id=0; }
    draw_results(&view);
}
static void retry(lv_event_t *e) { (void)e; request(TT_LOCATION_IP,NULL); }
static void auto_location(lv_event_t *e)
{
    (void)e; dialog("Locate my city",false);
    button(modal,T("Retry"),340,12,96,36,retry,0);
    text(modal,T("IP estimate can be affected by VPN or mobile networks."),20,400,416);
    request(TT_LOCATION_IP,NULL);
}
static void search_city(lv_event_t *e)
{
    (void)e; dialog("Search cities",true);
    lv_label_set_text(status,T("Type pinyin; results show province and country."));
}
static const char *const common_names[][2]={
    {"北京","Beijing"},{"上海","Shanghai"},{"广州","Guangzhou"},{"深圳","Shenzhen"},
    {"杭州","Hangzhou"},{"成都","Chengdu"},{"重庆","Chongqing"},{"武汉","Wuhan"},
    {"南京","Nanjing"},{"西安","Xi'an"},{"苏州","Suzhou"},{"天津","Tianjin"}
};
static void common_query(lv_event_t *e)
{
    unsigned i=(unsigned)(uintptr_t)lv_event_get_user_data(e);
    char query[80]; snprintf(query,sizeof(query),"%s, CN",common_names[i][1]);
    dialog("Choose a city",false); request(TT_LOCATION_SEARCH,query);
}
static void common(lv_event_t *e)
{
    (void)e; dialog("Popular cities",false);
    lv_label_set_text(status,T("Tap a city, then confirm its search result."));
    lv_obj_set_flex_flow(results,LV_FLEX_FLOW_ROW_WRAP); lv_obj_set_style_pad_column(results,8,0);
    for (unsigned i=0;i<12;++i)
        button(results,common_names[i][draft->language==TT_ZH?0:1],0,0,198,36,common_query,(intptr_t)i);
}
static void exact_save(lv_event_t *e)
{
    (void)e; int32_t lat,lon;
    if (!tt_weather_coordinates(lv_textarea_get_text(lat_field),lv_textarea_get_text(lon_field),&lat,&lon)) {
        lv_label_set_text(status,T("Enter valid WGS84 latitude / longitude")); return;
    }
    const char *name=lv_textarea_get_text(name_field);
    if (strlen(name)>=sizeof(draft->city)) return;
    snprintf(draft->city,sizeof(draft->city),"%s",*name?name:T("Selected location"));
    draft->latitude_e6=lat; draft->longitude_e6=lon; draft->precise_location=true;
    update_summary(); dismiss(NULL);
}
static void exact(lv_event_t *e)
{
    (void)e; dialog("Exact coordinates",false); advanced=true;
    lv_obj_add_flag(results,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(status,50); lv_label_set_text(status,"");
    button(modal,T("Use this place"),326,12,110,36,exact_save,0);
    text(modal,T("Location name / city"),20,76,400); name_field=field(draft->city,98,false);
    text(modal,T("Latitude (-90 to 90)"),20,142,400);
    char value[32]; snprintf(value,sizeof(value),"%.6f",draft->latitude_e6/1000000.0);
    lat_field=field(draft->precise_location?value:"",164,true);
    text(modal,T("Longitude (-180 to 180)"),20,208,400);
    snprintf(value,sizeof(value),"%.6f",draft->longitude_e6/1000000.0);
    lon_field=field(draft->precise_location?value:"",230,true);
    text(modal,T("Only needed when you already know the coordinates."),20,310,416);
    /* Inputs above the keyboard remain reachable; number entry can be closed
     * with the keyboard checkmark before confirming in the header. */
    lv_obj_set_pos(keys,0,284); lv_obj_set_size(keys,456,172); lv_obj_move_foreground(keys);
}
void tt_location_ui_create(lv_obj_t *parent,tt_settings_t *settings,tt_ui_submit_t callback,void *ctx)
{
    tt_location_ui_close(); draft=settings; submit=callback; context=ctx; root=parent;
    lv_obj_t *card=lv_obj_create(root); style(card,TT_CREAM); lv_obj_set_size(card,LV_PCT(100),54);
    summary=text(card,"",12,6,370); update_summary();
    button(root,T("Locate my city"),0,0,394,44,auto_location,0);
    lv_obj_t *row=lv_obj_create(root); lv_obj_remove_style_all(row); lv_obj_set_size(row,LV_PCT(100),40);
    button(row,T("Search cities"),0,0,192,40,search_city,0);
    button(row,T("Popular cities"),202,0,192,40,common,0);
    button(root,T("Advanced: coordinates"),0,0,394,30,exact,0);
    lv_obj_t *hint=lv_label_create(root); lv_label_set_text(hint,T("Choose a place, then save. No coordinates needed."));
    lv_obj_set_width(hint,LV_PCT(100)); tt_theme_bind(hint,LV_STYLE_TEXT_COLOR,TT_MUTED,0);
}
void tt_location_ui_update(const tt_location_view_t *view)
{
    if (close_pending) {
        if (modal) lv_obj_delete(modal);
        modal=NULL; keys=NULL; input=NULL; advanced=false; close_pending=false;
    }
    if (!root || !modal || advanced || !waiting_id) return;
    if (view->id==waiting_id && view->state!=shown.state) {
        draw_results(view);
        if (view->state!=TT_LOCATION_LOADING) waiting_id=0;
    } else if ((int32_t)(lv_tick_get()-deadline)>=0) {
        tt_location_view_t failed={.id=waiting_id,.kind=shown.kind,.state=TT_LOCATION_FAILED};
        waiting_id=0; draw_results(&failed);
    }
}
