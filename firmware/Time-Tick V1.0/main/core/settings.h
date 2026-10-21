// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "timetick_core.h"
#include "alarm_model.h"
#include <stddef.h>
typedef enum { TT_ZH, TT_EN } tt_language_t;
typedef enum { TT_DAIMAO, TT_PET_COUNT } tt_pet_t;
typedef enum { TT_THEME_AUTO, TT_THEME_DAY, TT_THEME_NIGHT } tt_theme_mode_t;
typedef struct {
    uint8_t mode;
    uint16_t night_start,night_end; /* Local minutes since midnight, [start,end). */
} tt_theme_config_t;
typedef struct {
    uint32_t schema;
    tt_config_t timer;
    char ssid[33], password[65], city[80], timezone[64];
    uint8_t language, pet, brightness;
    bool setup_complete, vibration;
    uint8_t sound_volume; bool sound;
    bool precise_location;
    int32_t latitude_e6, longitude_e6; /* WGS84, millionths of a degree. */
    tt_alarm_config_t alarm;
    tt_theme_config_t theme;
} tt_settings_t;
bool tt_theme_config_valid(const tt_theme_config_t *theme);
/* minute >=1440 means no valid local clock. Manual modes still work. */
bool tt_theme_night_at(const tt_theme_config_t *theme,unsigned minute,bool previous);
void tt_settings_default(tt_settings_t *settings);
bool tt_settings_valid(const tt_settings_t *settings);
bool tt_settings_decode(const void *data, size_t size, tt_settings_t *settings);
#ifdef ESP_PLATFORM
#include "esp_err.h"
esp_err_t tt_settings_load(tt_settings_t *settings);
esp_err_t tt_settings_save(const tt_settings_t *settings);
#endif
