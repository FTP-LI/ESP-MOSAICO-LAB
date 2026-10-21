// SPDX-License-Identifier: GPL-3.0-only
#include "settings.h"
#include <string.h>
bool tt_theme_config_valid(const tt_theme_config_t *t)
{
    return t->mode<=TT_THEME_NIGHT && t->night_start<1440 && t->night_end<1440 &&
        (t->mode!=TT_THEME_AUTO || t->night_start!=t->night_end);
}
bool tt_theme_night_at(const tt_theme_config_t *t,unsigned minute,bool previous)
{
    if (t->mode==TT_THEME_DAY) return false;
    if (t->mode==TT_THEME_NIGHT) return true;
    if (minute>=1440 || !tt_theme_config_valid(t)) return previous;
    return t->night_start<t->night_end ? minute>=t->night_start && minute<t->night_end :
        minute>=t->night_start || minute<t->night_end;
}
void tt_settings_default(tt_settings_t *s)
{
    memset(s, 0, sizeof(*s));
    s->schema = 6;
    s->theme = (tt_theme_config_t){.mode=TT_THEME_AUTO,.night_start=20*60,.night_end=5*60};
    s->alarm = tt_alarm_default();
    s->timer = tt_config_default();
    s->language = TT_ZH;
    s->pet = TT_DAIMAO;
    s->brightness = 75;
    s->vibration = true; s->sound=true; s->sound_volume=25;
    strcpy(s->timezone, "CST-8");
}
bool tt_settings_valid(const tt_settings_t *s)
{
    return s->schema == 6 && tt_theme_config_valid(&s->theme) && tt_alarm_config_valid(&s->alarm) && s->latitude_e6>=-90000000 && s->latitude_e6<=90000000 &&
        s->longitude_e6>=-180000000 && s->longitude_e6<=180000000 &&
        s->sound_volume<=80 && s->language <= TT_EN && s->pet == TT_DAIMAO &&
        s->brightness >= 10 && s->brightness <= 100 && tt_config_valid(&s->timer) &&
        memchr(s->ssid, 0, sizeof(s->ssid)) && memchr(s->password, 0, sizeof(s->password)) &&
        memchr(s->city, 0, sizeof(s->city)) && memchr(s->timezone, 0, sizeof(s->timezone)) &&
        s->timezone[0];
}

bool tt_settings_decode(const void *data, size_t size, tt_settings_t *s)
{
    /* Preserve v1 network/timer/calibration data; show the new setup once. */
    typedef struct {
        uint32_t schema;
        tt_config_t timer;
        char ssid[33], password[65], city[80], timezone[64];
    } legacy_t;
    typedef struct {
        uint32_t schema;
        tt_config_t timer;
        char ssid[33], password[65], city[80], timezone[64];
        uint8_t language,pet,brightness;
        bool setup_complete,vibration;
    } legacy_v2_t;
    typedef struct {
        uint32_t schema;
        tt_config_t timer;
        char ssid[33], password[65], city[80], timezone[64];
        uint8_t language,pet,brightness;
        bool setup_complete,vibration;
        uint8_t sound_volume; bool sound;
    } legacy_v3_t;
    typedef struct {
        uint32_t schema;
        tt_config_t timer;
        char ssid[33], password[65], city[80], timezone[64];
        uint8_t language,pet,brightness;
        bool setup_complete,vibration;
        uint8_t sound_volume; bool sound;
        bool precise_location;
        int32_t latitude_e6,longitude_e6;
    } legacy_v4_t;
    typedef struct {
        uint32_t schema;
        tt_config_t timer;
        char ssid[33],password[65],city[80],timezone[64];
        uint8_t language,pet,brightness;
        bool setup_complete,vibration;
        uint8_t sound_volume; bool sound;
        bool precise_location;
        int32_t latitude_e6,longitude_e6;
        tt_alarm_config_t alarm;
    } legacy_v5_t;
    tt_settings_t candidate;
    tt_settings_default(&candidate);
    if (size == sizeof(candidate)) {
        memcpy(&candidate,data,size);
    }
    else if (size==sizeof(legacy_v5_t)) {
        legacy_v5_t old; memcpy(&old,data,size);
        if (old.schema!=5) return false;
        _Static_assert(sizeof(legacy_v5_t)==offsetof(tt_settings_t,theme),"settings v5 layout");
        memcpy(&candidate,&old,sizeof(old)); candidate.schema=6;
    }
    else if (size==sizeof(legacy_v4_t)) {
        legacy_v4_t old; memcpy(&old,data,size);
        if (old.schema!=4) return false;
        /* v5 only appends alarm preferences; the complete v4 prefix survives. */
        _Static_assert(sizeof(legacy_v4_t)==offsetof(tt_settings_t,alarm),"settings v4 layout");
        memcpy(&candidate,&old,sizeof(old)); candidate.schema=6;
        candidate.alarm.enabled=old.sound; /* Retain an explicitly muted device. */
    }
    else if (size==sizeof(legacy_v3_t)) {
        legacy_v3_t old; memcpy(&old,data,size);
        if (old.schema!=3) return false;
        candidate.timer=old.timer;
        memcpy(candidate.ssid,old.ssid,sizeof(old.ssid)); memcpy(candidate.password,old.password,sizeof(old.password));
        memcpy(candidate.city,old.city,sizeof(old.city)); memcpy(candidate.timezone,old.timezone,sizeof(old.timezone));
        candidate.language=old.language; candidate.pet=old.pet; candidate.brightness=old.brightness;
        candidate.setup_complete=old.setup_complete; candidate.vibration=old.vibration;
        candidate.sound=old.sound; candidate.sound_volume=old.sound_volume;
        candidate.alarm.enabled=old.sound;
    }
    else if (size==sizeof(legacy_v2_t)) {
        legacy_v2_t old; memcpy(&old,data,size);
        if (old.schema!=2) return false;
        candidate.timer=old.timer;
        memcpy(candidate.ssid,old.ssid,sizeof(old.ssid)); memcpy(candidate.password,old.password,sizeof(old.password));
        memcpy(candidate.city,old.city,sizeof(old.city)); memcpy(candidate.timezone,old.timezone,sizeof(old.timezone));
        candidate.language=old.language; candidate.pet=old.pet; candidate.brightness=old.brightness;
        candidate.setup_complete=old.setup_complete; candidate.vibration=old.vibration;
    }
    else if (size == sizeof(legacy_t)) {
        legacy_t old;
        memcpy(&old, data, size);
        if (old.schema != 1) return false;
        candidate.timer = old.timer;
        memcpy(candidate.ssid, old.ssid, sizeof(old.ssid));
        memcpy(candidate.password, old.password, sizeof(old.password));
        memcpy(candidate.city, old.city, sizeof(old.city));
        memcpy(candidate.timezone, old.timezone, sizeof(old.timezone));
    } else return false;
    if (size!=sizeof(candidate)) candidate.timer.seconds[TT_BACK]=0;
    /* Flat presets were removed; retain all other saved settings and axes. */
    candidate.timer.seconds[TT_FRONT]=0;
    if (candidate.pet>2) return false; /* Old cat/dog/chick selection. */
    candidate.pet=TT_DAIMAO;
    if (!tt_settings_valid(&candidate)) return false;
    *s = candidate;
    return true;
}
