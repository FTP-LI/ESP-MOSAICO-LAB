// SPDX-License-Identifier: GPL-3.0-only
#include "weather_model.h"
#include <math.h>
#include <stdlib.h>
#include <ctype.h>

bool tt_weather_coordinates(const char *latitude,const char *longitude,int32_t *lat_e6,int32_t *lon_e6)
{
    const char *values[]={latitude,longitude}; double parsed[2];
    for (int i=0;i<2;++i) {
        if (!values[i] || !*values[i]) return false;
        char *end; parsed[i]=strtod(values[i],&end);
        if (end==values[i]) return false;
        while (isspace((unsigned char)*end)) ++end;
        double limit=i?180:90;
        if (*end || !isfinite(parsed[i]) || parsed[i]<-limit || parsed[i]>limit) return false;
    }
    *lat_e6=(int32_t)lround(parsed[0]*1000000.0);
    *lon_e6=(int32_t)lround(parsed[1]*1000000.0);
    return true;
}
/* Open-Meteo WMO interpretation codes, checked 2026-10-20.
 * https://open-meteo.com/en/docs#weathervariables */
tt_weather_kind_t tt_weather_kind(int code)
{
    switch (code) {
    case 0: case 1: return TT_WEATHER_CLEAR;
    case 2: case 3: return TT_WEATHER_CLOUDY;
    case 45: case 48: return TT_WEATHER_FOG;
    case 51: case 53: case 55: case 56: case 57:
    case 61: case 63: case 65: case 66: case 67:
    case 80: case 81: case 82: return TT_WEATHER_RAIN;
    case 71: case 73: case 75: case 77: case 85: case 86: return TT_WEATHER_SNOW;
    case 95: case 96: case 97: case 99: return TT_WEATHER_STORM;
    default: return TT_WEATHER_UNKNOWN;
    }
}
const char *tt_weather_text(int code)
{
    if (code==80 || code==81 || code==82) return "Showers";
    if (code==85 || code==86) return "Snow showers";
    static const char *names[]={"Weather","Clear","Cloudy","Rain","Snow","Fog","Thunderstorm"};
    return names[tt_weather_kind(code)];
}
