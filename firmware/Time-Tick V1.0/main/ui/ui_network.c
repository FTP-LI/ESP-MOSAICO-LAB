// SPDX-License-Identifier: GPL-3.0-only
#include "ui_network.h"
#include "ui_theme.h"
#include "ui_text.h"
#include <stdio.h>
#include <string.h>

static tt_settings_t *draft;
static tt_settings_t before_join;
static tt_ui_submit_t submit;
static void *context;
static lv_obj_t *root, *list, *scan_label, *connection, *refresh;
static lv_obj_t *modal, *name_field, *pass_field, *keys, *modal_status, *join;
static tt_services_view_t snapshot;
static bool manual, secured, attempted, waiting, close_pending, scan_pending;
static bool modal_attempted;
static uint32_t generation;
static tt_net_state_t listed_state;
static char listed_ssid[33];
static char selected[33];
#define T(s) tt_text(draft->language,s)

/* Five rows: digits always visible, then QWERTY, shift/symbols/space.
 * Mode names are LVGL's built-in keyboard commands. */
static const char *const lower[]={
    "1","2","3","4","5","6","7","8","9","0","\n",
    "q","w","e","r","t","y","u","i","o","p","\n",
    "a","s","d","f","g","h","j","k","l",LV_SYMBOL_BACKSPACE,"\n",
    "ABC","z","x","c","v","b","n","m",".","\n",
    "1#","_","-"," ",LV_SYMBOL_LEFT,LV_SYMBOL_RIGHT,LV_SYMBOL_OK,""};
static const char *const upper[]={
    "1","2","3","4","5","6","7","8","9","0","\n",
    "Q","W","E","R","T","Y","U","I","O","P","\n",
    "A","S","D","F","G","H","J","K","L",LV_SYMBOL_BACKSPACE,"\n",
    "abc","Z","X","C","V","B","N","M",".","\n",
    "1#","_","-"," ",LV_SYMBOL_LEFT,LV_SYMBOL_RIGHT,LV_SYMBOL_OK,""};
static const char *const symbols[]={
    "1","2","3","4","5","6","7","8","9","0","\n",
    "!","@","#","$","%","^","&","*","(",")","\n",
    "~","`","+","=","{","}","[","]","|",LV_SYMBOL_BACKSPACE,"\n",
    "abc","\\",";",":","'","\"",",","<",">","\n",
    "ABC","/","?"," ","_","-",LV_SYMBOL_OK,""};
static const lv_buttonmatrix_ctrl_t key_widths[]={
    1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,
    2,1,1,1,1,1,1,1,1,
    2,1,1,4,1,1,2};

static void style(lv_obj_t *obj,uint32_t bg)
{
    tt_theme_bind(obj,LV_STYLE_BG_COLOR,bg,0);
    tt_theme_bind(obj,LV_STYLE_BORDER_COLOR,0xd8c8a4,0);
    lv_obj_set_style_border_width(obj,1,0); lv_obj_set_style_radius(obj,10,0);
    lv_obj_set_style_shadow_width(obj,0,0); tt_theme_bind(obj,LV_STYLE_TEXT_COLOR,TT_INK,0);
}
static lv_obj_t *text(lv_obj_t *parent,const char *value,int x,int y,int w)
{
    lv_obj_t *l=lv_label_create(parent); tt_label_set_text(l,value);
    lv_obj_set_pos(l,x,y); lv_obj_set_width(l,w); lv_label_set_long_mode(l,LV_LABEL_LONG_DOT); return l;
}
static lv_obj_t *button(lv_obj_t *parent,const char *value,int x,int y,int w,lv_event_cb_t cb,intptr_t data)
{
    lv_obj_t *b=lv_button_create(parent); style(b,TT_CREAM); lv_obj_set_pos(b,x,y); lv_obj_set_size(b,w,42);
    tt_theme_bind(b,LV_STYLE_BG_COLOR,0xd6edbe,LV_STATE_PRESSED);
    lv_obj_t *l=text(b,value,0,0,w-16); lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0); lv_obj_center(l);
    lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,(void *)data); return b;
}
static void modal_close(void)
{
    if (modal) lv_obj_delete(modal);
    modal=name_field=pass_field=keys=modal_status=join=NULL; waiting=false; close_pending=false; modal_attempted=false;
}
void tt_network_ui_close(void)
{
    modal_close(); root=NULL; attempted=false; /* parent owns the page's remaining children */
}
bool tt_network_ui_attempted(void) { return attempted; }
static void dismiss(lv_event_t *e)
{
    (void)e;
    if (modal_attempted) {
        tt_command_t c={.type=TT_CMD_NETWORK,.settings=before_join};
        if (!submit(&c,context)) { tt_label_set_text(modal_status,T("Busy; please try again")); return; }
        *draft=before_join;
    }
    close_pending=true;
}
static void scan(lv_event_t *e)
{
    (void)e;
    tt_command_t c={.type=TT_CMD_SCAN};
    if (submit(&c,context)) { scan_pending=true; tt_label_set_text(scan_label,T("Scanning nearby networks...")); }
    else tt_label_set_text(scan_label,T("Busy; please try again"));
}
static void join_network(lv_event_t *e)
{
    (void)e; if (waiting) return;
    const char *name=manual?lv_textarea_get_text(name_field):selected;
    const char *pass=lv_textarea_get_text(pass_field); size_t n=strlen(pass);
    const char *err=NULL;
    if (!name[0]) err="Enter a Wi-Fi name first";
    else if (strlen(name)>32 || n>64) err="Wi-Fi name or password is too long";
    else if ((secured && !n) || (n && (n<8 || (n==64 && strspn(pass,"0123456789abcdefABCDEF")!=64))))
        err="Use 8-63 characters or 64 hex digits";
    if (err) { tt_label_set_text(modal_status,T(err)); return; }
    tt_command_t c={.type=TT_CMD_NETWORK,.settings=*draft};
    snprintf(c.settings.ssid,sizeof(c.settings.ssid),"%s",name);
    snprintf(c.settings.password,sizeof(c.settings.password),"%s",pass);
    if (!submit(&c,context)) { tt_label_set_text(modal_status,T("Busy; please try again")); return; }
    *draft=c.settings; attempted=true; modal_attempted=true; waiting=true;
    lv_obj_add_state(join,LV_STATE_DISABLED);
    tt_label_set_text(modal_status,T("Connecting..."));
}
static void keyboard_event(lv_event_t *e)
{
    if (lv_event_get_code(e)==LV_EVENT_READY) join_network(e);
}
static void field_focus(lv_event_t *e)
{
    lv_keyboard_set_textarea(keys,lv_event_get_target_obj(e));
}
static void password_visibility(lv_event_t *e)
{
    lv_textarea_set_password_mode(pass_field,!lv_obj_has_state(lv_event_get_target_obj(e),LV_STATE_CHECKED));
}
static lv_obj_t *input(int y,const char *value,const char *placeholder,int max)
{
    lv_obj_t *obj=lv_textarea_create(modal); tt_theme_input(obj); style(obj,TT_CREAM);
    lv_obj_set_pos(obj,20,y); lv_obj_set_size(obj,416,44); lv_textarea_set_one_line(obj,true);
    lv_textarea_set_max_length(obj,max); lv_textarea_set_text(obj,value); lv_textarea_set_placeholder_text(obj,T(placeholder));
    lv_obj_add_event_cb(obj,field_focus,LV_EVENT_FOCUSED,NULL); return obj;
}
static void select_network(lv_event_t *e)
{
    int index=(int)(intptr_t)lv_event_get_user_data(e);
    manual=index<0; if (!manual && index>=snapshot.ap_count) return;
    before_join=*draft; modal_attempted=false;
    secured=!manual && snapshot.aps[index].secured;
    snprintf(selected,sizeof(selected),"%s",manual?"":snapshot.aps[index].ssid);
    modal=lv_obj_create(lv_obj_get_screen(root)); style(modal,TT_PAPER);
    lv_obj_set_style_pad_all(modal,0,0); lv_obj_set_pos(modal,12,12); lv_obj_set_size(modal,456,456);
    lv_obj_remove_flag(modal,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *h=text(modal,T(manual?"Other network":"Join this network"),20,12,416);
    lv_obj_set_style_text_font(h,&tt_font_24,0);
    if (manual) name_field=input(50,"","Wi-Fi name",32);
    else { h=text(modal,selected,20,50,416); tt_theme_bind(h,LV_STYLE_TEXT_COLOR,TT_GREEN,0); }
    const char *saved=(!strcmp(selected,draft->ssid) && secured)?draft->password:"";
    pass_field=input(98,saved,secured?"Wi-Fi password":"Open network: no password",64);
    lv_textarea_set_password_mode(pass_field,true);
    if (!secured && !manual) lv_obj_add_state(pass_field,LV_STATE_DISABLED);
    lv_obj_t *show=lv_checkbox_create(modal); tt_theme_check(show); lv_obj_set_pos(show,20,150); lv_checkbox_set_text(show,T("Show password"));
    lv_obj_add_event_cb(show,password_visibility,LV_EVENT_VALUE_CHANGED,NULL);
    modal_status=text(modal,T(secured?"Password stays on your device":"Leave empty for an open network"),20,181,416);
    lv_obj_set_style_text_font(modal_status,&tt_font_16,0);
    button(modal,T("Cancel"),20,208,196,dismiss,0);
    join=button(modal,T("Connect"),240,208,196,join_network,0);
    tt_theme_bind(join,LV_STYLE_BG_COLOR,0xd6edbe,0);
    keys=lv_keyboard_create(modal); style(keys,0xe7ddc2);
    lv_obj_set_style_pad_all(keys,5,0); lv_obj_set_style_pad_row(keys,5,0); lv_obj_set_style_pad_column(keys,4,0);
    lv_obj_set_style_radius(keys,5,LV_PART_ITEMS); lv_obj_set_style_text_font(keys,&tt_font_18,LV_PART_ITEMS);
    tt_theme_bind(keys,LV_STYLE_BG_COLOR,TT_CREAM,LV_PART_ITEMS);
    tt_theme_bind(keys,LV_STYLE_TEXT_COLOR,TT_INK,LV_PART_ITEMS);
    tt_theme_bind(keys,LV_STYLE_BG_COLOR,0xc1e6a8,LV_PART_ITEMS|LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(keys,0,LV_PART_ITEMS);
    lv_obj_set_align(keys,LV_ALIGN_TOP_LEFT); lv_obj_set_pos(keys,0,260); lv_obj_set_size(keys,456,196);
    lv_keyboard_set_map(keys,LV_KEYBOARD_MODE_TEXT_LOWER,lower,key_widths);
    lv_keyboard_set_map(keys,LV_KEYBOARD_MODE_TEXT_UPPER,upper,key_widths);
    lv_keyboard_set_map(keys,LV_KEYBOARD_MODE_SPECIAL,symbols,key_widths);
    lv_keyboard_set_mode(keys,LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(keys,manual?name_field:pass_field);
    lv_obj_add_event_cb(keys,keyboard_event,LV_EVENT_READY,NULL);
    if (!manual && !secured) lv_obj_add_flag(keys,LV_OBJ_FLAG_HIDDEN);
}
void tt_network_ui_create(lv_obj_t *parent,tt_settings_t *settings,tt_ui_submit_t callback,void *ctx)
{
    draft=settings; submit=callback; context=ctx; attempted=false; generation=UINT32_MAX; scan_pending=false;
    listed_state=TT_NET_IDLE; listed_ssid[0]=0;
    root=lv_obj_create(parent); lv_obj_remove_style_all(root); lv_obj_set_size(root,LV_PCT(100),244);
    scan_label=text(root,T("Scanning nearby networks..."),0,0,286);
    lv_obj_set_style_text_font(scan_label,&tt_font_16,0);
    refresh=button(root,T("Refresh"),310,0,96,scan,0); lv_obj_set_height(refresh,32);
    connection=text(root,T("Select a network below"),0,33,408); lv_obj_set_style_text_font(connection,&tt_font_16,0);
    list=lv_obj_create(root); lv_obj_remove_style_all(list); lv_obj_set_pos(list,0,57); lv_obj_set_size(list,408,144);
    lv_obj_set_flex_flow(list,LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(list,5,0);
    lv_obj_set_scroll_dir(list,LV_DIR_VER); lv_obj_set_scrollbar_mode(list,LV_SCROLLBAR_MODE_AUTO);
    lv_obj_t *other=button(root,T("Other network"),0,208,196,select_network,-1); lv_obj_set_height(other,34);
    lv_obj_t *note=text(root,T("Offline is fine, too"),210,220,198); lv_obj_set_style_text_font(note,&tt_font_16,0);
    scan(NULL);
}
void tt_network_ui_update(const tt_services_view_t *view)
{
    if (!root) return;
    if (close_pending) modal_close();
    snapshot=*view; if (snapshot.ap_count>TT_WIFI_MAX_APS) snapshot.ap_count=TT_WIFI_MAX_APS;
    if (view->scan_state==TT_SCAN_RUNNING || view->scan_state==TT_SCAN_DONE || view->scan_state==TT_SCAN_FAILED) scan_pending=false;
    tt_label_set_text(scan_label,T(scan_pending || view->scan_state==TT_SCAN_RUNNING?"Scanning nearby networks...":
        view->scan_state==TT_SCAN_FAILED?"Scan failed; tap Refresh":snapshot.ap_count?(view->simulated?"Nearby networks / demo":"Nearby networks"):"No networks found; try Refresh"));
    if (view->scan_state==TT_SCAN_RUNNING || scan_pending) lv_obj_add_state(refresh,LV_STATE_DISABLED);
    else lv_obj_remove_state(refresh,LV_STATE_DISABLED);
    bool connected=view->connected && draft->ssid[0] && !strcmp(view->ssid,draft->ssid);
    char state_text[160];
    snprintf(state_text,sizeof(state_text),"%s%s%s",
        T(view->connected?(view->simulated?"Connected / simulated network":"Connected"):
            view->network_message[0]?view->network_message:"Select a network below"),
        view->ssid[0]?" · ":"",view->ssid);
    tt_label_set_text(connection,state_text);
    tt_theme_bind(connection,LV_STYLE_TEXT_COLOR,view->connected?TT_GREEN:TT_INK,0);
    if (!modal && (generation!=view->scan_generation || listed_state!=view->network_state ||
                   strcmp(listed_ssid,view->ssid))) {
        generation=view->scan_generation; listed_state=view->network_state;
        snprintf(listed_ssid,sizeof(listed_ssid),"%s",view->ssid);
        lv_obj_clean(list);
        for (int i=0;i<snapshot.ap_count;++i) {
            const tt_wifi_ap_t *ap=&snapshot.aps[i];
            lv_obj_t *b=button(list,ap->ssid,0,0,402,select_network,i); lv_obj_set_height(b,59);
            lv_obj_set_style_pad_all(b,8,0);
            lv_obj_t *name=lv_obj_get_child(b,0); lv_obj_set_width(name,314); lv_obj_set_align(name,LV_ALIGN_TOP_LEFT); lv_obj_set_pos(name,0,0);
            lv_obj_set_style_text_align(name,LV_TEXT_ALIGN_LEFT,0);
            bool current=view->connected && !strcmp(ap->ssid,view->ssid);
            if (current) tt_theme_bind(b,LV_STYLE_BG_COLOR,0xd6edbe,0);
            char detail[80]; snprintf(detail,sizeof(detail),"%s  /  %d dBm",T(current?"Connected":ap->secured?"Secured":"Open"),ap->rssi);
            lv_obj_t *d=text(b,detail,0,25,314); lv_obj_set_style_text_font(d,&tt_font_16,0);
            tt_theme_bind(d,LV_STYLE_TEXT_COLOR,TT_MUTED,0);
            int bars=ap->rssi>-55?4:ap->rssi>-67?3:ap->rssi>-78?2:1;
            for (int k=0;k<4;++k) {
                lv_obj_t *bar=lv_obj_create(b); lv_obj_remove_style_all(bar); lv_obj_set_pos(bar,343+k*8,30-k*5);
                lv_obj_set_size(bar,5,7+k*5); lv_obj_set_style_radius(bar,2,0); lv_obj_set_style_bg_opa(bar,LV_OPA_COVER,0);
                tt_theme_bind(bar,LV_STYLE_BG_COLOR,k<bars?TT_GREEN:0xdcd9c3,0);
            }
        }
    }
    if (modal && waiting) {
        if (connected) modal_close();
        else if (view->network_state==TT_NET_FAILED && (!view->ssid[0] || !strcmp(view->ssid,draft->ssid))) {
            waiting=false; lv_obj_remove_state(join,LV_STATE_DISABLED); tt_label_set_text(modal_status,T(view->network_message));
        }
    }
}
