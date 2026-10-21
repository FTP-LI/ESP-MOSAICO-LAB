// SPDX-License-Identifier: GPL-3.0-only
#include "ui_theme.h"
#include "weather_model.h"
#include <string.h>

typedef struct theme_color {
    struct theme_color *next;
    lv_style_prop_t property;
    lv_style_selector_t selector;
    uint32_t day;
} theme_color_t;
typedef struct theme_object {
    struct theme_object *next;
    lv_obj_t *obj;
    theme_color_t *colors;
} theme_object_t;
static theme_object_t *objects;
static bool night;

bool tt_theme_is_night(void) { return night; }

uint32_t tt_theme_color(lv_style_prop_t property,uint32_t day)
{
    if (!night) return day;
    bool text=property==LV_STYLE_TEXT_COLOR;
    bool surface=property==LV_STYLE_BG_COLOR || property==LV_STYLE_BG_GRAD_COLOR;
    if (property==LV_STYLE_SHADOW_COLOR) return 0x050c12;
    if (text) {
        switch (day) {
        case TT_INK: case 0x304d3d: case 0x586556: return 0xdce3da;
        case 0x66553d: case 0x66502d: return 0xe4d9bd;
        case TT_GREEN: case 0x5b8664: return 0xa4c5ab;
        case TT_MUTED: case 0x80927d: case 0x87977b: return 0xa5b5b2;
        case 0xa5ac99: return 0x899b99;
        case 0xaa844c: case 0xb88c38: return 0xdabb84;
        case 0xb94349: case 0xa94449: return 0xf0a09a;
        case TT_CREAM: case 0xffffff: return 0xdce3da;
        default: return day;
        }
    }
    switch (day) {
    case TT_PAPER: return 0x121f29;
    case 0xf5fbf4: return 0x101e34;
    case 0xeef1df: return 0x1b2d2d;
    case TT_CREAM: case 0xfffdf5: return 0x20313b;
    case 0xffffff: return 0x405354;
    case TT_WOOD: case 0xe7ddc2: return 0x15242e;
    case 0xe5ecd7: case 0xf0efe4: return 0x293e40;
    case 0xbce294: case 0xc4e7ac: case 0xd6edbe: case 0xc1e6a8:
    case 0xc8ddaf: case 0xdce9ca: case 0xe2ecd1: return 0x3a574b;
    case TT_GREEN: return surface?0x496f59:0x94b99d;
    case 0xb77537: return 0xc49a60;
    case 0xffc85b: return 0xc7a56b;
    case 0xd8dac6: case 0xd8c8a4: case 0xc8d3b8: return 0x40534f;
    case 0xd2a766: return 0x8c876c;
    case 0xe8d6ae: case 0xe2d2ad: return 0x544c38;
    case 0xdcd9c3: case 0xe4e5d2: case 0xdce3d2: case 0xcfe5f2: return 0x31474a;
    /* Countdown states retain distinct green / amber accents. */
    case 0xf6f8ef: return 0x152a27;
    case 0xf9f5ec: return 0x2b2924;
    case 0xfff8e6: return 0x312c22;
    case 0xf5f6f1: return 0x1c282d;
    case 0x5b8664: return 0x93ba9e;
    case 0xaa844c: return 0xc5a573;
    case 0xb88c38: return 0xd7b56b;
    case 0x87977b: return 0x92a99d;
    case 0xdce6d5: case 0xe0e5d9: return 0x354c48;
    case 0xe8ddc8: case 0xead9ae: return 0x514a38;
    case 0xc3d4b4: case 0xd1d9c4: return 0x516a5e;
    case 0xd7c8a9: case 0xdccb9c: return 0x76674a;
    case 0xeaf1e3: case 0xebeee5: return 0x2c443e;
    case 0xf2e8d5: case 0xf7ebc9: return 0x473e2e;
    default: return day;
    }
}

static void apply(theme_object_t *owner,theme_color_t *color)
{
    lv_style_value_t value={.color=lv_color_hex(tt_theme_color(color->property,color->day))},old;
    if (lv_obj_get_local_style_prop(owner->obj,color->property,&old,color->selector)==LV_STYLE_RES_FOUND &&
        lv_color_eq(old.color,value.color)) return;
    lv_obj_set_local_style_prop(owner->obj,color->property,value,color->selector);
}
static void deleted(lv_event_t *e)
{
    theme_object_t *owner=lv_event_get_user_data(e),**link=&objects;
    /* Child delete events must never release the parent's bindings. */
    if (lv_event_get_target(e)!=owner->obj) return;
    while (*link && *link!=owner) link=&(*link)->next;
    if (*link) *link=owner->next;
    while (owner->colors) { theme_color_t *c=owner->colors; owner->colors=c->next; lv_free(c); }
    lv_free(owner);
}
void tt_theme_bind(lv_obj_t *obj,lv_style_prop_t property,uint32_t day,lv_style_selector_t selector)
{
    theme_object_t *owner=NULL;
    for (uint32_t i=0;i<lv_obj_get_event_count(obj);++i) {
        lv_event_dsc_t *event=lv_obj_get_event_dsc(obj,i);
        if (lv_event_dsc_get_cb(event)==deleted) { owner=lv_event_dsc_get_user_data(event); break; }
    }
    if (!owner) {
        owner=lv_malloc(sizeof(*owner)); LV_ASSERT_MALLOC(owner);
        *owner=(theme_object_t){.obj=obj,.next=objects}; objects=owner;
        lv_obj_add_event_cb(obj,deleted,LV_EVENT_DELETE,owner);
    }
    theme_color_t *color=owner->colors;
    while (color && (color->property!=property || color->selector!=selector)) color=color->next;
    if (!color) {
        color=lv_malloc(sizeof(*color)); LV_ASSERT_MALLOC(color);
        *color=(theme_color_t){.property=property,.selector=selector,.next=owner->colors}; owner->colors=color;
    }
    color->day=day; apply(owner,color);
}
bool tt_theme_time(time_t now,const tt_theme_config_t *config)
{
    unsigned minute=1440;
    if (now>=1700000000) {
        struct tm clock;
#ifdef _WIN32
        localtime_s(&clock,&now);
#else
        localtime_r(&now,&clock);
#endif
        minute=(unsigned)(clock.tm_hour*60+clock.tm_min);
    }
    bool next=tt_theme_night_at(config,minute,night);
    if (night==next) return false;
    night=next;
    /* Runs only at a boundary or timezone change. No rebuilds, extra timers,
     * display-frequency changes, image tinting or per-frame tree walks. */
    for (theme_object_t *o=objects;o;o=o->next)
        for (theme_color_t *c=o->colors;c;c=c->next) apply(o,c);
    return true;
}
void tt_theme_input(lv_obj_t *obj)
{
    tt_theme_bind(obj,LV_STYLE_TEXT_COLOR,TT_MUTED,LV_PART_TEXTAREA_PLACEHOLDER);
    tt_theme_bind(obj,LV_STYLE_BG_COLOR,TT_GREEN,LV_PART_CURSOR);
    tt_theme_bind(obj,LV_STYLE_BORDER_COLOR,TT_GREEN,LV_PART_CURSOR);
    tt_theme_bind(obj,LV_STYLE_BORDER_COLOR,TT_GREEN,LV_STATE_FOCUSED);
}
void tt_theme_check(lv_obj_t *obj)
{
    tt_theme_bind(obj,LV_STYLE_BG_COLOR,TT_CREAM,LV_PART_INDICATOR);
    tt_theme_bind(obj,LV_STYLE_BG_COLOR,TT_GREEN,LV_PART_INDICATOR|LV_STATE_CHECKED);
    tt_theme_bind(obj,LV_STYLE_BORDER_COLOR,0xd8dac6,LV_PART_INDICATOR);
    tt_theme_bind(obj,LV_STYLE_BORDER_COLOR,TT_GREEN,LV_PART_INDICATOR|LV_STATE_CHECKED);
    tt_theme_bind(obj,LV_STYLE_BG_COLOR,0xc4e7ac,LV_PART_INDICATOR|LV_STATE_PRESSED);
    tt_theme_bind(obj,LV_STYLE_TEXT_COLOR,TT_CREAM,LV_PART_INDICATOR|LV_STATE_CHECKED);
}
void tt_theme_dropdown(lv_obj_t *list)
{
    tt_theme_bind(list,LV_STYLE_BG_COLOR,0xbce294,LV_PART_SELECTED|LV_STATE_CHECKED);
    tt_theme_bind(list,LV_STYLE_BG_COLOR,0xc8ddaf,LV_PART_SELECTED|LV_STATE_PRESSED);
    tt_theme_bind(list,LV_STYLE_TEXT_COLOR,TT_INK,LV_PART_SELECTED);
}

void tt_label_set_text(lv_obj_t *label,const char *text)
{
    if (strcmp(lv_label_get_text(label),text)) lv_label_set_text(label,text);
}

static lv_obj_t *drawing(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
void tt_theme_backdrop(lv_obj_t *screen)
{
    tt_theme_bind(screen,LV_STYLE_BG_COLOR,TT_PAPER,0);
    tt_theme_bind(screen,LV_STYLE_BG_GRAD_COLOR,0xeef1df,0);
    lv_obj_set_style_bg_grad_dir(screen,LV_GRAD_DIR_VER,0);
}

static void round_shape(lv_layer_t *l,int x,int y,int w,int h,int radius,uint32_t color)
{
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color=lv_color_hex(color); d.radius=radius;
    lv_area_t a={x,y,x+w-1,y+h-1}; lv_draw_rect(l,&d,&a);
}
static void weather_event(lv_event_t *e)
{
    lv_obj_t *obj=lv_event_get_target_obj(e);
    int code=(int)(intptr_t)lv_obj_get_user_data(obj);
    tt_weather_kind_t kind=tt_weather_kind(code);
    lv_area_t a; lv_obj_get_coords(obj,&a); lv_layer_t *l=lv_event_get_layer(e);
    int x=a.x1,y=a.y1;
    if (kind==TT_WEATHER_CLEAR || code==2) {
        round_shape(l,x+5,y+5,33,33,17,0xffe1a0); round_shape(l,x+9,y+9,25,25,13,0xf4b84a);
        round_shape(l,x+13,y+11,16,14,8,0xffcf70);
    }
    if (kind!=TT_WEATHER_CLEAR) {
        round_shape(l,x+6,y+26,40,17,9,0xa2b8b1);
        round_shape(l,x+10,y+19,21,21,11,0xe0e9e1);
        round_shape(l,x+22,y+15,21,25,11,0xf4f6eb);
        round_shape(l,x+8,y+29,38,10,5,0xf4f6eb);
    }
    if (kind==TT_WEATHER_FOG) {
        round_shape(l,x+5,y+44,36,2,1,0xa2b8b1); round_shape(l,x+12,y+48,32,2,1,0xa2b8b1);
    } else if (kind==TT_WEATHER_STORM) {
        round_shape(l,x+24,y+35,5,8,2,0xefb343); round_shape(l,x+20,y+41,8,3,1,0xefb343);
        round_shape(l,x+20,y+43,4,6,1,0xefb343);
    } else if (kind==TT_WEATHER_RAIN || kind==TT_WEATHER_SNOW) {
        bool snow=kind==TT_WEATHER_SNOW;
        for (int i=0;i<3;++i) round_shape(l,x+12+i*12,y+43,3,snow?3:7,2,snow?0x9eaea7:0x73aabd);
    }
}
lv_obj_t *tt_theme_weather(lv_obj_t *parent, int x, int y)
{
    lv_obj_t *obj = drawing(parent, x, y, 50, 50);
    lv_obj_set_user_data(obj, (void *)(intptr_t)-1);
    lv_obj_add_event_cb(obj, weather_event, LV_EVENT_DRAW_MAIN, NULL);
    return obj;
}
void tt_theme_weather_set(lv_obj_t *obj, int code)
{
    if ((int)(intptr_t)lv_obj_get_user_data(obj) == code) return;
    lv_obj_set_user_data(obj, (void *)(intptr_t)code);
    lv_obj_invalidate(obj);
}

