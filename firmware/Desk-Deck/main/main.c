// SPDX-License-Identifier: GPL-3.0-only
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bsp/esp_mosaico.h"
#include "esp_heap_caps.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "iris_ota_support.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "tusb.h"
#include "usb_hid.h"
#include "deck_font.h"

typedef struct { const char *label, *hint, *icon; deck_hid_action_t action; } deck_key_t;
#include "profiles.inc"
typedef struct { uint32_t magic; uint8_t brightness, light, haptics, flip; } deck_settings_t;
static deck_settings_t settings = {0x44444331, 70, 0, 1, 1}, draft;
static lv_obj_t *root, *status_label, *brightness_label;
static unsigned profile, detail_key, previous_completed;
static int panel; /* 0 home, 1 settings, 2 details, 3 lock */
static bool redraw = true, key_cancelled, key_held, save_requested, motor_ready, suppress_touch;
static int64_t key_pressed_at, motor_until;
static lv_point_t key_origin;
static char notice[128] = "轻点发送 · 长按查看";
static atomic_int requested_brightness = ATOMIC_VAR_INIT(70), test_profile = ATOMIC_VAR_INIT(-1);
static atomic_bool screen_test;
static atomic_uint screen_test_steps, live_profile, live_angle;
static atomic_int imu_x, imu_y, imu_z;
static uint8_t *capture;
static lv_color_t bg, card, ink, muted, accent;

static void colors(void)
{
    bg = lv_color_hex(settings.light ? 0xf0f2eb : 0x141b18);
    card = lv_color_hex(settings.light ? 0xfffff7 : 0x222d27);
    ink = lv_color_hex(settings.light ? 0x24382c : 0xe7eee8);
    muted = lv_color_hex(settings.light ? 0x647b6b : 0x9aac9f);
    static const uint32_t dark[] = {0xb9edc3,0xa5cbfa,0xf1bd98,0xc7b7f0};
    static const uint32_t light[] = {0x357448,0x2b609f,0x955726,0x7051a1};
    accent = lv_color_hex(settings.light ? light[profile] : dark[profile]);
}
static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text); lv_obj_set_pos(obj,x,y);
    lv_obj_set_style_text_font(obj,font,0); lv_obj_set_style_text_color(obj,color,0);
    lv_obj_remove_flag(obj,LV_OBJ_FLAG_CLICKABLE);
    return obj;
}
static lv_obj_t *button(int x, int y, int w, int h, const char *text, lv_event_cb_t cb, intptr_t data)
{
    lv_obj_t *obj=lv_button_create(root);
    lv_obj_set_pos(obj,x,y); lv_obj_set_size(obj,w,h);
    lv_obj_set_style_radius(obj,16,0); lv_obj_set_style_bg_color(obj,card,0);
    lv_obj_set_style_border_width(obj,1,0); lv_obj_set_style_border_color(obj,muted,0);
    lv_obj_set_style_shadow_width(obj,0,0); lv_obj_set_style_pad_all(obj,4,0);
    lv_obj_set_style_bg_color(obj,accent,LV_STATE_PRESSED);
    if (text) { lv_obj_t *l=label(obj,text,0,0,&lv_font_deck_20,ink); lv_obj_center(l); }
    if (cb) lv_obj_add_event_cb(obj,cb,LV_EVENT_ALL,(void *)data);
    return obj;
}
static void set_notice(const char *text)
{
    snprintf(notice,sizeof(notice),"%s",text);
    if (panel==0 && status_label) lv_label_set_text(status_label,notice);
}
static void choose_profile(unsigned value)
{
    if (value>3 || panel!=0) return;
    deck_hid_cancel_pending(); profile=value; atomic_store(&live_profile,value);
    snprintf(notice,sizeof(notice),"%s · 翻转只切换场景",profile_names[value]); redraw=true;
}
static void key_event(lv_event_t *event)
{
    unsigned slot=(unsigned)(uintptr_t)lv_event_get_user_data(event);
    lv_event_code_t code=lv_event_get_code(event);
    lv_indev_t *input=lv_indev_active();
    if (panel!=0 || slot>5 || suppress_touch || atomic_load(&screen_test)) return;
    if (code==LV_EVENT_PRESSED) {
        key_cancelled=key_held=false; key_pressed_at=esp_timer_get_time();
        if (input) lv_indev_get_point(input,&key_origin);
    } else if (code==LV_EVENT_PRESSING && input) {
        lv_point_t point; lv_indev_get_point(input,&point);
        if (abs(point.x-key_origin.x)>12 || abs(point.y-key_origin.y)>12) key_cancelled=true;
        if (!key_cancelled && !key_held && esp_timer_get_time()-key_pressed_at>=600000) {
            key_held=true; detail_key=slot; panel=2; redraw=true;
        }
    } else if (code==LV_EVENT_PRESS_LOST) key_cancelled=true;
    else if (code==LV_EVENT_RELEASED && !key_held && !key_cancelled) {
        if (deck_hid_send(defaults[profile][slot].action)) {
            char text[128]; snprintf(text,sizeof(text),"已提交 · %s",defaults[profile][slot].label); set_notice(text);
            if (settings.haptics) motor_until=esp_timer_get_time()+20000;
        } else set_notice("USB 未就绪或忙，请稍后重试");
    }
}
static void navigation_event(lv_event_t *event)
{
    if (lv_event_get_code(event)==LV_EVENT_CLICKED) choose_profile((unsigned)(uintptr_t)lv_event_get_user_data(event));
}
static void gesture_event(lv_event_t *event)
{
    if (panel || lv_event_get_code(event)!=LV_EVENT_GESTURE || !lv_indev_active()) return;
    lv_dir_t direction=lv_indev_get_gesture_dir(lv_indev_active());
    key_cancelled=true;
    if (direction==LV_DIR_LEFT) choose_profile((profile+1)%4);
    else if (direction==LV_DIR_RIGHT) choose_profile((profile+3)%4);
}
static void open_event(lv_event_t *event)
{
    if (lv_event_get_code(event)!=LV_EVENT_CLICKED) return;
    panel=(int)(intptr_t)lv_event_get_user_data(event);
    draft=settings; deck_hid_cancel_pending(); redraw=true;
}
static void close_event(lv_event_t *event)
{
    if (lv_event_get_code(event)!=LV_EVENT_CLICKED) return;
    bool save=(intptr_t)lv_event_get_user_data(event)==1;
    if (save) { settings=draft; save_requested=true; }
    atomic_store(&requested_brightness,settings.brightness);
    panel=0; redraw=true;
}
static void unlock_event(lv_event_t *event)
{
    if (lv_event_get_code(event)==LV_EVENT_LONG_PRESSED) { panel=0; redraw=true; }
}
static void brightness_event(lv_event_t *event)
{
    if (lv_event_get_code(event)!=LV_EVENT_VALUE_CHANGED) return;
    draft.brightness=(uint8_t)lv_slider_get_value(lv_event_get_target_obj(event));
    atomic_store(&requested_brightness,draft.brightness);
    lv_label_set_text_fmt(brightness_label,"屏幕亮度 %u%%",draft.brightness);
}
static void toggle_event(lv_event_t *event)
{
    if (lv_event_get_code(event)!=LV_EVENT_VALUE_CHANGED) return;
    bool on=lv_obj_has_state(lv_event_get_target_obj(event),LV_STATE_CHECKED);
    switch ((intptr_t)lv_event_get_user_data(event)) {
    case 0: draft.haptics=on; break;
    case 1: draft.flip=on; break;
    case 2: draft.light=on; break;
    }
}
static void toggle(int y,const char *text,bool checked,intptr_t field)
{
    label(root,text,24,y+8,&lv_font_deck_20,ink);
    lv_obj_t *obj=lv_switch_create(root); lv_obj_set_pos(obj,378,y); lv_obj_set_size(obj,56,32);
    lv_obj_set_ext_click_area(obj,10);
    if (checked) lv_obj_add_state(obj,LV_STATE_CHECKED);
    lv_obj_add_event_cb(obj,toggle_event,LV_EVENT_VALUE_CHANGED,(void *)field);
}
static void draw_ui(void)
{
    colors(); status_label=NULL;
    /* Do not turn a held finger into a new key press after replacing a page. */
    lv_indev_t *input=bsp_display_get_input_dev();
    if(input && lv_indev_get_state(input)==LV_INDEV_STATE_PRESSED) suppress_touch=true;
    if (root) lv_obj_delete(root);
    root=lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(root); lv_obj_set_size(root,480,480);
    lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0); lv_obj_set_style_bg_color(root,bg,0);
    lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLLABLE);
    if (panel==0) {
        label(root,"Desk-Deck",20,18,&lv_font_montserrat_24,ink);
        label(root,profile_names[profile],20,52,&lv_font_deck_20,accent);
        label(root,deck_hid_ready()?"USB 就绪":"USB 未就绪",138,53,&lv_font_deck_20,muted);
        button(320,15,64,44,"锁定",open_event,3); button(392,15,68,44,"设置",open_event,1);
        status_label=label(root,notice,20,85,&lv_font_deck_20,muted);
        lv_obj_set_width(status_label,438); lv_label_set_long_mode(status_label,LV_LABEL_LONG_DOT);
        for(unsigned i=0;i<6;i++) {
            lv_obj_t *key=button(20+(i%3)*148,116+(i/3)*147,140,135,NULL,key_event,i);
            lv_obj_add_flag(key,LV_OBJ_FLAG_GESTURE_BUBBLE);
            const deck_key_t *action=&defaults[profile][i];
            lv_obj_t *sym=label(key,action->icon,0,0,&lv_font_montserrat_32,accent); lv_obj_align(sym,LV_ALIGN_TOP_MID,0,17);
            lv_obj_t *name=label(key,action->label,0,0,&lv_font_deck_20,ink); lv_obj_align(name,LV_ALIGN_TOP_MID,0,66);
            lv_obj_t *hint=label(key,action->hint,0,0,&lv_font_deck_16,muted); lv_obj_align(hint,LV_ALIGN_BOTTOM_MID,0,-7);
            if (!deck_hid_ready()) lv_obj_add_state(key,LV_STATE_DISABLED);
        }
        for(unsigned i=0;i<4;i++) {
            lv_obj_t *nav=button(20+i*112,424,104,38,profile_names[i],navigation_event,i);
            if (i==profile) lv_obj_set_style_border_color(nav,accent,0);
        }
    } else if(panel==1) {
        label(root,"设备设置",24,25,&lv_font_deck_20,ink);
        brightness_label=label(root,"",24,91,&lv_font_deck_20,ink);
        lv_label_set_text_fmt(brightness_label,"屏幕亮度 %u%%",draft.brightness);
        lv_obj_t *slider=lv_slider_create(root); lv_obj_set_pos(slider,40,146); lv_obj_set_size(slider,392,16);
        lv_slider_set_range(slider,10,100); lv_slider_set_value(slider,draft.brightness,LV_ANIM_OFF);
        lv_obj_set_style_pad_all(slider,12,LV_PART_KNOB); lv_obj_set_ext_click_area(slider,24);
        lv_obj_add_event_cb(slider,brightness_event,LV_EVENT_VALUE_CHANGED,NULL);
        toggle(203,"轻触振动",draft.haptics,0); toggle(261,"翻转切换场景",draft.flip,1); toggle(319,"日间主题",draft.light,2);
        label(root,"亮度即时预览 · 返回撤销",24,379,&lv_font_deck_20,muted);
        button(24,420,204,44,"返回",close_event,0); button(244,420,212,44,"保存",close_event,1);
    } else if(panel==2) {
        const deck_key_t *action=&defaults[profile][detail_key];
        label(root,"操作说明",24,28,&lv_font_deck_20,ink);
        lv_obj_t *sym=label(root,action->icon,0,0,&lv_font_montserrat_32,accent); lv_obj_align(sym,LV_ALIGN_TOP_MID,0,122);
        lv_obj_t *name=label(root,action->label,0,0,&lv_font_deck_20,ink); lv_obj_align(name,LV_ALIGN_TOP_MID,0,184);
        lv_obj_t *hint=label(root,action->hint,0,0,&lv_font_deck_16,accent); lv_obj_align(hint,LV_ALIGN_TOP_MID,0,226);
        label(root,"长按仅查看，不会发送按键",82,294,&lv_font_deck_20,muted);
        button(24,418,432,46,"返回",close_event,0);
    } else {
        label(root,LV_SYMBOL_EYE_CLOSE,220,120,&lv_font_montserrat_32,accent);
        label(root,"控制台已锁定",166,190,&lv_font_deck_20,ink);
        label(root,"点击与翻转不会发送按键",100,235,&lv_font_deck_20,muted);
        button(90,302,300,64,"长按解锁",unlock_event,0);
    }
    lv_obj_add_event_cb(root,gesture_event,LV_EVENT_GESTURE,NULL);
    redraw=false;
}

static void load_settings(void)
{
    nvs_handle_t handle;
    if (nvs_open("deskdeck",NVS_READONLY,&handle)!=ESP_OK) return;
    deck_settings_t saved; size_t length=sizeof(saved);
    if (nvs_get_blob(handle,"settings_v1",&saved,&length)==ESP_OK && length==sizeof(saved) &&
        saved.magic==settings.magic && saved.brightness>=10 && saved.brightness<=100 && saved.light<=1 && saved.haptics<=1 && saved.flip<=1) settings=saved;
    nvs_close(handle);
}
static void store_settings(deck_settings_t value)
{
    nvs_handle_t handle; esp_err_t err=nvs_open("deskdeck",NVS_READWRITE,&handle);
    if(err==ESP_OK) {
        err=nvs_set_blob(handle,"settings_v1",&value,sizeof(value));
        if(err==ESP_OK) err=nvs_commit(handle);
        nvs_close(handle);
    }
    ESP_LOGI("desk_config","save: %s",esp_err_to_name(err));
    if(bsp_display_lock(100)) { set_notice(err==ESP_OK?"设置已保存":"保存失败，请重新尝试"); bsp_display_unlock(); }
}

static esp_err_t screen_begin(const esp_iris_media_desc_t *request,esp_iris_media_desc_t *actual,uint32_t *size,void *context)
{
    (void)request; (void)context;
    if(capture) return ESP_ERR_INVALID_STATE;
    uint8_t *bytes=heap_caps_malloc(480*480*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!bytes) return ESP_ERR_NO_MEM;
    if(!bsp_display_lock(500)) { free(bytes); return ESP_ERR_TIMEOUT; }
    lv_display_t *display=bsp_display_get(); lv_draw_buf_t *buffer=lv_display_get_buf_active(display);
    bool full=buffer && buffer->header.cf==LV_COLOR_FORMAT_RGB565 && buffer->header.w==480 && buffer->header.h==480 && buffer->header.stride==960 && lv_display_get_render_mode(display)==LV_DISPLAY_RENDER_MODE_FULL;
    if(full) for(size_t i=0;i<480*480*2;i+=2) { bytes[i]=buffer->data[i+1]; bytes[i+1]=buffer->data[i]; }
    bsp_display_unlock();
    if(!full) { free(bytes); return ESP_ERR_NOT_SUPPORTED; }
    capture=bytes; *size=480*480*2;
    *actual=(esp_iris_media_desc_t){.width=480,.height=480,.stride=960,.format=ESP_IRIS_PIXEL_FORMAT_RGB565};
    return ESP_OK;
}
static esp_err_t screen_read(uint32_t offset,uint8_t *out,size_t capacity,size_t *used,void *context)
{
    (void)context;
    if(!capture || offset>=480*480*2) return ESP_ERR_INVALID_ARG;
    *used=480*480*2-offset; if(*used>capacity) *used=capacity;
    memcpy(out,capture+offset,*used); return ESP_OK;
}
static void screen_end(void *context) { (void)context; free(capture); capture=NULL; }
static esp_err_t diagnostic_rpc(const esp_iris_rpc_request_t *request,uint8_t *response,size_t capacity,size_t *used,void *context)
{
    (void)context;
    if(capacity<400) return ESP_ERR_INVALID_SIZE;
    if(request->payload_size==11 && !memcmp(request->payload,"screen-test",11)) atomic_store(&screen_test,true);
    else if(request->payload_size==4 && !memcmp(request->payload,"stop",4)) atomic_store(&screen_test,false);
    else if(request->payload_size==2 && request->payload[0]=='p' && request->payload[1]>='0' && request->payload[1]<='3') atomic_store(&test_profile,request->payload[1]-'0');
    else if(request->payload_size!=6 || memcmp(request->payload,"status",6)) return ESP_ERR_INVALID_ARG;
    int length=snprintf((char *)response,capacity,"{\"hid_ready\":%s,\"completed\":%u,\"dropped\":%u,\"profile\":%u,\"angle\":%u,\"screen_test\":%s,\"steps\":%u,\"accel_mg\":[%d,%d,%d],\"free_internal\":%u}",
        deck_hid_ready()?"true":"false",deck_hid_completed(),deck_hid_dropped(),atomic_load(&live_profile),atomic_load(&live_angle),atomic_load(&screen_test)?"true":"false",atomic_load(&screen_test_steps),atomic_load(&imu_x),atomic_load(&imu_y),atomic_load(&imu_z),(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    if(length<0 || (size_t)length>=capacity) return ESP_ERR_INVALID_SIZE;
    *used=(size_t)length; return ESP_OK;
}

void app_main(void)
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_boot_probe());
    ESP_ERROR_CHECK(nvs_flash_init()); /* Preserve other applications' namespaces. */
    iris_ota_support_start();
    ESP_ERROR_CHECK(deck_hid_start()); load_settings();
    atomic_store(&requested_brightness,settings.brightness);
    ESP_ERROR_CHECK(bsp_display_start()?ESP_OK:ESP_FAIL);
    ESP_ERROR_CHECK(bsp_display_brightness_set(settings.brightness));
    if(bsp_display_lock(1000)) {
        lv_indev_set_long_press_time(bsp_display_get_input_dev(),800);
        draw_ui(); bsp_display_unlock();
    }
    esp_err_t imu_result=bsp_imu_init();
    if(imu_result==ESP_OK) imu_result=bsp_imu_start(NULL);
    bool imu_ready=imu_result==ESP_OK;
    ESP_LOGI("desk_imu","sampling: %s",esp_err_to_name(imu_result));
    motor_ready=bsp_motor_init()==ESP_OK;
    const esp_iris_screen_backend_t backend={.begin=screen_begin,.read=screen_read,.end=screen_end};
    ESP_ERROR_CHECK(esp_iris_screen_register(&backend));
    ESP_ERROR_CHECK(esp_iris_rpc_register(0x4444,1,diagnostic_rpc,NULL));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_mark_healthy());
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_task_wdt_add(NULL));
    int applied_brightness=settings.brightness, angle=0, candidate=-1, stable=-1;
    int64_t candidate_at=0, test_started=0, last_report=0, imu_retry_at=0;
    bool was_ready=false, motor_on=false;
    for(;;) {
        int64_t now=esp_timer_get_time();
        int desired_angle=angle, desired_profile=-1;
        if(imu_ready && now>=imu_retry_at) {
            float x,y,z;
            if(bsp_imu_get_accel(&x,&y,&z)==ESP_OK) {
                atomic_store(&imu_x,(int)(x*1000)); atomic_store(&imu_y,(int)(y*1000)); atomic_store(&imu_z,(int)(z*1000));
                int orientation=-1;
                if(fabsf(z)<0.72f) {
                    if(fabsf(y)>0.72f && fabsf(x)<0.5f) orientation=y>0?0:2;
                    else if(fabsf(x)>0.72f && fabsf(y)<0.5f) orientation=x>0?1:3;
                }
                if(orientation!=candidate) { candidate=orientation; candidate_at=now; }
                if(candidate>=0 && candidate!=stable && now-candidate_at>=400000) {
                    stable=candidate; desired_angle=stable*90; desired_profile=stable;
                }
            } else {
                candidate=-1; imu_retry_at=now+1000000;
            }
        }
        bool testing=atomic_load(&screen_test);
        if(testing) {
            if(!test_started) { test_started=now; atomic_store(&screen_test_steps,0); }
            unsigned step=(unsigned)((now-test_started)/1000000);
            if(step<24) {
                desired_angle=(step%4)*90; desired_profile=(int)(step%4);
                static const int levels[] = {20,70,40,100};
                atomic_store(&requested_brightness,levels[step%4]);
                atomic_store(&screen_test_steps,step+1);
            } else { atomic_store(&screen_test,false); }
        } else if(test_started) {
            test_started=0; desired_angle=stable>=0?stable*90:0;
            if(bsp_display_lock(100)) {
                atomic_store(&requested_brightness,settings.brightness);
                bsp_display_unlock();
            }
        }
        int explicit_profile=atomic_exchange(&test_profile,-1);
        if(explicit_profile>=0) desired_profile=explicit_profile;
        if(desired_angle!=angle && bsp_display_set_rotation((bsp_display_rotation_t)desired_angle)==ESP_OK) {
            angle=desired_angle; atomic_store(&live_angle,(unsigned)angle);
        }
        int brightness=atomic_load(&requested_brightness);
        if(brightness!=applied_brightness && bsp_display_brightness_set(brightness)==ESP_OK) applied_brightness=brightness;
        bool save=false, vibrate=motor_on; deck_settings_t snapshot;
        if(bsp_display_lock(100)) {
            lv_indev_t *input=bsp_display_get_input_dev();
            if(input && lv_indev_get_state(input)==LV_INDEV_STATE_RELEASED) suppress_touch=false;
            if(desired_profile>=0 && panel==0 && (settings.flip || explicit_profile>=0 || testing) && (unsigned)desired_profile!=profile) choose_profile((unsigned)desired_profile);
            bool connected=deck_hid_ready();
            if(connected!=was_ready) { was_ready=connected; if(panel==0) redraw=true; }
            unsigned sent=deck_hid_completed();
            if(sent!=previous_completed) { previous_completed=sent; set_notice("已发送 · 按键已释放"); }
            if(redraw) draw_ui();
            if(save_requested) { snapshot=settings; save_requested=false; save=true; }
            vibrate=settings.haptics && now<motor_until;
            bsp_display_unlock();
        }
        if(motor_ready && vibrate!=motor_on) { bsp_motor_set(vibrate); motor_on=vibrate; }
        if(save) store_settings(snapshot);
        if(now-last_report>=10000000) {
            last_report=now; ESP_LOGI("desk_status","hid=%d sent=%u dropped=%u heap=%u",deck_hid_ready(),deck_hid_completed(),deck_hid_dropped(),(unsigned)esp_get_free_heap_size());
        }
        (void)esp_task_wdt_reset(); vTaskDelay(pdMS_TO_TICKS(25));
    }
}
