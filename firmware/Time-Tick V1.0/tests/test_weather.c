// SPDX-License-Identifier: GPL-3.0-only
#include "weather_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    /* Official supported codes, including freezing precipitation and 97. */
    const struct { int code; tt_weather_kind_t kind; } cases[]={
        {0,TT_WEATHER_CLEAR},{1,TT_WEATHER_CLEAR},{2,TT_WEATHER_CLOUDY},{3,TT_WEATHER_CLOUDY},
        {45,TT_WEATHER_FOG},{48,TT_WEATHER_FOG},
        {51,TT_WEATHER_RAIN},{53,TT_WEATHER_RAIN},{55,TT_WEATHER_RAIN},{56,TT_WEATHER_RAIN},{57,TT_WEATHER_RAIN},
        {61,TT_WEATHER_RAIN},{63,TT_WEATHER_RAIN},{65,TT_WEATHER_RAIN},{66,TT_WEATHER_RAIN},{67,TT_WEATHER_RAIN},
        {71,TT_WEATHER_SNOW},{73,TT_WEATHER_SNOW},{75,TT_WEATHER_SNOW},{77,TT_WEATHER_SNOW},
        {80,TT_WEATHER_RAIN},{81,TT_WEATHER_RAIN},{82,TT_WEATHER_RAIN},
        {85,TT_WEATHER_SNOW},{86,TT_WEATHER_SNOW},
        {95,TT_WEATHER_STORM},{96,TT_WEATHER_STORM},{97,TT_WEATHER_STORM},{99,TT_WEATHER_STORM}
    };
    for (unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i) assert(tt_weather_kind(cases[i].code)==cases[i].kind);
    const int invalid[]={-999,-1,4,44,46,50,52,58,60,68,70,78,83,87,94,98,100,999};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        assert(tt_weather_kind(invalid[i])==TT_WEATHER_UNKNOWN);
        assert(!strcmp(tt_weather_text(invalid[i]),"Weather"));
    }
    assert(!strcmp(tt_weather_text(1),"Clear"));
    assert(!strcmp(tt_weather_text(82),"Showers"));
    assert(!strcmp(tt_weather_text(86),"Snow showers"));
    int32_t lat=7,lon=8;
    assert(tt_weather_coordinates("31.230416","121.473701",&lat,&lon) && lat==31230416 && lon==121473701);
    assert(tt_weather_coordinates("-90","180",&lat,&lon) && lat==-90000000 && lon==180000000);
    assert(tt_weather_coordinates("0","0",&lat,&lon) && lat==0 && lon==0);
    assert(!tt_weather_coordinates("91","0",&lat,&lon));
    assert(!tt_weather_coordinates("0","-181",&lat,&lon));
    assert(!tt_weather_coordinates("nan","0",&lat,&lon));
    assert(!tt_weather_coordinates("0","inf",&lat,&lon));
    assert(!tt_weather_coordinates("","0",&lat,&lon));
    assert(!tt_weather_coordinates("31x","121",&lat,&lon));
    assert(lat==0 && lon==0); /* Invalid input never partially commits. */
    puts("PASS: WMO weather mapping and WGS84 coordinate precision / bounds / invalid values");
}
