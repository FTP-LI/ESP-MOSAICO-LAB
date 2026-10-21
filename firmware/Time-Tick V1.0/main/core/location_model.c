// SPDX-License-Identifier: GPL-3.0-only
#include "location_model.h"
#include "cJSON.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

bool tt_location_url(const tt_location_request_t *r,char *url,size_t capacity)
{
    int length;
    if (r->kind==TT_LOCATION_IP) {
        length=snprintf(url,capacity,
            "https://ipwho.is/?fields=success,city,region,country,latitude,longitude&lang=%s",
            r->language==TT_ZH?"zh-CN":"en");
    } else if (r->kind==TT_LOCATION_SEARCH) {
        size_t n=0; while (n<sizeof(r->query) && r->query[n]) ++n;
        if (n<2 || n>=sizeof(r->query)) return false;
        char encoded[sizeof(r->query)*3]; char *out=encoded;
        static const char hex[]="0123456789ABCDEF";
        for (size_t i=0;i<n;++i) {
            unsigned char c=(unsigned char)r->query[i];
            if (c<32 || c==127) return false;
            if ((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-' || c=='_')
                *out++=(char)c;
            else { *out++='%'; *out++=hex[c>>4]; *out++=hex[c&15]; }
        }
        *out=0;
        length=snprintf(url,capacity,"https://geocoding-api.open-meteo.com/v1/search?name=%s&count=5&language=%s&format=json",
                        encoded,r->language==TT_ZH?"zh":"en");
    } else return false;
    return length>0 && (size_t)length<capacity;
}
static const char *string(cJSON *item,const char *key)
{
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(item,key);
    return cJSON_IsString(v) && v->valuestring?v->valuestring:"";
}
static bool coordinate(cJSON *item,const char *key,double limit,int32_t *out)
{
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(item,key);
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || fabs(v->valuedouble)>limit) return false;
    *out=(int32_t)llround(v->valuedouble*1000000.0); return true;
}
/* A truncated UTF-8 city must not split a glyph. Ignore control characters
 * from provider text, including newlines that could disguise a result row. */
static void text_copy(char *dst,size_t capacity,const char *src)
{
    size_t used=0;
    while (*src) {
        unsigned char c=(unsigned char)*src;
        size_t width=c<128?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:(c&0xf8)==0xf0?4:0;
        if (!width) break;
        bool valid=true;
        for (size_t i=1;i<width;++i) if (!src[i] || ((unsigned char)src[i]&0xc0)!=0x80) { valid=false; break; }
        if (!valid || used+width>=capacity) break;
        if (c>=32 && c!=127) { memcpy(dst+used,src,width); used+=width; }
        src+=width;
    }
    dst[used]=0;
}
static bool place(cJSON *item,bool ip,tt_location_t *out)
{
    if (!cJSON_IsObject(item) ||
        !coordinate(item,"latitude",90,&out->latitude_e6) ||
        !coordinate(item,"longitude",180,&out->longitude_e6)) return false;
    text_copy(out->name,sizeof(out->name),string(item,ip?"city":"name"));
    if (!out->name[0]) return false;
    char full[512];
    const char *admin=string(item,ip?"region":"admin1"),*county=ip?"":string(item,"admin2"),*country=string(item,"country");
    snprintf(full,sizeof(full),"%s%s%s%s%s",country,*country && *admin?" / ":"",admin,
             *county && strcmp(county,admin)?" / ":"",strcmp(county,admin)?county:"");
    text_copy(out->region,sizeof(out->region),full); return true;
}
void tt_location_parse(const char *body,tt_location_view_t *view)
{
    view->state=TT_LOCATION_FAILED; view->count=0;
    if (!body || strlen(body)>TT_LOCATION_BODY_MAX) return;
    cJSON *json=cJSON_ParseWithOpts(body,NULL,true);
    if (!cJSON_IsObject(json)) { cJSON_Delete(json); return; }
    if (view->kind==TT_LOCATION_IP) {
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json,"success")) && place(json,true,&view->places[0]))
            view->count=1;
        view->state=view->count?TT_LOCATION_READY:TT_LOCATION_FAILED;
    } else {
        cJSON *items=cJSON_GetObjectItemCaseSensitive(json,"results");
        if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json,"error")) && (!items || cJSON_IsArray(items))) {
            cJSON *item;
            cJSON_ArrayForEach(item,items) {
                tt_location_t candidate={0};
                if (!place(item,false,&candidate)) continue;
                bool duplicate=false;
                for (unsigned i=0;i<view->count;++i)
                    if (candidate.latitude_e6==view->places[i].latitude_e6 &&
                        candidate.longitude_e6==view->places[i].longitude_e6) duplicate=true;
                if (!duplicate) view->places[view->count++]=candidate;
                if (view->count==TT_LOCATION_MAX) break;
            }
            view->state=view->count?TT_LOCATION_READY:TT_LOCATION_EMPTY;
        }
    }
    cJSON_Delete(json);
}
