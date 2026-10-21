// SPDX-License-Identifier: GPL-3.0-only
#include "settings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    tt_settings_t s,out; tt_settings_default(&s);
    assert(tt_settings_valid(&s) && !s.setup_complete && s.language==TT_ZH);
    s.setup_complete=true; s.language=TT_EN; s.pet=TT_DAIMAO; s.brightness=90; s.vibration=false;
    strcpy(s.ssid,"test-network"); strcpy(s.password,"password123");
    assert(tt_settings_decode(&s,sizeof(s),&out));
    assert(!memcmp(&s,&out,sizeof(s)));
    s.timer.seconds[TT_FRONT]=1800; /* Existing schema-4 flat preset is retired. */
    assert(!tt_settings_valid(&s));
    assert(tt_settings_decode(&s,sizeof(s),&out) && out.timer.seconds[TT_FRONT]==0);
    s.timer.seconds[TT_FRONT]=0;
    assert(!memcmp(&s,&out,sizeof(s))); /* Wi-Fi, other presets and preferences survive. */
    s.pet=2; assert(tt_settings_decode(&s,sizeof(s),&out) && out.pet==TT_DAIMAO); s.pet=TT_DAIMAO;
    s.brightness=0; assert(!tt_settings_valid(&s)); s.brightness=90;
    s.language=2; assert(!tt_settings_valid(&s)); s.language=TT_EN;
    s.pet=3; assert(!tt_settings_valid(&s)); s.pet=TT_DAIMAO;
    assert(!tt_settings_decode(&s,sizeof(s)-1,&out));
    struct { uint32_t schema; tt_config_t timer; char ssid[33],password[65],city[80],timezone[64]; } old={0};
    old.schema=1; old.timer=tt_config_default(); old.timer.seconds[TT_RIGHT]=123; old.timer.seconds[TT_FRONT]=1800;
    old.timer.calibrated=true; old.timer.home_axis=2; old.timer.right_axis=1;
    strcpy(old.ssid,"kept-network"); strcpy(old.password,"kept-password"); strcpy(old.city,"Shanghai"); strcpy(old.timezone,"CST-8");
    assert(tt_settings_decode(&old,sizeof(old),&out));
    assert(out.schema==6 && !out.setup_complete && out.timer.calibrated && out.timer.seconds[TT_RIGHT]==123 && out.timer.seconds[TT_FRONT]==0);
    assert(!strcmp(out.ssid,old.ssid) && !strcmp(out.password,old.password) && !strcmp(out.city,old.city));
    struct {
        uint32_t schema; tt_config_t timer; char ssid[33],password[65],city[80],timezone[64];
        uint8_t language,pet,brightness; bool setup_complete,vibration;
    } v2={.schema=2,.timer=old.timer,.language=TT_EN,.pet=1,.brightness=60,.setup_complete=true};
    strcpy(v2.timezone,"CST-8"); strcpy(v2.ssid,"legacy-v2");
    assert(tt_settings_decode(&v2,sizeof(v2),&out) && out.schema==6 && out.sound && out.sound_volume==25);
    assert(out.setup_complete && out.pet==TT_DAIMAO && out.brightness==60 && !strcmp(out.ssid,"legacy-v2"));
    s=out; s.sound_volume=81; assert(!tt_settings_valid(&s));
    struct {
        uint32_t schema; tt_config_t timer; char ssid[33],password[65],city[80],timezone[64];
        uint8_t language,pet,brightness; bool setup_complete,vibration; uint8_t sound_volume; bool sound;
    } v3={.schema=3,.timer=old.timer,.language=TT_ZH,.pet=TT_DAIMAO,.brightness=75,.setup_complete=true,.sound_volume=38,.sound=true};
    strcpy(v3.timezone,"CST-8"); strcpy(v3.ssid,"legacy-v3"); v3.timer.seconds[TT_BACK]=3600;
    assert(tt_settings_decode(&v3,sizeof(v3),&out) && out.schema==6 && out.sound_volume==38);
    assert(out.timer.seconds[TT_BACK]==0 && out.timer.seconds[TT_RIGHT]==123 && !out.precise_location);
    out.precise_location=true; out.latitude_e6=31230416; out.longitude_e6=121473701;
    assert(tt_settings_decode(&out,sizeof(out),&s) && !memcmp(&s,&out,sizeof(s)));
    for (unsigned tone=0;tone<TT_RING_COUNT;++tone) {
        out.alarm.ringtone=(uint8_t)tone;
        assert(tt_settings_decode(&out,sizeof(out),&s) && !memcmp(&s,&out,sizeof(s)));
    }
    out.latitude_e6=90000001; assert(!tt_settings_valid(&out));
    out.latitude_e6=0; out.longitude_e6=-180000001; assert(!tt_settings_valid(&out));
    old.schema=99; assert(!tt_settings_decode(&old,sizeof(old),&out));
    struct {
        uint32_t schema; tt_config_t timer; char ssid[33],password[65],city[80],timezone[64];
        uint8_t language,pet,brightness; bool setup_complete,vibration; uint8_t sound_volume; bool sound;
        bool precise_location; int32_t latitude_e6,longitude_e6;
    } v4={.schema=4,.timer=tt_config_default(),.language=TT_EN,.pet=TT_DAIMAO,.brightness=65,
        .setup_complete=true,.vibration=true,.sound_volume=25,.sound=true,
        .precise_location=true,.latitude_e6=31230416,.longitude_e6=121473701};
    strcpy(v4.ssid,"kept-v4-network"); strcpy(v4.password,"kept-v4-password"); strcpy(v4.city,"Saved city"); strcpy(v4.timezone,"CST-8");
    v4.timer.seconds[TT_LEFT]=4321; v4.timer.calibrated=true; v4.timer.home_axis=2; v4.timer.right_axis=1;
    assert(tt_settings_decode(&v4,sizeof(v4),&out) && out.schema==6);
    s=out; s.schema=4;
    assert(!memcmp(&s,&v4,sizeof(v4))); /* Complete old prefix, including location and axes. */
    assert(out.alarm.enabled && out.alarm.volume==65 && out.alarm.duration_s==30);
    v4.sound=false; assert(tt_settings_decode(&v4,sizeof(v4),&out) && !out.alarm.enabled);
    out.alarm=(tt_alarm_config_t){.enabled=true,.ringtone=TT_RING_DIGITAL,.volume=73,.duration_s=60};
    assert(tt_settings_decode(&out,sizeof(out),&s) && !memcmp(&s,&out,sizeof(s)));
    struct {
        uint32_t schema; tt_config_t timer; char ssid[33],password[65],city[80],timezone[64];
        uint8_t language,pet,brightness; bool setup_complete,vibration; uint8_t sound_volume; bool sound;
        bool precise_location; int32_t latitude_e6,longitude_e6; tt_alarm_config_t alarm;
    } v5;
    _Static_assert(sizeof(v5)==offsetof(tt_settings_t,theme),"old schema-5 prefix");
    memcpy(&v5,&s,sizeof(v5)); v5.schema=5;
    assert(tt_settings_decode(&v5,sizeof(v5),&out) && out.schema==6);
    out.schema=5; assert(!memcmp(&out,&v5,sizeof(v5))); out.schema=6;
    assert(out.theme.mode==TT_THEME_AUTO && out.theme.night_start==1200 && out.theme.night_end==300);
    tt_theme_config_t t=out.theme;
    assert(!tt_theme_night_at(&t,1199,false) && tt_theme_night_at(&t,1200,false));
    assert(tt_theme_night_at(&t,0,false) && tt_theme_night_at(&t,299,false) && !tt_theme_night_at(&t,300,true));
    assert(tt_theme_night_at(&t,1440,true) && !tt_theme_night_at(&t,1440,false));
    t.night_start=8*60+17; t.night_end=17*60+42;
    assert(!tt_theme_night_at(&t,496,true) && tt_theme_night_at(&t,497,false));
    assert(tt_theme_night_at(&t,1061,false) && !tt_theme_night_at(&t,1062,true));
    t.night_end=t.night_start; assert(!tt_theme_config_valid(&t));
    t.mode=TT_THEME_DAY; assert(tt_theme_config_valid(&t) && !tt_theme_night_at(&t,1440,true));
    t.mode=TT_THEME_NIGHT; assert(tt_theme_night_at(&t,1440,false));
    out.theme=t; assert(tt_settings_decode(&out,sizeof(out),&s) && !memcmp(&s,&out,sizeof(s)));
    t.mode=3; assert(!tt_theme_config_valid(&t)); t.mode=TT_THEME_AUTO;
    t.night_start=1440; t.night_end=300; assert(!tt_theme_config_valid(&t));
    t.night_start=1200; t.night_end=1440; assert(!tt_theme_config_valid(&t));
    puts("PASS: settings defaults, persistence roundtrip, bounds, legacy migration");
    return 0;
}
