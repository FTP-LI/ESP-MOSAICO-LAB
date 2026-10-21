// SPDX-License-Identifier: GPL-3.0-only
#include "ui_scene_background.h"
#include "src/misc/cache/instance/lv_image_cache.h"

typedef struct { uint32_t color; int top,bottom; } tint_t;
static uint16_t blend(uint16_t pixel,uint16_t color,unsigned alpha)
{
    unsigned inverse=255-alpha;
    unsigned r=(((pixel>>11)*inverse+(color>>11)*alpha)+127)/255;
    unsigned g=((((pixel>>5)&63)*inverse+((color>>5)&63)*alpha)+127)/255;
    unsigned b=(((pixel&31)*inverse+(color&31)*alpha)+127)/255;
    return (uint16_t)((r<<11)|(g<<5)|b);
}
bool tt_scene_background(lv_image_dsc_t *cache,const lv_image_dsc_t *source,
                         int visible_height,tt_weather_kind_t weather,tt_scene_t scene)
{
    if (!source || source->header.cf!=LV_COLOR_FORMAT_RGB565 ||
        source->header.stride!=source->header.w*2 || visible_height<2 ||
        visible_height>(int)source->header.h) return false;
    size_t bytes=source->header.stride*source->header.h;
    if (!cache->data) {
        cache->data=lv_malloc(bytes);
        if (!cache->data) return false; /* Keep the original live renderer on OOM. */
        cache->header=source->header; cache->data_size=bytes;
    }
    if (cache->data_size!=bytes) return false;
    lv_image_cache_drop(cache);
    tint_t tints[2]={{0}};
    if (scene.season==TT_AUTUMN) tints[0]=(tint_t){0xbd733c,12,40};
    else if (scene.season==TT_WINTER && weather!=TT_WEATHER_SNOW) tints[0]=(tint_t){0xdce9f4,18,35};
    if (scene.daylight==TT_NIGHT) tints[1]=(tint_t){0x091630,205,158};
    else if (scene.daylight==TT_DUSK) tints[1]=(tint_t){0xd57863,98,22};
    else if (scene.daylight==TT_DAWN) tints[1]=(tint_t){0xffd6ad,75,12};
    uint16_t colors[2];
    for (int i=0;i<2;++i) {
        uint32_t c=tints[i].color;
        colors[i]=(uint16_t)(((c>>8)&0xf800)|((c>>5)&0x7e0)|((c>>3)&31));
    }
    uint8_t *dest=(uint8_t *)cache->data;
    for (unsigned y=0;y<source->header.h;++y) {
        int row=(int)y-((int)source->header.h-visible_height); if (row<0) row=0;
        unsigned alpha[2];
        for (int i=0;i<2;++i) alpha[i]=(unsigned)(tints[i].top+(tints[i].bottom-tints[i].top)*row/(visible_height-1));
        for (unsigned x=0;x<source->header.w;++x) {
            size_t at=(size_t)y*source->header.stride+x*2;
            uint16_t pixel=(uint16_t)(source->data[at]|source->data[at+1]<<8);
            for (int i=0;i<2;++i) if (alpha[i]) pixel=blend(pixel,colors[i],alpha[i]);
            dest[at]=(uint8_t)pixel; dest[at+1]=(uint8_t)(pixel>>8);
        }
    }
    return true;
}
void tt_scene_background_free(lv_image_dsc_t *cache)
{
    lv_image_cache_drop(cache); lv_free((void *)cache->data); *cache=(lv_image_dsc_t){0};
}
