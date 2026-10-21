// SPDX-License-Identifier: GPL-3.0-only
/* Desktop hardware adapter. UI, orientation filter, timers and settings validation
 * are compiled directly from main/. Location queries use Windows HTTPS; weather,
 * Wi-Fi remains simulated. No ESP32 or NVS is emulated here. */
#include <SDL.h>
#include "ui_weather_art.h"
#include "ui_daimao.h"
#include "project_identity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "ui.h"
#include "ui_theme.h"
#include "ui_pet.h"
#include "ui_text.h"
#include "ui_network.h"
#include "location_pc.h"
#include <windows.h>

static tt_settings_t settings;
static tt_pet_book_t pets;
static tt_pet_runtime_t pet_runtime;
static bool pet_dirty, force_pet_save_failure;
static int64_t next_pet_save=60000;
static char pet_path[1024];
static tt_timer_t timer;
static tt_orientation_t orientation;
static tt_face_t face = TT_UNKNOWN;
static int input_axis = 2, pending_home, rotation;
static bool alarm, short_timers, weather = true, moving;
static tt_alarm_t reminder;
static tt_interaction_view_t interaction;
static unsigned weather_scenario,power_scenario;
static int simulated_period=-1, simulated_season=-1;
static const int weather_codes[]={0,3,61,73,45,95};
static const float weather_temperatures[]={23.5f,21.f,17.f,-4.f,12.f,16.f};
static const int weather_humidity[]={55,65,90,85,96,92};
static lv_display_t *display;
static lv_obj_t *info, *control_buttons[15];
static void device_closed(lv_event_t *e) { (void)e; display = NULL; }
static void controls_closed(lv_event_t *e) { (void)e; info = NULL; }
static char notice[112] = "Preview / sample weather";
static char simulated_ssid[33];
static tt_services_view_t mock_network;
static int lookup_scenario; /* Test-only: success, empty, offline, failure. */
static bool test_mode, mock_wrong_password;
static uint32_t test_tick;
static void proof(const char *name);
static void advance(unsigned ms);
static uint32_t preview_tick(void) { return test_mode?test_tick:SDL_GetTicks(); }
static int scan_scenario;
static int64_t scan_due, connect_due;
static void mock_network_poll(int64_t now)
{
    if (scan_due && now>=scan_due) {
        scan_due=0; mock_network.scan_state=scan_scenario==2?TT_SCAN_FAILED:TT_SCAN_DONE;
        mock_network.ap_count=scan_scenario?0:6; ++mock_network.scan_generation;
        const tt_wifi_ap_t aps[]={
            {.ssid="Home_Garden",.rssi=-39,.secured=true}, {.ssid="小院工作室",.rssi=-51,.secured=true},
            {.ssid="Guest_Open",.rssi=-62,.secured=false}, {.ssid="Mosaico_Lab",.rssi=-68,.secured=true},
            {.ssid="Studio_2.4G",.rssi=-74,.secured=true}, {.ssid="Neighbor_WiFi",.rssi=-83,.secured=true}};
        memcpy(mock_network.aps,aps,sizeof(aps));
    }
    if (connect_due && now>=connect_due) {
        connect_due=0; mock_network.network_state=mock_wrong_password?TT_NET_FAILED:TT_NET_CONNECTED;
        snprintf(mock_network.network_message,sizeof(mock_network.network_message),"%s",mock_wrong_password?"Password rejected; please retry":"Connected");
    }
}
static tt_command_t commands[8];
static unsigned command_count;
static const char *face_names[] = {"正放", "右转", "倒置", "左转", "朝上平放", "屏幕朝下", "未定向"};

static int64_t now_ms(void) { return test_mode?test_tick:(int64_t)SDL_GetTicks64(); }
static void pet_load(void)
{
    tt_pet_default(&pets);
    if (test_mode) return;
    char *base=SDL_GetBasePath();
    snprintf(pet_path,sizeof(pet_path),"%spreview_pet_save.bin",base?base:""); SDL_free(base);
    SDL_RWops *f=SDL_RWFromFile(pet_path,"rb"); if (!f) return;
    tt_pet_book_t saved; size_t size=SDL_RWread(f,&saved,1,sizeof(saved));
    Sint64 total=SDL_RWsize(f); SDL_RWclose(f);
    if (total!=(Sint64)size || !tt_pet_decode(&pets,&saved,size))
        fprintf(stderr,"Invalid preview pet save; defaults loaded.\n");
}
static bool pet_save(const tt_pet_book_t *book)
{
    if (force_pet_save_failure) return false;
    if (test_mode) return true;
    char temp[1040]; snprintf(temp,sizeof(temp),"%s.tmp",pet_path);
    SDL_RWops *f=SDL_RWFromFile(temp,"wb"); if (!f) return false;
    bool written=SDL_RWwrite(f,book,1,sizeof(*book))==sizeof(*book);
    if (SDL_RWclose(f)!=0) written=false;
    if (!written) return false;
    wchar_t source[1040],destination[1024];
    if (!MultiByteToWideChar(CP_UTF8,0,temp,-1,source,1040) || !MultiByteToWideChar(CP_UTF8,0,pet_path,-1,destination,1024)) return false;
    return MoveFileExW(source,destination,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
}
static void preview_shutdown(void)
{
    if (pet_dirty && !pet_save(&pets)) fprintf(stderr,"Could not save pet progress.\n");
    SDL_Quit();
}
static void timezone_apply(void)
{
    _putenv_s("TZ", settings.timezone);
    _tzset();
}
static bool submit(const tt_command_t *command, void *context)
{
    (void)context;
    if (command_count == 8) return false;
    commands[command_count++] = *command;
    return true;
}
static void process_commands(int64_t now)
{
    for (unsigned i = 0; i < command_count; ++i) {
        tt_command_t *c = &commands[i];
        if (c->type==TT_CMD_LOCATION) {
            if (test_mode) {
                mock_network.lookup=(tt_location_view_t){.id=c->location.id,.kind=c->location.kind,.simulated=true};
                const char *body=c->location.kind==TT_LOCATION_IP?
                    "{\"success\":true,\"city\":\"杭州\",\"region\":\"浙江省\",\"country\":\"中国\",\"latitude\":30.2741,\"longitude\":120.1551}":
                    "{\"results\":[{\"name\":\"杭州\",\"admin1\":\"浙江省\",\"country\":\"中国\",\"latitude\":30.2741,\"longitude\":120.1551},"
                    "{\"name\":\"杭州湾\",\"admin1\":\"浙江省\",\"country\":\"中国\",\"latitude\":30.3,\"longitude\":121.1}]}";
                tt_location_parse(lookup_scenario==1?"{}":body,&mock_network.lookup);
                if (lookup_scenario>=2) { mock_network.lookup.count=0;
                    mock_network.lookup.state=lookup_scenario==2?TT_LOCATION_OFFLINE:TT_LOCATION_FAILED; }
            } else tt_pc_location_request(&c->location);
        } else if (c->type==TT_CMD_FEEDBACK) {
            if (!alarm && tt_settings_valid(&c->settings)) tt_alarm_start(&reminder,&c->settings.alarm,c->settings.vibration,true,now);
            snprintf(notice,sizeof(notice),"PC preview: sound and vibration simulated");
        } else if (c->type==TT_CMD_FEEDBACK_STOP) {
            if (reminder.preview) tt_alarm_stop(&reminder);
        }
        else if (c->type == TT_CMD_STOP) {
            tt_timer_stop(&timer); alarm = false; tt_alarm_stop(&reminder);
            snprintf(notice, sizeof(notice), "Stopped; flip to another face to start");
        } else if (c->type == TT_CMD_START) {
            /* Match firmware: explicit touch starts the displayed face even
             * while the finger temporarily disturbs orientation settlement. */
            if (tt_timer_start(&timer,&settings.timer,face,now)) {
                alarm=false; tt_alarm_stop(&reminder); notice[0]='\0';
            }
        } else if (c->type == TT_CMD_TOGGLE) {
            tt_timer_toggle(&timer, now);
        } else if (c->type == TT_CMD_SAVE) {
            c->settings.timer.home_axis = settings.timer.home_axis;
            c->settings.timer.right_axis = settings.timer.right_axis;
            c->settings.timer.calibrated = settings.timer.calibrated;
            if (tt_settings_valid(&c->settings)) {
                settings = c->settings;
                timezone_apply();
                tt_ui_save_result(true);
                snprintf(simulated_ssid,sizeof(simulated_ssid),"%s",settings.ssid);
                snprintf(notice, sizeof(notice), "Saved for this session; Wi-Fi/weather remain simulated");
            } else { tt_ui_save_result(false); snprintf(notice, sizeof(notice), "Invalid settings"); }
        } else if (c->type == TT_CMD_PET) {
            tt_pet_book_t candidate=pets; tt_pet_runtime_t runtime=pet_runtime;
            tt_pet_result_t result=tt_pet_act(&candidate,&runtime,c->settings.pet,c->pet_action,now);
            if (result==TT_PET_OK) {
                if (pet_save(&candidate)) { pets=candidate; pet_runtime=runtime; pet_dirty=false; next_pet_save=now+60000; }
                else result=TT_PET_SAVE_FAILED;
            }
            tt_ui_pet_result(result);
        } else if (c->type == TT_CMD_NETWORK) {
            snprintf(simulated_ssid,sizeof(simulated_ssid),"%s",c->settings.ssid);
            mock_wrong_password=!strcmp(c->settings.password,"wrongpass");
            mock_network.network_state=TT_NET_CONNECTING;
            snprintf(mock_network.network_message,sizeof(mock_network.network_message),"Connecting...");
            connect_due=now+(test_mode?1:1400);
        } else if (c->type==TT_CMD_SCAN) {
            mock_network.scan_state=TT_SCAN_RUNNING; ++mock_network.scan_generation;
            scan_due=now+(test_mode?1:1200);
        } else if (c->type == TT_CMD_HOME || c->type == TT_CMD_RIGHT) {
            if (!orientation.settled) {
                snprintf(notice, sizeof(notice), "Hold still on a flat side, then tap again");
            } else if (c->type == TT_CMD_HOME) {
                pending_home = orientation.stable;
                snprintf(notice, sizeof(notice), "Select Right in controls; tap Set right turn");
            } else if (!pending_home || abs(pending_home) == abs(orientation.stable)) {
                snprintf(notice, sizeof(notice), "Set upright first, then turn exactly 90 deg");
            } else {
                settings.timer.home_axis = (int8_t)pending_home;
                settings.timer.right_axis = orientation.stable;
                settings.timer.calibrated = true;
                pending_home = 0;
                tt_timer_init(&timer); alarm = false;
                snprintf(notice, sizeof(notice), "Calibration saved for this session");
            }
        }
    }
    command_count = 0;
}
static void update(void)
{
    int64_t now = now_ms();
    float accel[3] = {0}, gyro[3] = {0};
    accel[abs(input_axis)-1] = input_axis > 0 ? 1.f : -1.f;
    if (moving) gyro[0] = 90.f;
    tt_orientation_sample(&orientation, accel, gyro, true, now);
    if (tt_timer_tick(&timer, now)) {
        alarm=true;
        tt_alarm_start(&reminder,&settings.alarm,settings.vibration,false,now);
        reminder.origin_axis=orientation.stable;
        if (timer.duration_s>=60) {
            (void)tt_pet_focus_complete(&pets,settings.pet,timer.duration_s);
            pet_dirty=!pet_save(&pets); next_pet_save=now+60000;
        }
    }
    pet_dirty|=tt_pet_tick(&pets,&pet_runtime,settings.pet,now);
    process_commands(now);
    if (pet_dirty && now>=next_pet_save) { if (pet_save(&pets)) pet_dirty=false; next_pet_save=now+60000; }
    if ((!tt_ui_editing() || timer.state==TT_FINISHED) && orientation.settled) {
        face = tt_face_from_axis(&settings.timer, orientation.stable);
        if (timer.state==TT_FINISHED) {
            if (tt_alarm_turn(&reminder,orientation.stable)) {
                tt_timer_stop(&timer); timer.observed=face;
            }
        } else tt_timer_face(&timer, &settings.timer, face, now);
        int wanted = tt_rotation(face, settings.timer.reverse_rotation, rotation);
        if (wanted != rotation) {
            rotation = wanted;
            lv_display_set_rotation(display, (lv_display_rotation_t)(rotation / 90));
        }
    }
    if (timer.state != TT_FINISHED) { alarm = false; if (!reminder.preview) tt_alarm_stop(&reminder); }
    interaction.alarm_active=tt_alarm_poll(&reminder,now).active;
    mock_network_poll(now+(test_mode?2:0));
    tt_services_view_t service=mock_network;
    const tt_power_view_t scenarios[]={
        {.valid=true,.simulated=true,.state=TT_POWER_CHARGING,.percent=78,.voltage_mv=4020,.current_ma=26},
        {.valid=true,.simulated=true,.state=TT_POWER_BATTERY,.percent=67,.voltage_mv=3850,.current_ma=-118},
        {.valid=true,.simulated=true,.state=TT_POWER_BATTERY,.percent=12,.voltage_mv=3520,.current_ma=-108},
        {.valid=true,.simulated=true,.state=TT_POWER_FULL,.percent=100,.voltage_mv=4180},
        {.simulated=true,.state=TT_POWER_UNKNOWN}};
    service.power=scenarios[power_scenario];
    if (!test_mode) tt_pc_location_snapshot(&service.lookup);
    service.connected=weather && mock_network.network_state==TT_NET_CONNECTED;
    service.weather_valid=weather; service.simulated=true; service.temperature=weather_temperatures[weather_scenario];
    service.humidity=weather_humidity[weather_scenario]; service.weather_code=weather_codes[weather_scenario]; service.updated=time(NULL);
    time_t scene_time=time(NULL); struct tm scene_clock; localtime_s(&scene_clock,&scene_time);
    if (test_mode || simulated_period>=0 || simulated_season>=0) {
        if (test_mode) { scene_clock.tm_mon=3; scene_clock.tm_hour=12; }
        if (simulated_period>=0) scene_clock.tm_hour=(int[]){6,12,18,22}[simulated_period];
        if (simulated_season>=0) scene_clock.tm_mon=(int[]){3,6,9,0}[simulated_season];
        scene_clock.tm_isdst=-1; service.scene_time=mktime(&scene_clock);
    }
    snprintf(service.location,sizeof(service.location),"DEMO weather");
    snprintf(service.message,sizeof(service.message),"DEMO: offline / no weather data");
    snprintf(service.ssid,sizeof(service.ssid),"%s",simulated_ssid);
    tt_ui_interaction_snapshot(&interaction);
    tt_ui_pet_snapshot(&pets);
    tt_ui_render(&timer, face, &settings, &service, notice, alarm);
    if (info) lv_label_set_text_fmt(info,
        "姿态：%s · %s · 校准：%s\n计时：%s · 剩余 %lld 毫秒 · 屏幕 %d°\n模拟天气：%s（代码 %d）\n%s",
        face_names[face], orientation.settled ? "稳定" : moving?"移动中":"识别中",
        settings.timer.calibrated?"完成":"待校准",
        (const char *[]) {"待机", "计时中", "已暂停", "时间到"}[timer.state],
        (long long)timer.remaining_ms,rotation,
        weather?tt_text(TT_ZH,tt_weather_text(service.weather_code)):"无数据",weather?service.weather_code:-1,
        alarm?"到时提醒：仅模拟，不驱动马达":tt_ui_editing()?"设置中：暂时锁定屏幕方向":"方向按钮模拟 IMU；电脑不连接真实传感器");
    if (info) {
        lv_label_set_text_fmt(lv_obj_get_child(control_buttons[12],0),"时段：%s · 点击切换",simulated_period<0?"跟随时钟":(const char*[]){"清晨","白天","黄昏","夜晚"}[simulated_period]);
        lv_label_set_text_fmt(lv_obj_get_child(control_buttons[13],0),"季节：%s · 点击切换",simulated_season<0?"跟随日期":(const char*[]){"春季","夏季","秋季","冬季"}[simulated_season]);
        lv_label_set_text(lv_obj_get_child(control_buttons[8],0),moving?"当前移动中 · 点此恢复静止":"当前静止 · 点此模拟晃动");
        lv_label_set_text(lv_obj_get_child(control_buttons[9],0),settings.timer.calibrated?"虚拟校准已完成 · 点此清除":"尚未校准 · 点此恢复虚拟校准");
        lv_label_set_text(lv_obj_get_child(control_buttons[6],0),short_timers?"当前5秒演示 · 点此恢复默认时长":"当前默认时长 · 点此改为5秒演示");
    }
}
static void control(lv_event_t *e)
{
    intptr_t action = (intptr_t)lv_event_get_user_data(e);
    static const int axes[] = {2, 1, -2, -1, 3, -3};
    if (action < 6) input_axis = axes[action];
    else if (action == 6) {
        short_timers = !short_timers;
        tt_config_t defaults = tt_config_default();
        for (int i = 1; i < TT_FRONT; ++i) settings.timer.seconds[i] = short_timers ? 5 : defaults.seconds[i];
        tt_timer_init(&timer); alarm = false;
        snprintf(notice, sizeof(notice), short_timers ? "DEMO: all directions use 5 seconds" : "Default durations restored");
    } else if (action == 7) weather = !weather;
    else if (action == 8) moving = !moving;
    else if (action == 9) {
        settings.timer.calibrated = !settings.timer.calibrated; pending_home = 0;
        if (settings.timer.calibrated) { settings.timer.home_axis=2; settings.timer.right_axis=1; }
        tt_timer_init(&timer); alarm = false;
        snprintf(notice, sizeof(notice), "%s",settings.timer.calibrated?"Calibration saved for this session":"Open Settings to test two-step calibration");
    } else if (action==10) scan_scenario=(scan_scenario+1)%3;
    else if (action==11) { weather_scenario=(weather_scenario+1)%6; weather=true; }
    else if (action==12) simulated_period=simulated_period==3?-1:simulated_period+1;
    else if (action==13) simulated_season=simulated_season==3?-1:simulated_season+1;
    else if (action==14) power_scenario=(power_scenario+1)%5;

}
static void text(lv_obj_t *parent, const char *value)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_text(label, value);
}
static lv_display_t *controls_create(void)
{
    lv_display_t *controls = lv_sdl_window_create(480, 720);
    lv_display_add_event_cb(controls, controls_closed, LV_EVENT_DELETE, NULL);
    lv_sdl_window_set_title(controls, "Time-Tick | 中文模拟控制台");
    lv_sdl_window_set_resizeable(controls, false);
    lv_display_set_default(controls);
    lv_indev_set_display(lv_sdl_mouse_create(), controls);
    SDL_Window *main_window = lv_sdl_window_get_window(display);
    int x, y; SDL_GetWindowPosition(main_window, &x, &y);
    SDL_SetWindowPosition(lv_sdl_window_get_window(controls), x + 500, y);
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_text_font(screen,&tt_font_16,0);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 20, 0);
    lv_obj_set_style_pad_row(screen, 6, 0);
    lv_obj_set_style_bg_color(screen, lv_color_hex(TT_PAPER), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(TT_INK), 0);
    text(screen, "Time-Tick · 电脑预览控制台");
    info = lv_label_create(screen);
    lv_obj_set_width(info, LV_PCT(100));
    lv_label_set_text(info,"正在启动虚拟方向传感器…");
    static const char *labels[] = {"正放 / 时钟", "右转 / 5分钟", "倒置 / 10分钟", "左转 / 25分钟",
        "朝上平放 / 不启动", "屏幕朝下 / 不启动", "切换5秒演示 / 默认时长", "切换示例天气 / 无数据",
        "模拟晃动 / 恢复静止", "清除虚拟校准", "网络扫描：正常 / 空列表 / 失败",
        "下一种天气：晴、多云、雨、雪、雾、雷雨", "时段：跟随时钟 · 点击切换", "季节：跟随日期 · 点击切换", "电量模拟：充电 / 放电 / 低电 / 满电 / 未知"};
    lv_obj_t *row = lv_obj_create(screen);
    lv_obj_set_size(row, LV_PCT(100), 152);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_all(row, 4, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(TT_CREAM), 0);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(TT_WOOD), 0);
    const int order[]={0,1,2,3,4,5,11,12,13,14,6,7,8,9,10};
    for (unsigned n = 0; n < sizeof(order)/sizeof(order[0]); ++n) {
        int i=order[n];
        lv_obj_t *b = lv_button_create(i < 6 ? row : screen);
        control_buttons[i]=b;
        lv_obj_set_size(b, i < 6 ? 194 : 430, 34);
        lv_obj_set_style_bg_color(b, lv_color_hex(TT_GREEN), 0);
        lv_obj_set_style_radius(b, 10, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(TT_WOOD), LV_STATE_PRESSED);
        lv_obj_add_event_cb(b, control, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, labels[i]); lv_obj_center(l);
    }
    lv_display_set_default(display);
    return controls;
}
static lv_obj_t *find_button(lv_obj_t *parent, const char *caption)
{
    if (lv_obj_has_flag(parent,LV_OBJ_FLAG_HIDDEN)) return NULL;
    /* Match hit-testing order: modal controls cover same-named page buttons. */
    for (int32_t i=(int32_t)lv_obj_get_child_count(parent)-1; i>=0; --i) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        if (lv_obj_has_flag(child,LV_OBJ_FLAG_HIDDEN)) continue;
        if (lv_obj_check_type(child, &lv_button_class)) {
            lv_obj_t *label = lv_obj_get_child(child, 0);
            if (label && lv_obj_check_type(label, &lv_label_class) &&
                strcmp(lv_label_get_text(label), caption) == 0) return child;
        }
        lv_obj_t *found = find_button(child, caption);
        if (found) return found;
    }
    return NULL;
}
static lv_obj_t *find_field(lv_obj_t *parent)
{
    for (uint32_t i=0; i<lv_obj_get_child_count(parent); ++i) {
        lv_obj_t *child = lv_obj_get_child(parent, (int32_t)i);
        if (lv_obj_check_type(child, &lv_textarea_class)) return child;
        lv_obj_t *found = find_field(child);
        if (found) return found;
    }
    return NULL;
}
static lv_obj_t *find_dropdown(lv_obj_t *parent)
{
    for (uint32_t i=0;i<lv_obj_get_child_count(parent);++i) {
        lv_obj_t *child=lv_obj_get_child(parent,(int32_t)i);
        if (lv_obj_check_type(child,&lv_dropdown_class)) return child;
        lv_obj_t *found=find_dropdown(child); if (found) return found;
    }
    return NULL;
}
static lv_obj_t *find_visible_class(lv_obj_t *parent,const lv_obj_class_t *cls)
{
    if (lv_obj_has_flag(parent,LV_OBJ_FLAG_HIDDEN)) return NULL;
    if (lv_obj_check_type(parent,cls)) return parent;
    for (uint32_t i=0;i<lv_obj_get_child_count(parent);++i) {
        lv_obj_t *found=find_visible_class(lv_obj_get_child(parent,(int32_t)i),cls); if (found) return found;
    }
    return NULL;
}
static unsigned find_wheels(lv_obj_t *parent,lv_obj_t **wheels,unsigned count)
{
    if (lv_obj_has_flag(parent,LV_OBJ_FLAG_HIDDEN)) return count;
    if (lv_obj_check_type(parent,&lv_roller_class)) { if (count<3) wheels[count]=parent; return count+1; }
    for (uint32_t i=0;i<lv_obj_get_child_count(parent);++i)
        count=find_wheels(lv_obj_get_child(parent,(int32_t)i),wheels,count);
    return count;
}
static void wheel_set(lv_obj_t *wheel,unsigned value)
{
    lv_roller_set_selected(wheel,value,LV_ANIM_OFF);
    lv_obj_send_event(wheel,LV_EVENT_VALUE_CHANGED,NULL);
}
static lv_indev_data_t wheel_touch;
static void wheel_touch_read(lv_indev_t *indev,lv_indev_data_t *data) { (void)indev; *data=wheel_touch; }
static void wheel_swipe(lv_obj_t *wheel)
{
    lv_display_set_rotation(display,LV_DISPLAY_ROTATION_0); lv_obj_update_layout(wheel);
    lv_area_t area; lv_obj_get_coords(wheel,&area);
    lv_indev_t *touch=lv_indev_create(); lv_indev_set_type(touch,LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(touch,display); lv_indev_set_read_cb(touch,wheel_touch_read);
    lv_indev_set_mode(touch,LV_INDEV_MODE_EVENT);
    wheel_touch=(lv_indev_data_t){.point={(area.x1+area.x2)/2,(area.y1+area.y2)/2+30},.state=LV_INDEV_STATE_PRESSED};
    for (int i=0;i<13;++i) {
        test_tick+=20; wheel_touch.point.y-=5; lv_indev_read(touch); lv_timer_handler();
    }
    wheel_touch.state=LV_INDEV_STATE_RELEASED; lv_indev_read(touch);
    for (int i=0;i<30;++i) { test_tick+=20; lv_timer_handler(); }
    lv_indev_delete(touch);
}
typedef struct { unsigned toggle,stop,start; tt_state_t before_release; } dial_result_t;
static dial_result_t dial_pointer_move(int x,int y,unsigned hold_ms,bool leave,int dx,int dy)
{
    dial_result_t result={0};
    lv_indev_t *touch=lv_indev_create(); lv_indev_set_type(touch,LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(touch,display); lv_indev_set_read_cb(touch,wheel_touch_read);
    lv_indev_set_mode(touch,LV_INDEV_MODE_EVENT);
    wheel_touch=(lv_indev_data_t){.point={x,y},.state=LV_INDEV_STATE_PRESSED};
    for (unsigned elapsed=0;elapsed<=hold_ms+20;elapsed+=20) {
        test_tick+=20;
        /* Inputs are raw screen coordinates. Map the requested logical dial
         * point back through the current 0/90/180/270-degree preview rotation. */
        lv_point_t raw={leave && elapsed>=100?450:x+(elapsed>=40?dx:0),y+(elapsed>=40?dy:0)};
        for (unsigned i=0;i<3;++i) lv_display_rotate_point(display,&raw);
        wheel_touch.point=raw;
        if (elapsed>hold_ms) { result.before_release=timer.state; wheel_touch.state=LV_INDEV_STATE_RELEASED; }
        lv_indev_read(touch);
        for (unsigned i=0;i<command_count;++i) {
            result.toggle+=commands[i].type==TT_CMD_TOGGLE;
            result.stop+=commands[i].type==TT_CMD_STOP;
            result.start+=commands[i].type==TT_CMD_START;
        }
        update(); lv_timer_handler();
    }
    lv_indev_delete(touch); return result;
}
static dial_result_t dial_pointer(int x,int y,unsigned hold_ms,bool leave)
{
    return dial_pointer_move(x,y,hold_ms,leave,0,0);
}
static bool dial_tap(void)
{
    dial_result_t r=dial_pointer(240,229,100,false);
    return r.toggle+r.start+r.stop==1;
}
static bool dial_hold(void)
{
    dial_result_t r=dial_pointer(240,229,1000,false);
    return r.stop==1 && !r.toggle && !r.start;
}
static bool has_label(lv_obj_t *parent,const char *text)
{
    if (lv_obj_has_flag(parent,LV_OBJ_FLAG_HIDDEN)) return false;
    if (lv_obj_check_type(parent,&lv_label_class) && !strcmp(lv_label_get_text(parent),text)) return true;
    for (uint32_t i=0;i<lv_obj_get_child_count(parent);++i)
        if (has_label(lv_obj_get_child(parent,(int32_t)i),text)) return true;
    return false;
}
static bool label_fits(lv_obj_t *parent,const char *text)
{
    if (lv_obj_has_flag(parent,LV_OBJ_FLAG_HIDDEN)) return false;
    if (lv_obj_check_type(parent,&lv_label_class) && !strcmp(lv_label_get_text(parent),text)) {
        lv_obj_update_layout(parent);
        lv_point_t size;
        lv_text_get_size(&size,text,lv_obj_get_style_text_font(parent,0),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        return size.x<=lv_obj_get_content_width(parent) && size.y<=lv_obj_get_content_height(parent);
    }
    for (uint32_t i=0;i<lv_obj_get_child_count(parent);++i)
        if (label_fits(lv_obj_get_child(parent,(int32_t)i),text)) return true;
    return false;
}
static lv_obj_t *find_background(lv_obj_t *parent,const lv_image_dsc_t *source)
{
    if (lv_obj_has_flag(parent,LV_OBJ_FLAG_HIDDEN)) return NULL;
    if (lv_obj_check_type(parent,&lv_image_class) && lv_image_get_src(parent)==source) return parent;
    for (uint32_t i=0;i<lv_obj_get_child_count(parent);++i) {
        lv_obj_t *found=find_background(lv_obj_get_child(parent,(int32_t)i),source);
        if (found) return found;
    }
    return NULL;
}
static bool keyboard_key(lv_obj_t *keys,const char *text)
{
    for (uint32_t i=0;i<60;++i) {
        const char *key=lv_buttonmatrix_get_button_text(keys,i);
        if (key && !strcmp(key,text)) {
            lv_buttonmatrix_set_selected_button(keys,i); lv_obj_send_event(keys,LV_EVENT_VALUE_CHANGED,NULL); return true;
        }
    }
    return false;
}
static bool press(const char *text)
{
    lv_obj_t *b=find_button(lv_screen_active(),text);
    if (!b) { fprintf(stderr,"Missing button: %s\n",text); return false; }
    lv_obj_send_event(b,LV_EVENT_CLICKED,NULL); update(); update();
    if (!strcmp(text,"Settings") || !strcmp(text,"设置")) { test_tick+=200; lv_timer_handler(); update(); }
    lv_refr_now(display); return true;
}
static bool check_action_animation(lv_obj_t *garden,tt_animation_clip_t clip)
{
    unsigned frames=0;
    int x0,y0,x,y; tt_garden_position(garden,&x0,&y0);
    tt_garden_walk_to(garden,12,54); /* Care cannot be interrupted by walking. */
    for (unsigned ms=0;ms<tt_animation_duration(clip);ms+=50) {
        tt_animation_t a=tt_garden_animation(garden);
        if (a.clip!=clip || !tt_garden_busy(garden)) return false;
        frames|=1u<<tt_animation_frame(&a,lv_tick_get());
        tt_garden_position(garden,&x,&y); if (x!=x0 || y!=y0) return false;
        weather_scenario=(ms/150)%6; /* Weather replacement must not restart a care action. */
        test_tick+=50; lv_timer_handler(); update();
        /* A weather blend may still show its previous scene while care continues. */
    }
    return frames==63 && !tt_garden_busy(garden);
}
static int brightness_smoke(void)
{
    update();
    unsigned saved=settings.brightness;
    if (!press("Settings") || !press("Display")) return 201;
    lv_obj_t *slider=find_visible_class(lv_screen_active(),&lv_slider_class);
    if (!slider) return 202;
    /* Hit and drag above the visible track, through the real input dispatcher. */
    lv_obj_update_layout(slider); lv_area_t track; lv_obj_get_coords(slider,&track);
    lv_point_t target={track.x1+(track.x2-track.x1)/4,track.y1-12};
    if (lv_indev_search_obj(lv_screen_active(),&target)!=slider) return 217;
    lv_indev_t *touch=lv_indev_create(); lv_indev_set_type(touch,LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(touch,display); lv_indev_set_read_cb(touch,wheel_touch_read);
    lv_indev_set_mode(touch,LV_INDEV_MODE_EVENT);
    wheel_touch=(lv_indev_data_t){.point=target,.state=LV_INDEV_STATE_PRESSED};
    lv_indev_read(touch); test_tick+=20; lv_indev_read(touch);
    wheel_touch.state=LV_INDEV_STATE_RELEASED; lv_indev_read(touch);
    if (lv_slider_get_value(slider)<25 || lv_slider_get_value(slider)>40) return 218;
    wheel_touch.state=LV_INDEV_STATE_PRESSED; test_tick+=20; lv_indev_read(touch);
    wheel_touch.point.x=track.x1+3*(track.x2-track.x1)/4;
    test_tick+=20; lv_indev_read(touch);
    wheel_touch.state=LV_INDEV_STATE_RELEASED; lv_indev_read(touch); lv_indev_delete(touch);
    if (tt_ui_display_brightness(saved,false)<65 || tt_ui_display_brightness(saved,false)>90 ||
        settings.brightness!=saved || command_count) return 219;
    target.y=track.y2+14;
    if (lv_indev_search_obj(lv_screen_active(),&target)!=slider) return 220;
    proof("brightness-touch");
    /* Real slider events must preview without queuing a settings write. */
    for (int value=10;value<=100;value+=5) {
        lv_slider_set_value(slider,value,LV_ANIM_OFF);
        lv_obj_send_event(slider,LV_EVENT_VALUE_CHANGED,NULL);
        if (tt_ui_display_brightness(saved,false)!=(unsigned)value ||
            settings.brightness!=saved || command_count) return 203;
    }
    if (!press("Back") || tt_ui_display_brightness(saved,false)!=saved) return 204;
    /* Saving the parent page must not resurrect a cancelled brightness draft. */
    if (!press("Save") || tt_ui_editing() || settings.brightness!=saved) return 214;
    if (!press("Settings") || !press("Display")) return 205;
    slider=find_visible_class(lv_screen_active(),&lv_slider_class);
    if (!slider || lv_slider_get_value(slider)!=(int)saved) return 206;
    lv_slider_set_value(slider,10,LV_ANIM_OFF);
    lv_obj_send_event(slider,LV_EVENT_VALUE_CHANGED,NULL);
    if (!press("Back") || !press("Display")) return 215;
    slider=find_visible_class(lv_screen_active(),&lv_slider_class);
    if (!slider || lv_slider_get_value(slider)!=(int)saved ||
        tt_ui_display_brightness(saved,false)!=saved) return 216;
    lv_slider_set_value(slider,27,LV_ANIM_OFF);
    lv_obj_send_event(slider,LV_EVENT_VALUE_CHANGED,NULL);
    /* Alarms preempt dim drafts, then restore the still-unsaved form. */
    if (tt_ui_display_brightness(saved,true)!=saved) return 207;
    tt_services_view_t services={0};
    tt_ui_render(&timer,face,&settings,&services,"",true);
    if (tt_ui_display_brightness(saved,false)!=saved) return 208;
    tt_ui_render(&timer,face,&settings,&services,"",false);
    if (tt_ui_display_brightness(saved,false)!=27 || settings.brightness!=saved) return 209;
    lv_obj_t *save=find_button(lv_screen_active(),"Save");
    if (!save) return 210;
    lv_obj_send_event(save,LV_EVENT_CLICKED,NULL);
    if (command_count!=1 || commands[0].type!=TT_CMD_SAVE || commands[0].settings.brightness!=27) return 211;
    command_count=0; tt_ui_save_result(false); update();
    if (!tt_ui_editing() || settings.brightness!=saved || tt_ui_display_brightness(saved,false)!=27) return 212;
    if (!press("Save") || tt_ui_editing() || settings.brightness!=27 ||
        tt_ui_display_brightness(settings.brightness,false)!=27) return 213;
    puts("PASS: live brightness, back cancels draft before parent save/reopen, alarm override, failed save/retry");
    return 0;
}
static int smoke(void)
{
    if (!settings.setup_complete) {
        proof("welcome-zh");
        if (!has_label(lv_screen_active(),"版本 " TT_PRODUCT_VERSION)) return 47;
        if (!tt_ui_editing() || !press("English")) return 10;
        if (!has_label(lv_screen_active(),"Version " TT_PRODUCT_VERSION)) return 47;
        proof("welcome-en");
        if (!press("Next")) return 10;
        if (!press("Home_Garden")) return 11;
        lv_obj_t *pass=find_field(lv_screen_active()); if (!pass) return 11;
        lv_obj_t *keys=find_visible_class(lv_screen_active(),&lv_keyboard_class); if (!keys) return 29;
        lv_obj_update_layout(lv_screen_active()); lv_area_t key_area; lv_obj_get_coords(keys,&key_area);
        if (key_area.y1<200 || key_area.y2>=480 || key_area.x1<0 || key_area.x2>=480) return 29;
        if (!keyboard_key(keys,"1") || !keyboard_key(keys,"ABC") || !keyboard_key(keys,"A") ||
            !keyboard_key(keys,"1#") || !keyboard_key(keys,"@") || strcmp(lv_textarea_get_text(pass),"1A@")) return 30;
        if (!keyboard_key(keys,LV_SYMBOL_BACKSPACE) || strcmp(lv_textarea_get_text(pass),"1A")) return 30;
        lv_textarea_set_text(pass,"short");
        lv_obj_t *connect_button=find_button(lv_screen_active(),"Connect");
        lv_obj_send_event(connect_button,LV_EVENT_CLICKED,NULL);
        if (command_count || settings.setup_complete) return 12;
        lv_textarea_set_text(pass,"wrongpass");
        if (!press("Connect") || mock_network.network_state!=TT_NET_FAILED || !find_button(lv_screen_active(),"Connect")) return 13;
        lv_textarea_set_text(pass,"preview123");
        lv_obj_send_event(connect_button,LV_EVENT_CLICKED,NULL);
        if (command_count!=1 || commands[0].type!=TT_CMD_NETWORK || strcmp(commands[0].settings.ssid,"Home_Garden")) return 13;
        update();
        if (find_button(lv_screen_active(),"Connect")) return 13;
        /* The status must update even without a new scan or another button tap. */
        tt_services_view_t live=mock_network;
        live.simulated=false; live.connected=true; live.network_state=TT_NET_CONNECTED;
        snprintf(live.ssid,sizeof(live.ssid),"Home_Garden");
        tt_network_ui_update(&live);
        if (!has_label(lv_screen_active(),"Connected · Home_Garden")) return 130;
        live.connected=false; live.network_state=TT_NET_CONNECTING;
        snprintf(live.network_message,sizeof(live.network_message),"Connecting...");
        tt_network_ui_update(&live);
        if (!has_label(lv_screen_active(),"Connecting... · Home_Garden") ||
            has_label(lv_screen_active(),"Connected · Home_Garden")) return 131;
        live.connected=true; live.network_state=TT_NET_CONNECTED;
        tt_network_ui_update(&live);
        if (!has_label(lv_screen_active(),"Connected · Home_Garden")) return 132;
        update();
        scan_scenario=1; if (!press("Refresh") || mock_network.ap_count) return 24;
        scan_scenario=2; if (!press("Refresh") || mock_network.scan_state!=TT_SCAN_FAILED) return 25;
        scan_scenario=0; if (!press("Refresh") || mock_network.ap_count!=6) return 26;
        if (!press("Guest_Open") || !press("Connect") || strcmp(simulated_ssid,"Guest_Open")) return 27;
        if (!press("Home_Garden")) return 34;
        pass=find_field(lv_screen_active()); lv_textarea_set_text(pass,"preview123");
        connect_button=find_button(lv_screen_active(),"Connect");
        lv_obj_send_event(connect_button,LV_EVENT_CLICKED,NULL);
        if (!press("Cancel") || strcmp(simulated_ssid,"Guest_Open")) return 35;
        if (!press("Other network") || !press("Cancel")) return 28;
        if (!press("Next") || !press("Dai Meow")) return 14;
        unsigned selected_frames=0;
        for (int i=0;i<24;++i) {
            advance(50);
            for (unsigned frame=0;frame<6;++frame)
                if (find_background(lv_screen_active(),tt_art_daimao[TT_ANIM_PLAY][frame])) selected_frames|=1u<<frame;
        }
        if (selected_frames!=63 || find_button(lv_screen_active(),"Dog") || find_button(lv_screen_active(),"Chick")) {
            fprintf(stderr,"Selected companion frames: %u\n",selected_frames); return 120;
        }
        proof("companion-picker");
        if (!press("Next") || !has_label(lv_screen_active(),"Display preferences")) return 121;
        proof("setup-display");
        if (!press("Next") || !has_label(lv_screen_active(),"Timer alarm")) return 122;
        proof("setup-sound");
        if (!press("Next") || !has_label(lv_screen_active(),"Weather location")) return 123;
        proof("setup-weather");
        if (!press("Next")) return 15;
        proof("setup-motion");
        lv_obj_t *finish=find_button(lv_screen_active(),"Finish");
        if (!finish) return 16;
        lv_obj_send_event(finish,LV_EVENT_CLICKED,NULL);
        if (!tt_ui_editing() || settings.setup_complete || command_count!=1) return 17;
        command_count=0; tt_ui_save_result(false); update();
        if (!tt_ui_editing() || settings.setup_complete) return 18;
        if (!press("Finish") || tt_ui_editing() || !settings.setup_complete || settings.pet!=TT_DAIMAO) return 19;
        if (!press("Settings")) return 20;
        lv_obj_t *lang=find_dropdown(lv_screen_active());
        lv_dropdown_set_selected(lang,TT_ZH); lv_obj_send_event(lang,LV_EVENT_VALUE_CHANGED,NULL); update();
        if (!press("保存") || settings.language!=TT_ZH) return 20;
        /* Switch back through the real settings page; canceled edits must not persist. */
        if (!press("设置") || !press("伙伴") || !press("呆猫") || !press("返回") || !press("返回") || settings.pet!=TT_DAIMAO) return 20;
        if (!press("设置")) return 21;
        lang=find_dropdown(lv_screen_active()); lv_dropdown_set_selected(lang,TT_EN);
        lv_obj_send_event(lang,LV_EVENT_VALUE_CHANGED,NULL); update();
        if (!press("Save") || settings.language!=TT_EN || tt_ui_editing()) return 22;
    } else if (tt_ui_editing()) return 23;
    if (!press("Settings") || !press("Battery and power") || !has_label(lv_screen_active(),"Simulated battery / Charging\n78%  ·  4.020 V\nBattery current: +26 mA")) return 160;
    proof("battery-charging");
    power_scenario=2; update(); proof("battery-low");
    if (!has_label(lv_screen_active(),"Simulated battery / On battery\n12%  ·  3.520 V\nBattery current: -108 mA")) return 161;
    power_scenario=4; update();
    if (!has_label(lv_screen_active(),"Simulated battery / Battery unavailable")) return 162;
    power_scenario=0; update();
    if (!press("Back")) return 163;
    if (!press("Pet care") || !tt_ui_pet_open() || tt_ui_editing()) return 31;
    lv_obj_t *garden=tt_pet_ui_garden(); int x0,y0,x1,y1;
    tt_garden_position(garden,&x0,&y0);
    bool moved=false; unsigned walk_frames=0;
    tt_garden_walk_to(garden,12,54); /* Long enough to observe a complete stride before autonomous wandering. */
    for (int i=0;i<300;++i) {
        test_tick+=50; lv_timer_handler(); update();
        tt_garden_position(garden,&x1,&y1);
        if (x1!=x0 || y1!=y0) moved=true;
        tt_animation_t a=tt_garden_animation(garden);
        if (a.clip==TT_ANIM_WALK) walk_frames|=1u<<tt_animation_frame(&a,lv_tick_get());
        if (x1<12 || x1>328 || y1<54 || y1>84) {
            fprintf(stderr,"Pet left care garden: %d,%d\n",x1,y1); return 32;
        }
    }
    if (!moved || walk_frames!=63) {
        fprintf(stderr,"Walking regression: moved=%d frames=0x%x visible=%d\n",moved,walk_frames,lv_obj_is_visible(garden)); return 32;
    }
    unsigned original=pets.pets[settings.pet].food;
    force_pet_save_failure=true;
    if (!press("Feed") || pets.pets[settings.pet].food!=original || !has_label(lv_screen_active(),"Care could not be saved; retry")) return 33;
    force_pet_save_failure=false;
    if (!press("Feed") || pets.pets[settings.pet].food<=original || pets.pets[settings.pet].meals!=1) return 36;
    if (!press("Feed") || pets.pets[settings.pet].meals!=1) return 37;
    if (!press("Play") || pets.pets[settings.pet].games!=0) return 43;
    if (!check_action_animation(garden,TT_ANIM_EAT)) return 44;
    if (!press("Play") || pets.pets[settings.pet].games!=1) return 38;
    if (!check_action_animation(garden,TT_ANIM_PLAY)) return 45;
    if (!press("Wash") || pets.pets[settings.pet].hygiene!=1000) return 39;
    if (!check_action_animation(garden,TT_ANIM_WASH)) return 46;
    if (!press("Sleep") || !pets.pets[settings.pet].resting) return 40;
    unsigned energy=pets.pets[settings.pet].energy;
    test_tick+=3000; update(); lv_timer_handler();
    if (pets.pets[settings.pet].energy<energy || !press("Wake up") || pets.pets[settings.pet].resting) return 41;
    if (!press("Growth journal") || !has_label(lv_screen_active(),"Focus milestones") || !press("Care") || !press("Back") || tt_ui_pet_open()) return 42;
    /* Exercise the actual renderer with all faces, rotations, completion and
     * settings navigation. Dummy video is only for automated regression tests. */
    tt_config_t c = settings.timer;
    for (int f = TT_HOME; f < TT_BACK; ++f) {
        tt_timer_face(&timer, &c, (tt_face_t)f, now_ms());
        lv_display_set_rotation(display, (lv_display_rotation_t)(tt_rotation((tt_face_t)f, false, 0) / 90));
        tt_services_view_t s = {.weather_valid = true, .connected = true, .temperature = 23.5f,
            .humidity = 55, .location = "DEMO weather", .updated = time(NULL)};
        tt_ui_render(&timer, (tt_face_t)f, &settings, &s, "Smoke test", false);
        lv_refr_now(display);
    }
    tt_timer_tick(&timer, timer.deadline_ms);
    tt_services_view_t offline = {.message = "DEMO offline"};
    tt_ui_render(&timer, TT_FRONT, &settings, &offline, "Completion", true);
    lv_refr_now(display);
    lv_display_set_rotation(display,LV_DISPLAY_ROTATION_0);
    if (!dial_tap()) return 52;
    /* Locate controls semantically; decorative artwork can change child ordering. */
    lv_obj_t *settings_button = find_button(lv_screen_active(), "Settings");
    if (!settings_button) return 2;
    lv_obj_send_event(settings_button, LV_EVENT_CLICKED, NULL);
    if (!tt_ui_editing()) return 2;
    if (!press("Timers") || find_button(lv_screen_active(),"Flat")) return 2;
    lv_refr_now(display);
    lv_obj_t *settings_screen = lv_screen_active();
    proof("timer-settings");
    lv_obj_t *wheels[3];
    lv_obj_t *save_button = find_button(settings_screen, "Save");
    if (find_wheels(settings_screen,wheels,0)!=3 || !save_button || find_field(settings_screen) ||
        find_visible_class(settings_screen,&lv_keyboard_class)) return 2;
    unsigned before=lv_roller_get_selected(wheels[1]);
    wheel_swipe(wheels[1]);
    if (lv_roller_get_selected(wheels[1])<=before) return 164;
    lv_obj_t *timer_body=lv_obj_get_parent(lv_obj_get_parent(wheels[0]));
    if (lv_obj_get_scroll_y(timer_body)!=0 || lv_obj_get_scroll_bottom(timer_body)>0) return 165;
    if (!press("60 min") || lv_roller_get_selected(wheels[0])!=1 || lv_roller_get_selected(wheels[1])!=0) return 166;
    wheel_set(wheels[1],12); wheel_set(wheels[2],34); wheel_set(wheels[0],24);
    if (lv_roller_get_selected(wheels[1]) || lv_roller_get_selected(wheels[2]) ||
        !lv_obj_has_state(wheels[1],LV_STATE_DISABLED)) return 167;
    wheel_set(wheels[0],0);
    if (lv_obj_has_state(wheels[1],LV_STATE_DISABLED)) return 168;
    lv_obj_send_event(save_button, LV_EVENT_CLICKED, NULL);
    if (!tt_ui_editing() || command_count) return 3;
    wheel_set(wheels[2],5);
    if (!press("Left") || !press("25 min") || !press("Right") || find_wheels(settings_screen,wheels,0)!=3 ||
        lv_roller_get_selected(wheels[0]) || lv_roller_get_selected(wheels[1]) || lv_roller_get_selected(wheels[2])!=5) return 169;
    if (!press("Upright behavior  >") || !press("Back") || find_wheels(settings_screen,wheels,0)!=3 ||
        lv_roller_get_selected(wheels[2])!=5) return 170;
    lv_obj_send_event(save_button, LV_EVENT_CLICKED, NULL);
    if (!tt_ui_editing() || command_count != 1 || commands[0].type != TT_CMD_SAVE ||
        commands[0].settings.timer.seconds[TT_RIGHT] != 5 || commands[0].settings.timer.seconds[TT_LEFT]!=1500) return 4;
    process_commands(now_ms());
    update();
    if (tt_ui_editing()) return 4;
    if (settings.timer.seconds[TT_RIGHT] != 5) return 5;
    lv_obj_send_event(settings_button, LV_EVENT_CLICKED, NULL);
    if (!press("Timers") || !press("60 min")) return 171;
    lv_obj_t *back = find_button(lv_screen_active(), "Back");
    if (!back) return 2;
    lv_obj_send_event(back, LV_EVENT_CLICKED, NULL);
    if (tt_ui_editing() || settings.timer.seconds[TT_RIGHT]!=5) return 3;
    /* Exercise the widest supported time and all icon classes. */
    timer.duration_s = 86400; timer.remaining_ms = 86400000; timer.state = TT_RUNNING;
    lv_display_set_rotation(display,LV_DISPLAY_ROTATION_0);
    const int codes[] = {0, 1, 3, 45, 61, 73, 85, 95, 97, 999, -1};
    for (unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i) {
        tt_services_view_t s = {.weather_valid = codes[i] >= 0, .weather_code = codes[i],
            .temperature = -12.5f, .humidity = 100, .location = "A long weather location to exercise clipping inside the small weather card"};
        tt_ui_render(&timer, TT_HOME, &settings, &s, "", false);
        for (int pass=0;pass<2;++pass) {
            test_tick+=700; lv_timer_handler(); tt_ui_render(&timer,TT_HOME,&settings,&s,"",false);
        }
        lv_refr_now(display);
        lv_obj_t *background=find_background(lv_screen_active(),tt_art_weather[tt_weather_kind(codes[i])]);
        if (!background) return 48;
        lv_area_t area; lv_obj_get_coords(background,&area);
        if (area.x1!=0 || area.y1!=0 || area.x2!=479 || area.y2!=479) return 49;
    }
    /* A cached observation keeps its scene; no valid observation uses the fallback. */
    tt_services_view_t cached_weather={.weather_valid=true,.connected=false,.weather_code=61,
        .updated=time(NULL)-3600,.location="Cached city"};
    tt_ui_render(&timer,TT_HOME,&settings,&cached_weather,"",false);
    for (int pass=0;pass<2;++pass) { test_tick+=700; lv_timer_handler(); tt_ui_render(&timer,TT_HOME,&settings,&cached_weather,"",false); }
    if (!find_background(lv_screen_active(),tt_art_weather[TT_WEATHER_RAIN])) return 50;
    cached_weather.weather_valid=false;
    tt_ui_render(&timer,TT_HOME,&settings,&cached_weather,"",false);
    for (int pass=0;pass<2;++pass) { test_tick+=700; lv_timer_handler(); tt_ui_render(&timer,TT_HOME,&settings,&cached_weather,"",false); }
    if (!find_background(lv_screen_active(),tt_art_weather[TT_WEATHER_UNKNOWN])) return 51;
    lv_refr_now(display);
    puts("PASS: fullscreen weather scenes/cache/fallback, weather changes during care, branded onboarding, scan/keyboard/retry, six-frame gait/eat/play/wash, save rollback/cooldown/sleep/journal, setup/i18n/rotations/timers");
    return 0;
}
static void advance(unsigned ms)
{
    for (unsigned i=0;i<ms;i+=20) { test_tick+=20; update(); lv_timer_handler(); }
    lv_refr_now(display);
}
static void flip(int axis) { input_axis=axis; advance(1000); }
/* Optional render artifacts from the headless regression, without touching a desktop window. */
static void proof_display(lv_display_t *target,const char *name)
{
    const char *dir=getenv("TT_PREVIEW_PROOF_DIR"); if (!dir || !*dir) return;
    lv_display_rotation_t saved_rotation=lv_display_get_rotation(target);
    lv_display_set_rotation(target,LV_DISPLAY_ROTATION_0); /* Review in the device's readable orientation. */
    lv_refr_now(target);
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,lv_display_get_horizontal_resolution(target),
        lv_display_get_vertical_resolution(target),32,SDL_PIXELFORMAT_ARGB8888);
    if (surface) {
        char path[1024]; snprintf(path,sizeof(path),"%s/%s.bmp",dir,name);
        if (SDL_RenderReadPixels(lv_sdl_window_get_renderer(target),NULL,surface->format->format,surface->pixels,surface->pitch)==0)
            (void)SDL_SaveBMP(surface,path);
        SDL_FreeSurface(surface);
    }
    lv_display_set_rotation(target,saved_rotation); lv_refr_now(target);
}
static void proof(const char *name) { proof_display(display,name); }
static void count_invalidations(lv_event_t *e)
{
    unsigned *count=lv_event_get_user_data(e); ++*count;
}
static bool unchanged_view_stays_clean(tt_face_t view_face,bool night)
{
    tt_timer_t held={.state=TT_PAUSED,.duration_s=300,.remaining_ms=120000,.active=TT_RIGHT};
    struct tm local={.tm_year=126,.tm_mon=3,.tm_mday=1,.tm_hour=night?22:12,.tm_isdst=-1};
    tt_services_view_t services={.weather_valid=true,.simulated=true,.weather_code=0,.temperature=23.5f,
        .humidity=55,.scene_time=mktime(&local)};
    tt_ui_render(&held,view_face,&settings,&services,"",false); lv_refr_now(display);
    /* Let the garden apply its night visibility before the next snapshot. */
    test_tick+=120; lv_timer_handler(); lv_refr_now(display);
    unsigned invalidations=0;
    lv_display_add_event_cb(display,count_invalidations,LV_EVENT_INVALIDATE_AREA,&invalidations);
    /* No timer handlers here: identical model snapshots must not redraw the
     * scene. Independent animation timers still own their normal cadence. */
    for (unsigned i=0;i<30;++i) tt_ui_render(&held,view_face,&settings,&services,"",false);
    lv_display_remove_event_cb_with_user_data(display,count_invalidations,&invalidations);
    if (invalidations) fprintf(stderr,"unchanged view %d night=%d invalidated %u times\n",view_face,night,invalidations);
    update(); return invalidations==0;
}
static bool subpixel_timer_stays_clean(void)
{
    tt_timer_t held={.state=TT_RUNNING,.duration_s=300,.remaining_ms=149700,.active=TT_RIGHT};
    tt_services_view_t services={.weather_valid=true,.simulated=true,.weather_code=0,.scene_time=1775001600};
    tt_ui_render(&held,TT_RIGHT,&settings,&services,"",false); lv_refr_now(display);
    unsigned invalidations=0;
    lv_display_add_event_cb(display,count_invalidations,LV_EVENT_INVALIDATE_AREA,&invalidations);
    /* Both snapshots show 02:30, 50%, and a 179-degree ring in LVGL 9.5. */
    held.remaining_ms=149400; tt_ui_render(&held,TT_RIGHT,&settings,&services,"",false);
    bool quiet=invalidations==0;
    held.remaining_ms=148900; tt_ui_render(&held,TT_RIGHT,&settings,&services,"",false);
    bool changed=invalidations>0 && has_label(lv_screen_active(),"02:29");
    lv_display_remove_event_cb_with_user_data(display,count_invalidations,&invalidations);
    if (!quiet || !changed) fprintf(stderr,"timer pixel boundary: quiet=%d changed=%d\n",quiet,changed);
    update(); return quiet && changed;
}
static int timer_smoke(void)
{
    flip(2);
    if (!unchanged_view_stays_clean(TT_HOME,false) || !unchanged_view_stays_clean(TT_RIGHT,false) ||
        !unchanged_view_stays_clean(TT_HOME,true) || !unchanged_view_stays_clean(TT_RIGHT,true)) return 133;
    if (!subpixel_timer_stays_clean()) return 219;
    proof("clock");
    if (face!=TT_HOME || find_button(lv_screen_active(),"Pause") || find_button(lv_screen_active(),"Stop")) return 60;
    if (!press("Pet care") || !tt_ui_pet_open() || tt_ui_editing()) return 61;
    flip(1);
    if (tt_ui_pet_open() || timer.state!=TT_RUNNING || timer.remaining_ms<299000 || timer.remaining_ms>300000 ||
        !has_label(lv_screen_active(),"Countdown") || find_button(lv_screen_active(),"Pet care") || rotation!=270) {
        fprintf(stderr,"flip: face=%d state=%d left=%lld rotation=%d pet=%d title=%d care=%p\n",
            face,timer.state,(long long)timer.remaining_ms,rotation,tt_ui_pet_open(),
            has_label(lv_screen_active(),"Countdown"),(void *)find_button(lv_screen_active(),"Pet care")); return 62;
    }
    proof("countdown");
    settings.language=TT_ZH; update(); proof("countdown-zh"); settings.language=TT_EN; update();
    test_tick+=60000; update(); lv_timer_handler();
    if (!has_label(lv_screen_active(),"04:00") || !has_label(lv_screen_active(),"Remaining  80%")) return 88;
    settings.language=TT_ZH; update(); proof("progress-zh"); settings.language=TT_EN; update();
    if (!dial_tap() || timer.state!=TT_PAUSED) return 63;
    settings.language=TT_ZH; update(); proof("paused-zh"); settings.language=TT_EN; update();
    int64_t paused=timer.remaining_ms; advance(1000);
    if (timer.remaining_ms!=paused || !dial_tap() || timer.state!=TT_RUNNING) return 64;
    dial_result_t gesture=dial_pointer(240,229,100,false);
    if (gesture.toggle!=1 || gesture.stop || timer.state!=TT_PAUSED) return 220;
    paused=timer.remaining_ms; advance(500);
    if (timer.remaining_ms!=paused) return 221;
    gesture=dial_pointer(370,229,100,false); /* Ring edge, outside the number label. */
    if (gesture.toggle!=1 || gesture.stop || timer.state!=TT_RUNNING) return 222;
    if (find_button(lv_screen_active(),"Pause") || find_button(lv_screen_active(),"Stop")) return 233;
    gesture=dial_pointer(402,229,100,false); /* Outer tick + finger margin. */
    if (gesture.toggle!=1 || timer.state!=TT_PAUSED) return 234;
    gesture=dial_pointer(240,229,500,false); /* Deliberate tap must not reset. */
    if (gesture.toggle!=1 || gesture.stop || timer.state!=TT_RUNNING) return 235;
    gesture=dial_pointer(240,229,0,false); /* One press sample, then release. */
    if (gesture.toggle!=1 || gesture.stop || timer.state!=TT_PAUSED) return 241;
    gesture=dial_pointer_move(240,229,180,false,24,-12); /* Finger drift inside the dial. */
    if (gesture.toggle!=1 || gesture.stop || timer.state!=TT_RUNNING) return 242;
    gesture=dial_pointer(240,229,1200,false);
    if (gesture.stop!=1 || gesture.toggle || gesture.start || gesture.before_release!=TT_IDLE || timer.state!=TT_IDLE) return 223;
    /* A finger tap can disturb the IMU after a hold. Manual start must use
     * the face already shown, without waiting for another 450ms settlement. */
    moving=true; gesture=dial_pointer(240,229,100,false);
    if (orientation.settled || gesture.toggle || gesture.stop || gesture.start!=1 ||
        timer.state!=TT_RUNNING || timer.active!=TT_RIGHT || timer.remaining_ms<299000) {
        fprintf(stderr,"restart while moving: start=%u settled=%d state=%d face=%d\n",
            gesture.start,orientation.settled,timer.state,face); return 224;
    }
    if (!dial_tap() || timer.state!=TT_PAUSED || !dial_tap() || timer.state!=TT_RUNNING) return 243;
    int64_t restarted_deadline=timer.deadline_ms;
    moving=false; advance(700);
    if (!orientation.settled || timer.deadline_ms!=restarted_deadline) return 244;
    gesture=dial_pointer(240,229,700,true); /* Slide off before the long-press threshold. */
    if (gesture.toggle || gesture.stop || timer.state!=TT_RUNNING) return 225;
    gesture=dial_pointer(90,80,700,false); /* Outside the circular hit shape. */
    if (gesture.toggle || gesture.stop || timer.state!=TT_RUNNING || !dial_tap()) { fprintf(stderr,"corner: toggle=%u stop=%u state=%d\n",gesture.toggle,gesture.stop,timer.state); return 226; }
    gesture=dial_pointer(240,229,1000,false);
    if (gesture.stop!=1 || gesture.toggle || gesture.start || timer.state!=TT_IDLE || !dial_tap()) return 227;
    flip(2); paused=timer.remaining_ms; advance(1000);
    if (timer.state!=TT_PAUSED || timer.remaining_ms!=paused || !find_button(lv_screen_active(),"Pet care")) return 65;
    flip(1);
    if (timer.remaining_ms<299000) return 66;
    const int axes[]={-1,-2};
    const tt_face_t faces[]={TT_LEFT,TT_INVERTED};
    for (unsigned i=0;i<2;++i) {
        flip(axes[i]);
        if (timer.active!=faces[i] || timer.state!=TT_RUNNING ||
            timer.duration_s!=settings.timer.seconds[faces[i]] || !has_label(lv_screen_active(),"Countdown")) return 67;
        gesture=dial_pointer(240,229,100,false);
        if (gesture.toggle!=1 || timer.state!=TT_PAUSED) return 228;
        gesture=dial_pointer(240,229,100,false);
        if (gesture.toggle!=1 || timer.state!=TT_RUNNING) return 229;
    }
    int flat_rotation=rotation; int64_t flat_deadline=timer.deadline_ms;
    flip(3);
    if (face!=TT_FRONT || rotation!=flat_rotation || timer.deadline_ms!=flat_deadline ||
        timer.active!=TT_INVERTED || has_label(lv_screen_active(),"Countdown") || find_button(lv_screen_active(),"Start")) return 134;
    flip(-3);
    if (face!=TT_UNKNOWN || rotation!=flat_rotation || timer.deadline_ms!=flat_deadline || timer.active!=TT_INVERTED) return 124;
    flip(-2);
    if (timer.deadline_ms!=flat_deadline) return 135;
    if (!dial_hold() || timer.state!=TT_IDLE || !has_label(lv_screen_active(),"Countdown") ||
        !has_label(lv_screen_active(),"Tap dial to start") || !has_label(lv_screen_active(),"10:00")) return 68;
    advance(1000); if (timer.state!=TT_IDLE || !dial_tap() || timer.state!=TT_RUNNING) return 69;
    settings.timer.seconds[TT_LEFT]=0; flip(-1);
    if (timer.state!=TT_IDLE || !has_label(lv_screen_active(),"This face is disabled") || find_button(lv_screen_active(),"Start")) return 70;
    gesture=dial_pointer(240,229,1000,false);
    if (gesture.toggle || gesture.stop || gesture.start || timer.state!=TT_IDLE) return 230;
    settings.timer.seconds[TT_RIGHT]=5;
    flip(2); if (!press("Pet care")) return 71;
    flip(1); advance(5000);
    if (timer.state!=TT_FINISHED || tt_ui_pet_open() || !has_label(lv_screen_active(),"Time is up!") ||
        !has_label(lv_screen_active(),"Tap dial to dismiss") || find_button(lv_screen_active(),"Pause")) return 72;
    proof("alarm");
    advance(3200); /* The old three-second reminder must no longer expire here. */
    if (!reminder.active || reminder.preview || reminder.config.volume!=settings.alarm.volume) return 164;
    settings.language=TT_ZH; update(); proof("alarm-zh"); settings.language=TT_EN; update();
    if (!dial_tap() || timer.state!=TT_IDLE || reminder.active) return 73;
    advance(1000); if (timer.state!=TT_IDLE || !dial_tap() || !press("Settings")) return 74;
    if (!press("Weather") || !press("Advanced: coordinates")) return 75;
    lv_obj_t *city=find_field(lv_screen_active()); if (!city) return 75;
    lv_textarea_set_text(city,"DraftCity");
    advance(5500);
    if (!has_label(lv_screen_active(),"Time is up!") || !tt_ui_editing() || !dial_tap() ||
        !tt_ui_editing() || strcmp(lv_textarea_get_text(find_field(lv_screen_active())),"DraftCity")) return 76;
    if (!press("Back") || !press("Back") || !press("Back")) return 77;
    /* Background policy still alerts when a pet page is open on the upright clock. */
    settings.timer.policy=TT_HOME_CONTINUE;
    if (!dial_tap()) return 78;
    flip(2); if (!press("Pet care")) return 79;
    advance(5000);
    if (timer.state!=TT_FINISHED || tt_ui_pet_open() || !has_label(lv_screen_active(),"Time is up!")) return 80;
    test_tick=(uint32_t)reminder.until_ms; update(); lv_timer_handler();
    if (reminder.active || timer.state!=TT_FINISHED || !has_label(lv_screen_active(),"Tap dial to dismiss")) return 165;
    if (!dial_tap() || !find_button(lv_screen_active(),"Pet care") || find_button(lv_screen_active(),"Stop")) return 81;
    /* Motion never starts a timer; stabilizing and restoring calibration do. */
    moving=true; flip(1); if (timer.state!=TT_IDLE) return 82;
    moving=false; advance(1000); if (timer.state!=TT_RUNNING) return 83;
    settings.timer.calibrated=false; tt_timer_init(&timer); flip(-1);
    if (timer.state!=TT_IDLE || face!=TT_UNKNOWN) return 84;
    settings.timer.calibrated=true; settings.timer.seconds[TT_LEFT]=1500; advance(1000);
    if (timer.state!=TT_RUNNING || timer.active!=TT_LEFT) return 85;
    settings.timer.seconds[TT_LEFT]=86400;
    if (!tt_timer_start(&timer,&settings.timer,TT_LEFT,now_ms())) return 86;
    update(); lv_refr_now(display);
    if (!label_fits(lv_screen_active(),"1440:00")) return 87;
    proof("maximum-duration");
    if (!dial_hold()) return 89;
    settings.timer.seconds[TT_LEFT]=60;
    if (!dial_tap()) return 90;
    unsigned completed=pets.focus.sessions;
    test_tick+=60000; update();
    if (pets.focus.sessions!=completed+1 || !(pets.focus.badges&1)) return 91;
    update(); if (pets.focus.sessions!=completed+1) return 92;
    proof("achievement");
    if (!dial_tap()) return 93;
    flip(2); settings.language=TT_ZH; update();
    if (!press("陪伴") || !press("专注成就")) return 94;
    proof("achievements-zh");
    if (!press("返回")) return 95;
    if (!press("设置") || !press("计时")) return 96;
    proof("timer-settings-zh");
    if (!press("返回") || find_button(lv_screen_active(),"语音助手") ||
        find_button(lv_screen_active(),"Voice assistant")) return 97;
    proof("home-without-voice-zh");
    flip(1); update();
    if (!dial_hold()) return 102;
    flip(2); weather_scenario=0; advance(1500); proof("weather-before");
    weather_scenario=3; update();
    for (int i=0;i<6;++i) { advance(50); char name[40]; snprintf(name,sizeof(name),"weather-transition-%02d",i); proof(name); }
    proof("weather-blend");
    lv_obj_t *incoming=find_background(lv_screen_active(),tt_art_weather[TT_WEATHER_SNOW]);
    if (!incoming || lv_obj_get_style_image_opa(incoming,0)==0 || lv_obj_get_style_image_opa(incoming,0)==255) return 103;
    for (int i=6;i<14;++i) { advance(50); char name[40]; snprintf(name,sizeof(name),"weather-transition-%02d",i); proof(name); }
    proof("weather-after");
    settings.timer.seconds[TT_RIGHT]=5; flip(1);
    test_tick=(uint32_t)timer.deadline_ms; update();
    gesture=dial_pointer(240,229,100,false);
    if (gesture.stop!=1 || gesture.toggle || gesture.start || timer.state!=TT_IDLE || reminder.active) return 231;
    if (!dial_tap()) return 231;
    test_tick=(uint32_t)timer.deadline_ms; update();
    gesture=dial_pointer(240,229,1000,false);
    if (gesture.stop!=1 || gesture.toggle || gesture.start || timer.state!=TT_IDLE || reminder.active || interaction.alarm_active) return 232;
    /* A completed timer consumes exactly one stable turn, including flat/down.
     * Expiring while already flat or upright must not dismiss immediately. */
    settings.timer.policy=TT_HOME_CONTINUE;
    settings.timer.seconds[TT_RIGHT]=5; settings.timer.seconds[TT_LEFT]=10;
    const int end_axes[]={1,1,1,1,3,2};
    const int dismiss_axes[]={2,-1,3,-3,1,1};
    for (unsigned i=0;i<sizeof(end_axes)/sizeof(*end_axes);++i) {
        tt_timer_init(&timer); tt_alarm_stop(&reminder); alarm=false;
        flip(1);
        if (end_axes[i]!=1) flip(end_axes[i]);
        test_tick=(uint32_t)timer.deadline_ms; update();
        if (timer.state!=TT_FINISHED || !reminder.active || reminder.origin_axis!=end_axes[i]) {
            fprintf(stderr,"turn scenario %u: state=%d active=%d origin=%d expected=%d\n",
                i,timer.state,reminder.active,reminder.origin_axis,end_axes[i]); return 168;
        }
        moving=true; input_axis=dismiss_axes[i]; advance(200);
        if (timer.state!=TT_FINISHED || !reminder.active) return 169;
        moving=false; advance(400);
        if (timer.state!=TT_FINISHED || !reminder.active) return 170; /* Dwell rejects brief shakes. */
        advance(700);
        if (timer.state!=TT_IDLE || reminder.active || alarm || interaction.alarm_active) return 171;
        advance(1000); if (timer.state!=TT_IDLE) return 172; /* No timer on the dismissing pose. */
        flip(dismiss_axes[i]==1?-1:1);
        if (timer.state!=TT_RUNNING) return 173; /* The next separate flip works. */
    }
    /* The finished overlay must accept dismissal above an unsaved settings form. */
    settings.language=TT_EN; tt_timer_init(&timer); tt_alarm_stop(&reminder); alarm=false; flip(1);
    if (!press("Settings") || !press("Weather") || !press("Advanced: coordinates")) return 174;
    city=find_field(lv_screen_active()); if (!city) return 175;
    lv_textarea_set_text(city,"KeepTurnDraft");
    test_tick=(uint32_t)timer.deadline_ms; update(); advance(100);
    if (timer.state!=TT_FINISHED || !tt_ui_editing() || !reminder.active) return 176;
    flip(2);
    city=find_field(lv_screen_active());
    if (timer.state!=TT_IDLE || reminder.active || !tt_ui_editing() || !city ||
        strcmp(lv_textarea_get_text(city),"KeepTurnDraft")) return 177;
    if (!press("Back") || !press("Back") || !press("Back")) return 178;
    puts("PASS: real IMU filter -> all presets/rotation -> pause/resume/home restart -> stop/start/disabled -> alarm over care/settings, draft retained, motion/calibration guards");
    return 0;
}
static int weather_smoke(void)
{
    settings.language=TT_ZH; flip(2);
    tt_pet_book_t original=pets;
    for (unsigned species=0;species<TT_PET_COUNT;++species) {
        settings.pet=(uint8_t)species; update();
        for (unsigned w=0;w<6;++w) {
            weather_scenario=w; advance(1500);
            lv_obj_t *bg=find_background(lv_screen_active(),tt_art_weather[tt_weather_kind(weather_codes[w])]);
            if (!bg) return 110;
            lv_obj_t *garden=lv_obj_get_parent(bg);
            bool sleeping=false;
            for (unsigned ms=0;ms<9000;ms+=100) {
                advance(100);
                int x,y; tt_garden_position(garden,&x,&y);
                if (x<12 || x>360 || y<260 || y>294) return 111;
                sleeping=tt_garden_animation(garden).clip==TT_ANIM_SLEEP;
                if ((w==3 || w==5)?sleeping:ms>=2400) break;
            }
            if ((w==3 || w==5) && !sleeping) return 112;
            char name[48]; snprintf(name,sizeof(name),"pet-weather-%u-%u",species,w); proof(name);
            /* Weather changes walking pace, but a ground tap still works in every scene. */
            int x,y,x2,y2; tt_garden_position(garden,&x,&y);
            tt_garden_walk_to(garden,x<180?x+140:x-140,y);
            advance(500); tt_garden_position(garden,&x2,&y2);
            int distance=abs(x2-x);
            if (tt_garden_animation(garden).clip!=TT_ANIM_WALK || distance<12 || distance>42) return 113;
            if ((w==0 && distance<18) || (w==4 && distance>19)) return 114;
        }
        /* Explicit sleep survives weather changes; weather never writes a care penalty. */
        pets.pets[species].resting=1; update(); weather_scenario=0; advance(1500);
        lv_obj_t *bg=find_background(lv_screen_active(),tt_art_weather[TT_WEATHER_CLEAR]);
        if (!bg || tt_garden_animation(lv_obj_get_parent(bg)).clip!=TT_ANIM_SLEEP ||
            !pets.pets[species].resting) return 115;
        pets.pets[species].resting=0; update();
        if (pets.pets[species].xp!=original.pets[species].xp ||
            pets.pets[species].bond!=original.pets[species].bond) return 116;
    }
    weather=false; advance(1500);
    if (!find_background(lv_screen_active(),tt_art_weather[TT_WEATHER_UNKNOWN])) return 117;
    proof("pet-weather-unknown");
    weather=true; weather_scenario=5; advance(1500);
    if (!press("陪伴")) return 118;
    advance(800); proof("pet-weather-care");
    flip(1);
    if (tt_ui_pet_open() || timer.state!=TT_RUNNING || !has_label(lv_screen_active(),"倒计时")) return 119;
    proof("pet-weather-timer-priority");
    puts("PASS: three pets / six weather scenes, safe bounds, weather-specific walking pace, snow/storm rest, manual walk/sleep priority, no XP/bond penalty, offline fallback, flip timer over weather companion");
    return 0;
}
static int scene_smoke(void)
{
    settings.language=TT_ZH; flip(2); weather_scenario=0;
    for (unsigned species=0;species<TT_PET_COUNT;++species) {
        settings.pet=(uint8_t)species; update();
        for (int season=0;season<4;++season) {
            simulated_season=season; simulated_period=1; weather_scenario=season==3?3:0;
            advance(800);
            char name[50]; snprintf(name,sizeof(name),"scene-pet-%u-season-%d",species,season); proof(name);
        }
    }
    settings.pet=TT_DAIMAO; simulated_season=1; weather_scenario=0;
    for (int period=0;period<4;++period) {
        simulated_period=period; advance(1700);
        char name[40]; snprintf(name,sizeof(name),"scene-time-%d",period); proof(name);
    }
    simulated_period=1; simulated_season=0; weather_scenario=2; advance(1200);
    proof("scene-rain-a"); advance(220); proof("scene-rain-b");
    flip(1); advance(800); proof("scene-timer-weather");
    if (has_label(lv_screen_active(),"Time-Tick V1.0") || timer.state!=TT_RUNNING) return 150;
    flip(2); advance(250); proof("scene-paused-home");
    if (timer.state!=TT_PAUSED) return 151;
    /* Check every idle frame can be rendered, including wearables, without clipping neighbours. */
    lv_obj_t *canvas=lv_obj_create(NULL); lv_obj_remove_style_all(canvas); lv_obj_set_size(canvas,480,480);
    lv_obj_set_style_bg_color(canvas,lv_color_hex(TT_PAPER),0); lv_obj_set_style_bg_opa(canvas,255,0);
    lv_screen_load(canvas);
    for (int pet=0;pet<TT_PET_COUNT;++pet) for (int season=0;season<4;++season) {
        lv_obj_t *sprite=tt_sprite_create(canvas,8+season*117,40+pet*137,(tt_pet_t)pet);
        tt_sprite_environment(sprite,TT_WEATHER_CLEAR,(tt_scene_t){.season=(tt_season_t)season,.daylight=TT_DAY});
    }
    for (int frame=0;frame<6;++frame) {
        test_tick=(test_tick/12000+1)*12000+(uint32_t[]){1000,2750,2900,6100,9700,11000}[frame];
        lv_timer_handler(); char name[40]; snprintf(name,sizeof(name),"wardrobe-idle-%d",frame); proof(name);
    }
    /* Inspect actual fitted scarf rendering: three species, idle and both
     * walking directions, all six frames. No timer pass between individual
     * renders: each species has a deliberately staggered idle phase. */
    lv_obj_clean(canvas);
    lv_obj_t *fitted[3][3];
    for (int pet=0;pet<TT_PET_COUNT;++pet) for (int pose=0;pose<3;++pose)
        fitted[pet][pose]=tt_sprite_create(canvas,24+pose*155,30+pet*145,(tt_pet_t)pet);
    for (int frame=0;frame<6;++frame) {
        uint32_t epoch=(test_tick/12000+2)*12000;
        for (int pet=0;pet<TT_PET_COUNT;++pet) {
            test_tick=epoch+(uint32_t[]){1000,2750,2900,6100,9700,11000}[frame]-(uint32_t)pet*1193;
            for (int pose=0;pose<3;++pose) {
                lv_obj_t *sprite=fitted[pet][pose];
                tt_sprite_environment(sprite,TT_WEATHER_CLEAR,(tt_scene_t){.season=TT_AUTUMN,.daylight=TT_DAY});
                tt_sprite_set(sprite,(tt_pet_t)pet,TT_ANIM_IDLE,false);
                if (pose) {
                    tt_sprite_set(sprite,(tt_pet_t)pet,TT_ANIM_WALK,pose==2);
                    tt_sprite_walk(sprite,(uint32_t)frame*(TT_STRIDE_PIXELS*256/TT_ANIMATION_FRAMES));
                }
            }
        }
        char name[40]; snprintf(name,sizeof(name),"scarf-fit-%d",frame); proof(name);
    }
    /* Every pose shares environmental light, without making the pet transparent.
     * A return to daylight must restore the original artwork, including after
     * a weather/animation change during the fade. */
    lv_obj_t *lit=fitted[0][0];
    lv_obj_t *art=lv_obj_get_child(lit,1);
    tt_scene_t night={.season=TT_SUMMER,.daylight=TT_NIGHT};
    tt_sprite_environment(lit,TT_WEATHER_CLEAR,night);
    test_tick+=300; lv_timer_handler();
    unsigned middle=lv_obj_get_style_image_recolor_opa(art,0);
    if (middle==0 || middle>=150) return 236;
    test_tick+=400; lv_timer_handler();
    unsigned dark=lv_obj_get_style_image_recolor_opa(art,0);
    if (dark<100 || lv_obj_get_style_image_opa(art,0)!=255) return 237;
    for (int clip=TT_ANIM_IDLE;clip<=TT_ANIM_SLEEP;++clip) {
        tt_sprite_set(lit,TT_DAIMAO,(tt_animation_clip_t)clip,false);
        test_tick+=100; lv_timer_handler();
        if (lv_obj_get_style_image_recolor_opa(art,0)!=dark) return 238;
    }
    tt_sprite_environment(lit,TT_WEATHER_RAIN,night);
    if (lv_obj_get_style_image_recolor_opa(art,0)!=dark) return 239;
    night.daylight=TT_DAY; tt_sprite_environment(lit,TT_WEATHER_CLEAR,night);
    test_tick+=700; lv_timer_handler();
    if (lv_obj_get_style_image_recolor_opa(art,0)!=0) return 240;
    puts("PASS: seasonal and daylight scene rendering, weather countdown and paused home, all costume/idle poses");
    return 0;
}
static lv_obj_t *nth_control(lv_obj_t *parent,const lv_obj_class_t *cls,unsigned *index)
{
    if (lv_obj_check_type(parent,cls)) { if (!*index) return parent; --*index; }
    for (uint32_t i=0;i<lv_obj_get_child_count(parent);++i) {
        lv_obj_t *found=nth_control(lv_obj_get_child(parent,(int32_t)i),cls,index);
        if (found) return found;
    }
    return NULL;
}
static int setup_smoke(void)
{
    proof("setup-language-zh");
    if (!press("下一步") || !press("离线继续") || !press("呆猫")) return 130;
    advance(500); proof("setup-companion-zh");
    if (!press("下一步")) return 131;
    proof("setup-display-zh");
    if (!press("主题与时段  >") || !press("夜间") || !tt_theme_is_night() ||
        !press("应用") || !tt_ui_editing() || settings.theme.mode!=TT_THEME_AUTO) return 274;
    if (!press("下一步")) return 132;
    proof("setup-sound-zh");
    unsigned alarm_index=0;
    lv_obj_t *ring=nth_control(lv_screen_active(),&lv_dropdown_class,&alarm_index);
    alarm_index=1; lv_obj_t *length=nth_control(lv_screen_active(),&lv_dropdown_class,&alarm_index);
    alarm_index=0; lv_obj_t *loudness=nth_control(lv_screen_active(),&lv_slider_class,&alarm_index);
    if (!ring || !length || !loudness) return 157;
    if (lv_dropdown_get_option_count(ring)!=TT_RING_COUNT) return 166;
    lv_dropdown_open(ring); advance(100); proof("ringtone-list-zh"); lv_dropdown_close(ring);
    for (unsigned tone=0;tone<TT_RING_COUNT;++tone) {
        lv_dropdown_set_selected(ring,tone);
        if (!press("试听 3 秒") || reminder.config.ringtone!=tone || !reminder.active ||
            settings.alarm.ringtone!=TT_RING_BELL || !press("停止试听") || reminder.active) return 167;
    }
    lv_dropdown_set_selected(ring,TT_RING_PULSE); lv_dropdown_set_selected(length,2);
    lv_slider_set_value(loudness,73,LV_ANIM_OFF); lv_obj_send_event(loudness,LV_EVENT_VALUE_CHANGED,NULL);
    if (!press("试听 3 秒") || !reminder.active || !reminder.preview || reminder.config.volume!=73 ||
        reminder.config.ringtone!=TT_RING_PULSE || reminder.config.duration_s!=60 || settings.alarm.volume!=65) return 158;
    proof("alarm-preview-zh");
    if (!press("停止试听") || reminder.active) return 159;
    if (!press("试听 3 秒")) return 160;
    advance(3200); if (reminder.active) return 161;
    if (!press("下一步")) return 133;
    if (reminder.active) return 162;
    proof("setup-weather-zh");
    if (!press("联网定位当前位置") || !has_label(lv_screen_active(),"杭州") || settings.precise_location) return 134;
    proof("location-ip-confirm-zh");
    if (!press("返回") || settings.precise_location) return 135;
    lookup_scenario=2;
    if (!press("联网定位当前位置") || !has_label(lv_screen_active(),tt_text(TT_ZH,"Connect Wi-Fi, then try again"))) return 144;
    if (!press("返回")) return 145;
    lookup_scenario=3;
    if (!press("联网定位当前位置") || !has_label(lv_screen_active(),tt_text(TT_ZH,"Lookup unavailable. Retry or choose another method"))) return 146;
    if (!press("返回") || !press("搜索城市")) return 147;
    unsigned index=0;
    lv_obj_t *query=nth_control(lv_screen_active(),&lv_textarea_class,&index);
    if (!query) return 148;
    lv_textarea_set_text(query,"hangzhou"); lookup_scenario=1;
    if (!press("搜索") || !has_label(lv_screen_active(),tt_text(TT_ZH,"No matches. Try pinyin or a nearby city"))) return 149;
    lookup_scenario=0;
    if (!press("搜索") || !has_label(lv_screen_active(),"杭州湾") || settings.precise_location) return 152;
    proof("location-search-results-zh");
    if (!press("返回") || !press("常用城市")) return 153;
    proof("location-common-zh");
    if (!press("杭州") || !press("杭州") || settings.precise_location) return 154;
    if (!press("高级：手动经纬度")) return 155;
    index=0; lv_obj_t *name=nth_control(lv_screen_active(),&lv_textarea_class,&index);
    index=1; lv_obj_t *lat=nth_control(lv_screen_active(),&lv_textarea_class,&index);
    index=2; lv_obj_t *lon=nth_control(lv_screen_active(),&lv_textarea_class,&index);
    if (!name || !lat || !lon) return 136;
    lv_textarea_set_text(name,"West Lake");
    lv_textarea_set_text(lat,"91"); lv_textarea_set_text(lon,"120.155100");
    if (!press("使用这个地点") || settings.precise_location ||
        !has_label(lv_screen_active(),tt_text(TT_ZH,"Enter valid WGS84 latitude / longitude"))) return 137;
    lv_textarea_set_text(lat,"30.274100");
    if (!press("使用这个地点") || !press("下一步")) return 138;
    proof("setup-motion-zh");
    if (!press("进入小院") || tt_ui_editing() || !settings.precise_location ||
        settings.latitude_e6!=30274100 || settings.longitude_e6!=120155100 || settings.pet!=TT_DAIMAO) return 139;
    if (settings.alarm.volume!=73 || settings.alarm.ringtone!=TT_RING_PULSE || settings.alarm.duration_s!=60 ||
        settings.theme.mode!=TT_THEME_NIGHT) return 163;
    if (!press("设置")) return 140;
    proof("settings-categories-zh");
    if (!press("天气")) return 141;
    proof("location-settings-zh");
    if (!press("高级：手动经纬度")) return 156;
    proof("weather-coordinates-zh");
    index=1; lat=nth_control(lv_screen_active(),&lv_textarea_class,&index);
    if (!lat) return 142;
    lv_textarea_set_text(lat,"-12.5");
    if (!press("使用这个地点") || !press("返回") || !press("返回") || settings.latitude_e6!=30274100) return 143;
    lv_display_t *controls=controls_create(); advance(500);
    proof_display(controls,"controls-zh");
    puts("PASS: Chinese seven-page setup, location confirmation/search/ambiguity/empty/offline/failure, popular cities, coordinate validation/save/cancel, Chinese controls");
    return 0;
}
static time_t theme_clock(int hour,int minute)
{
    struct tm clock={.tm_year=126,.tm_mon=9,.tm_mday=21,.tm_hour=hour,.tm_min=minute,.tm_isdst=-1};
    return mktime(&clock);
}
static bool theme_background(lv_obj_t *obj,uint32_t color,lv_part_t part)
{
    return obj && lv_color_eq(lv_obj_get_style_bg_color(obj,part),lv_color_hex(color));
}
static int theme_options_smoke(void)
{
    if (!press("设置") || !press("显示")) return 275;
    lv_obj_t *entry=find_button(lv_screen_active(),"主题与时段  >");
    lv_area_t a,b; lv_obj_get_coords(entry,&a); lv_obj_get_coords(lv_obj_get_parent(entry),&b);
    if (a.y2>b.y2 || !press("主题与时段  >")) return 276;
    lv_obj_t *wheels[4];
    for (unsigned i=0;i<4;++i) { unsigned n=i; wheels[i]=nth_control(lv_screen_active(),&lv_roller_class,&n); if (!wheels[i]) return 277; }
    if (!press("夜间") || !tt_theme_is_night() || !lv_obj_has_state(wheels[0],LV_STATE_DISABLED) ||
        settings.theme.mode!=TT_THEME_AUTO) return 278;
    proof("theme-options-night");
    if (!press("返回") || tt_theme_is_night() || !press("返回") || !press("保存") || settings.theme.mode!=TT_THEME_AUTO) return 279;
    if (!press("设置") || !press("显示") || !press("主题与时段  >") || !press("日间")) return 280;
    simulated_period=3; advance(1000);
    if (tt_theme_is_night() || !press("保存") || settings.theme.mode!=TT_THEME_DAY || tt_ui_editing()) return 281;
    proof("theme-forced-day-home");
    if (!press("设置") || !press("显示") || !press("主题与时段  >") || !press("夜间") || !press("保存")) return 282;
    simulated_period=1; advance(1000);
    if (!tt_theme_is_night() || settings.theme.mode!=TT_THEME_NIGHT) return 283;
    proof("theme-forced-night-home");
    if (!press("设置") || !press("显示") || !press("主题与时段  >") || !press("自动")) return 284;
    for (unsigned i=0;i<4;++i) { unsigned n=i; wheels[i]=nth_control(lv_screen_active(),&lv_roller_class,&n); }
    if (lv_obj_has_state(wheels[0],LV_STATE_DISABLED)) return 285;
    wheel_set(wheels[0],7); wheel_set(wheels[1],20); wheel_set(wheels[2],7); wheel_set(wheels[3],20);
    if (!press("保存") || !tt_ui_editing() || !has_label(lv_screen_active(),"开始和恢复时间不能相同")) return 286;
    wheel_set(wheels[0],22); wheel_set(wheels[1],35);
    proof("theme-options-custom");
    /* Storage failure must retain the chosen mode and time for a retry. */
    lv_obj_send_event(find_button(lv_screen_active(),"保存"),LV_EVENT_CLICKED,NULL);
    if (command_count!=1 || commands[0].type!=TT_CMD_SAVE) return 287;
    command_count=0; tt_ui_save_result(false); update(); lv_timer_handler();
    if (!tt_ui_editing() || lv_roller_get_selected(wheels[0])!=22 || !press("保存") ||
        settings.theme.mode!=TT_THEME_AUTO || settings.theme.night_start!=1355 || settings.theme.night_end!=440) return 288;
    tt_theme_time(theme_clock(22,34),&settings.theme); if (tt_theme_is_night()) return 289;
    tt_theme_time(theme_clock(22,35),&settings.theme); if (!tt_theme_is_night()) return 290;
    tt_theme_time(theme_clock(7,19),&settings.theme); if (!tt_theme_is_night()) return 291;
    tt_theme_time(theme_clock(7,20),&settings.theme); if (tt_theme_is_night()) return 292;
    /* Restore defaults through the UI, so the following boundary tests stay independent. */
    if (!press("设置") || !press("显示") || !press("主题与时段  >")) return 293;
    for (unsigned i=0;i<4;++i) { unsigned n=i; wheels[i]=nth_control(lv_screen_active(),&lv_roller_class,&n); }
    wheel_set(wheels[0],20); wheel_set(wheels[1],0); wheel_set(wheels[2],5); wheel_set(wheels[3],0);
    if (!press("保存") || tt_ui_editing()) return 294;
    return 0;
}
static int theme_smoke(void)
{
    settings.language=TT_ZH; simulated_period=1; flip(2);
    int options=theme_options_smoke(); if (options) return options;
    if (tt_theme_is_night()) return 250;
    proof("theme-day-home");
    if (!press("设置") || !press("系统") || !press("显示")) return 251;
    lv_obj_t *editor=lv_screen_active(); unsigned index=0;
    lv_obj_t *slider=nth_control(editor,&lv_slider_class,&index);
    if (!slider) return 252;
    unsigned saved=settings.brightness;
    lv_slider_set_value(slider,31,LV_ANIM_OFF); lv_obj_send_event(slider,LV_EVENT_VALUE_CHANGED,NULL);
    index=0; lv_obj_t *zone=nth_control(editor,&lv_dropdown_class,&index);
    if (!zone) return 253;
    unsigned selected=lv_dropdown_get_selected(zone);
    lv_dropdown_open(zone); advance(100); proof("theme-day-dropdown");
    simulated_period=3; advance(900);
    if (!tt_theme_is_night() || lv_screen_active()!=editor || !lv_dropdown_is_open(zone) ||
        lv_dropdown_get_selected(zone)!=selected || lv_slider_get_value(slider)!=31 || settings.brightness!=saved ||
        tt_ui_display_brightness(saved,false)!=31 || !theme_background(editor,0x121f29,0) ||
        !theme_background(lv_dropdown_get_list(zone),0x20313b,0)) return 254;
    proof("theme-night-dropdown"); lv_dropdown_close(zone); proof("theme-night-display");
    /* Boundary and invalid-clock handling, while an unsaved editor stays open. */
    if (!tt_theme_time(theme_clock(19,59),&settings.theme) || tt_theme_is_night() || !theme_background(editor,TT_PAPER,0) ||
        !tt_theme_time(theme_clock(20,0),&settings.theme) || !tt_theme_is_night() || tt_theme_time(theme_clock(0,0),&settings.theme) ||
        tt_theme_time(theme_clock(4,59),&settings.theme) || tt_theme_time(0,&settings.theme) || !tt_theme_is_night() ||
        !tt_theme_time(theme_clock(5,0),&settings.theme) || tt_theme_is_night() || lv_slider_get_value(slider)!=31) return 255;
    advance(100);
    if (!press("返回") || !press("返回") || settings.brightness!=saved) return 256;
    if (!press("设置") || !press("网络") || !press("其他网络")) return 257;
    index=0; lv_obj_t *field=nth_control(lv_screen_active(),&lv_textarea_class,&index);
    lv_obj_t *keys=find_visible_class(lv_screen_active(),&lv_keyboard_class);
    if (!field || !keys) return 258;
    lv_textarea_set_text(field,"theme-test"); lv_textarea_set_cursor_pos(field,4);
    proof("theme-night-keyboard"); simulated_period=1; advance(100);
    if (strcmp(lv_textarea_get_text(field),"theme-test") || lv_textarea_get_cursor_pos(field)!=4 ||
        !lv_obj_is_visible(keys) || !theme_background(keys,TT_CREAM,LV_PART_ITEMS)) return 259;
    simulated_period=3; advance(100);
    if (!theme_background(keys,0x20313b,LV_PART_ITEMS) || strcmp(lv_textarea_get_text(field),"theme-test")) return 260;
    if (!press("取消") || !press("计时")) return 261;
    lv_obj_t *wheels[3]; if (find_wheels(lv_screen_active(),wheels,0)!=3) return 262;
    wheel_set(wheels[1],17); wheel_set(wheels[2],35); proof("theme-night-wheels");
    simulated_period=1; advance(100); simulated_period=3; advance(100);
    if (lv_roller_get_selected(wheels[1])!=17 || lv_roller_get_selected(wheels[2])!=35 || !press("返回")) return 263;
    if (!press("陪伴") || !press("专注成就")) return 264;
    proof("theme-night-achievements");
    if (!theme_background(lv_screen_active(),0x121f29,0) || !press("返回")) return 265;
    /* Repeated page deletion + palette changes exercise binding lifetime. */
    for (int i=0;i<20;++i) {
        if (!press("设置") || !press("系统") || !press("到时提醒")) return 266;
        simulated_period=i%2?3:1; advance(20);
        if (!press("返回") || !press("返回")) return 267;
    }
    simulated_period=3; advance(900); proof("theme-night-home");
    flip(1); proof("theme-night-timer");
    int64_t deadline=timer.deadline_ms;
    simulated_period=1; advance(100); simulated_period=3; advance(100);
    if (timer.state!=TT_RUNNING || timer.deadline_ms!=deadline || !dial_tap() || timer.state!=TT_PAUSED) return 268;
    proof("theme-night-paused");
    dial_result_t gesture=dial_pointer(240,229,1000,false);
    if (gesture.stop!=1 || timer.state!=TT_IDLE || !dial_tap() || timer.state!=TT_RUNNING) return 269;
    test_tick+=(uint32_t)timer.remaining_ms+20; update(); lv_timer_handler(); proof("theme-night-alarm");
    if (timer.state!=TT_FINISHED || !reminder.active) return 270;
    flip(-1); if (reminder.active || timer.state!=TT_IDLE) return 271;
    flip(1); if (timer.state!=TT_RUNNING) return 272;
    if (!unchanged_view_stays_clean(TT_HOME,true) || !unchanged_view_stays_clean(TT_RIGHT,true)) return 273;
    puts("PASS: local night boundaries/invalid time, live dropdown/keyboard/brightness/timer drafts, page deletion, night dial gestures/alarm flips, unchanged snapshots stay clean");
    return 0;
}
int main(int argc, char **argv)
{
    if (argc==2 && !strcmp(argv[1],"--check-location-live")) return tt_pc_location_check();
    bool preview_location=argc==2 && !strcmp(argv[1],"--preview-location");
    bool preview_ready=preview_location || (argc==2 && strcmp(argv[1],"--preview-ready")==0);
    bool scene_test=argc==2 && strcmp(argv[1],"--smoke-scene")==0;
    bool setup_test=argc==2 && strcmp(argv[1],"--smoke-setup")==0;
    bool timer_test=argc==2 && strcmp(argv[1],"--smoke-timer")==0;
    bool weather_test=argc==2 && strcmp(argv[1],"--smoke-weather")==0;
    bool brightness_test=argc==2 && strcmp(argv[1],"--smoke-brightness")==0;
    bool theme_test=argc==2 && strcmp(argv[1],"--smoke-theme")==0;
    bool configured = theme_test || preview_ready || brightness_test || scene_test || timer_test || weather_test || (argc == 2 && strcmp(argv[1], "--smoke-configured") == 0);
    bool test = (configured && !preview_ready) || setup_test || (argc == 2 && strcmp(argv[1], "--smoke") == 0);
    test_mode=test;
    SDL_SetMainReady();
    lv_init();
    display = lv_sdl_window_create(480, 480);
    if (!display) return 1;
    lv_tick_set_cb(preview_tick);
    lv_display_add_event_cb(display, device_closed, LV_EVENT_DELETE, NULL);
    lv_sdl_window_set_title(display, "Time-Tick V1.0 | 电脑预览 | 模拟天气");
    lv_sdl_window_set_resizeable(display, false);
    lv_display_set_dpi(display, 60); /* Same LVGL DPI as the firmware. */
    lv_indev_set_display(lv_sdl_mouse_create(), display);
    tt_settings_default(&settings);
    if (configured) { settings.setup_complete=true; settings.language=preview_ready?TT_ZH:TT_EN; }
    settings.timer.calibrated = true; /* Explicit virtual board axes, not real calibration. */
    timezone_apply();
    tt_timer_init(&timer);
    pet_load();
    if (!test) atexit(preview_shutdown);
    tt_ui_init(submit, NULL, &settings);
    tt_ui_pet_snapshot(&pets);
    if (test) {
        int result=theme_test?theme_smoke():brightness_test?brightness_smoke():scene_test?scene_smoke():setup_test?setup_smoke():timer_test?timer_smoke():weather_test?weather_smoke():smoke();
        if (result) fprintf(stderr,"Preview regression failed at checkpoint %d\n",result);
        return result;
    }
    controls_create();
    if (preview_location) { update(); (void)press("设置"); (void)press("天气"); }
    puts("Time-Tick 电脑预览：关闭设备窗口即可退出。系统设置仅保留本次会话，天气和传感器均为模拟数据。");
    int64_t next = 0;
    while (display) {
        if (now_ms() >= next) { update(); next = now_ms() + 20; }
        lv_timer_handler();
        SDL_Delay(5);
    }
    return 0;
}
