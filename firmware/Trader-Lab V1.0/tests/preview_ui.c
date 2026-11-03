// Real firmware UI on headless LVGL. Host screenshots are NOT device evidence.
#include "../main/main.c"
#include <assert.h>
#include <sys/stat.h>
#include <direct.h>
#undef assert
#define assert(x) do { if(!(x)) { fprintf(stderr,"UI CHECK FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static uint32_t pixels[480*480],buffer[480*480];
static unsigned refresh_calls;
void tl_network_scan(void) {}
void tl_network_refresh(void) { refresh_calls++; }
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *data)
{
    for(int y=a->y1;y<=a->y2;y++) memcpy(pixels+y*480+a->x1,data+(y-a->y1)*(a->x2-a->x1+1)*4,(a->x2-a->x1+1)*4);
    lv_display_flush_ready(d);
}
static void capture_ui(const char *name)
{
    draw_ui();lv_refr_now(NULL);
    if(panel==INPUT) {
        bool found=false;
        for(unsigned i=0;i<lv_obj_get_child_count(root);i++) {
            lv_obj_t *o=lv_obj_get_child(root,i);
            if(lv_obj_check_type(o,&lv_keyboard_class)) {
                lv_area_t bounds; lv_obj_get_coords(o,&bounds);
                assert(bounds.x1>=0 && bounds.x2<480 && bounds.y1>=150 && bounds.y2<420); found=true;
            }
        }
        assert(found);
    }
    char path[256];snprintf(path,sizeof(path),"build-ui/screens/%s.bmp",name);
    FILE *f=fopen(path,"wb");assert(f);
    uint32_t header[13]={480*480*4+54,0,54,40,480,480,32u<<16|1,0,480*480*4,2835,2835,0,0};
    fwrite("BM",1,2,f);fwrite(header,4,13,f);
    for(int y=479;y>=0;y--)fwrite(pixels+y*480,4,480,f);
    fclose(f);
    printf("rendered %s\n",name);
}
static lv_obj_t *find_action(lv_obj_t *o,int id)
{
    for(unsigned i=0;i<lv_obj_get_event_count(o);i++) {
        lv_event_dsc_t *d=lv_obj_get_event_dsc(o,i);
        if(lv_obj_check_type(o,&lv_button_class) && lv_event_dsc_get_cb(d)==action && (intptr_t)lv_event_dsc_get_user_data(d)==id) return o;
    }
    for(unsigned i=0;i<lv_obj_get_child_count(o);i++){lv_obj_t *found=find_action(lv_obj_get_child(o,i),id);if(found)return found;}
    return NULL;
}
static void click(int id)
{
    lv_obj_t *o=find_action(root,id);assert(o);lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);draw_ui();
}
int main(void)
{
    _mkdir("build-ui/screens");lv_init();lv_display_t *d=lv_display_create(480,480);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_XRGB8888);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(d,flush);
    tl_defaults(&config);draft=config;
    capture_ui("01-offline");
    FILE *fixture=fopen("tests/fixtures.local/eastmoney.txt","rb");
    if(fixture){char body[4096]={0};fread(body,1,sizeof(body)-1,fixture);fclose(fixture);tl_quote_t parsed[12];int count=tl_parse_eastmoney(body,parsed,12);for(int j=0;j<count;j++)for(unsigned i=0;i<config.count;i++)if(!strcmp(parsed[j].code,config.watch[i].code)){network.quotes[i]=parsed[j];network.fetched_ok[i]=true;}}
    network.connected=true;network.clock_ok=true;
    snprintf(network.message,sizeof(network.message),"部分报价不可用");
    capture_ui("02-live-fixture");
    click(A_SETTINGS);capture_ui("03-settings");click(A_WIFI);capture_ui("04-wifi");click(A_PASSWORD);capture_ui("05-password");
    lv_textarea_set_text(input_text,"test-only");click(A_INPUT_CANCEL);assert(!draft.password[0]);
    click(A_BACK);click(A_DISPLAY);capture_ui("06-display");
    draft.brightness=10;atomic_store(&requested_brightness,10);click(A_BACK);assert(config.brightness==40&&draft.brightness==40&&atomic_load(&requested_brightness)==40);
    click(A_BACK);click(A_ADD);capture_ui("07-add");click(A_CODE);capture_ui("08-code-keypad");
    lv_textarea_set_text(input_text,"513100");
    bool ready_sent=false;
    for(unsigned i=0;i<lv_obj_get_child_count(root);i++) {
        lv_obj_t *o=lv_obj_get_child(root,i);
        if(lv_obj_check_type(o,&lv_keyboard_class)) { lv_obj_send_event(o,LV_EVENT_READY,NULL); ready_sent=true; break; }
    }
    assert(ready_sent && panel==ADD); draw_ui();click(A_ADD_SAVE);assert(config.count==48&&!strcmp(config.watch[47].code,"513100"));
    click(A_DETAIL+47);click(A_RULE);capture_ui("09-rule");click(A_THRESHOLD);lv_textarea_set_text(input_text,"4.123");click(A_INPUT_APPLY);click(A_ENABLE);click(A_SAVE_RULE);
    assert(config.watch[47].enabled&&config.watch[47].threshold==4123);capture_ui("10-detail");
    click(A_REMOVE);click(A_CONFIRM_REMOVE);assert(config.count==47&&!config.watch[47].code[0]);
    selected=0;click(A_NAV+1);capture_ui("11-global-unavailable");
    const char *names[]={"tencent-global.txt","tencent-global-gz.txt","tencent-global-regional.txt"};
    for(unsigned f=0;f<3;f++) {char path[160];snprintf(path,sizeof(path),"tests/fixtures.local/%s",names[f]);FILE *in=fopen(path,"rb");if(in){char body[65536]={0};fread(body,1,sizeof(body)-1,in);fclose(in);tl_global_tencent(body,network.indices);}}
    for(unsigned i=0;i<TL_GLOBAL_COUNT;i++)network.index_ok[i]=network.indices[i].source!=0;
    capture_ui("12-global-asia");click(A_NEXT);assert(global_page==1);capture_ui("13-global-west");click(A_PREV);assert(global_page==0);
    click(A_SETTINGS);click(A_ALERTS);capture_ui("14-alert-list");click(A_BACK);click(A_NAV);
    watch_page=11;capture_ui("15-last-watch-page");click(A_DETAIL+46);assert(selected==46&&panel==DETAIL);click(A_BACK);
    while(config.count<TL_MAX_WATCH){unsigned i=config.count++;snprintf(config.watch[i].code,sizeof(config.watch[i].code),"599%03u",i);}
    watch_page=15;draw_ui();click(A_DETAIL+63);assert(selected==63&&panel==DETAIL);click(A_RULE);assert(panel==RULE);click(A_BACK);click(A_BACK);
    active_alerts=UINT64_C(1)<<63;capture_ui("16-high-index-alert");click(A_ACK);assert(!active_alerts);
    tl_defaults(&config);watch_page=0;draw_ui();
    active_alerts=3;config.watch[0].threshold=4200;config.watch[1].threshold=2600;config.watch[1].below=1;capture_ui("17-alert-overlay");click(A_ACK);assert(!active_alerts);
    config.light=1;page=0;capture_ui("18-light");
    click(A_REFRESH);assert(refresh_calls==1);
    lv_obj_delete(root);lv_display_delete(d);lv_deinit();puts("PASS UI navigation, edit/cancel, add/remove, alert config/ack, brightness rollback, refresh");
    return 0;
}
