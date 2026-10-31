// SPDX-License-Identifier: GPL-3.0-only
#include "clip_file.h"
#include "mic_core.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
static uint32_t crc(const void *data,size_t n,uint32_t state){
    const uint8_t *p=data;
    for(size_t i=0;i<n;++i){state^=p[i];for(int b=0;b<8;++b)state=(state>>1)^(0xedb88320u & (0u-(state&1u)));}
    return state;
}
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(i*8));}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static bool path_for(char p[256],const char *dir,const char *name){
    int n=snprintf(p,256,"%s/%s",dir,name);return n>0 && n<256;
}
static clip_result_t read_file(const char *path,int16_t *out,size_t cap,size_t *count){
    *count=0;FILE *f=fopen(path,"rb");if(!f)return errno==ENOENT?CLIP_MISSING:CLIP_IO_ERROR;
    uint8_t h[32];clip_result_t result=CLIP_INVALID;
    if(fread(h,1,sizeof(h),f)!=sizeof(h))goto done;
    size_t n=get32(h+12);
    if(memcmp(h,"MICLAB01",8)||get32(h+8)!=MIC_RATE||get32(h+20)!=2||get32(h+24)!=1||
       get32(h+28)!=~crc(h,28,~0u)||n==0||n>MIC_MAX_SAMPLES||n>cap)goto done;
    uint8_t buf[1024];size_t offset=0,total=n*2;uint32_t sum=~0u;
    while(offset<total){
        size_t take=total-offset;if(take>sizeof(buf))take=sizeof(buf);
        if(fread(buf,1,take,f)!=take)goto done;
        sum=crc(buf,take,sum);if(out)memcpy((uint8_t *)out+offset,buf,take);offset+=take;
    }
    if(~sum!=get32(h+16)||fgetc(f)!=EOF||ferror(f))goto done;
    *count=n;result=CLIP_OK;
done:
    if(ferror(f))result=CLIP_IO_ERROR;
    if(fclose(f)!=0)result=CLIP_IO_ERROR;
    if(result!=CLIP_OK)*count=0;
    return result;
}
clip_result_t clip_file_load(const char *dir,int16_t *samples,size_t cap,size_t *n){
    char path[256];*n=0;
    if(!path_for(path,dir,"latest.mic"))return CLIP_INVALID;
    return read_file(path,samples,cap,n);
}
clip_result_t clip_file_save(const char *dir,const int16_t *samples,size_t n){
    if(!samples||!n||n>MIC_MAX_SAMPLES)return CLIP_INVALID;
    char pending[256],latest[256];
    if(!path_for(pending,dir,"pending.mic")||!path_for(latest,dir,"latest.mic"))return CLIP_INVALID;
    uint8_t h[32]={0};memcpy(h,"MICLAB01",8);put32(h+8,MIC_RATE);put32(h+12,(uint32_t)n);
    put32(h+16,~crc(samples,n*2,~0u));put32(h+20,2);put32(h+24,1);put32(h+28,~crc(h,28,~0u));
    FILE *f=fopen(pending,"wb");if(!f)return CLIP_IO_ERROR;
    bool ok=fwrite(h,1,sizeof(h),f)==sizeof(h);
    for(size_t offset=0;ok && offset<n;){size_t take=n-offset;if(take>2048)take=2048;
        ok=fwrite(samples+offset,2,take,f)==take;offset+=take;}
    if(ok)ok=fflush(f)==0;
#ifdef _WIN32
    if(ok)ok=_commit(_fileno(f))==0;
#else
    if(ok)ok=fsync(fileno(f))==0;
#endif
    if(fclose(f)!=0)ok=false;
    size_t verified=0;
    if(ok)ok=read_file(pending,NULL,MIC_MAX_SAMPLES,&verified)==CLIP_OK && verified==n;
    if(!ok){remove(pending);return CLIP_IO_ERROR;}
#ifdef _WIN32
    ok=MoveFileExA(pending,latest,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    ok=rename(pending,latest)==0;
#endif
    if(!ok){remove(pending);return CLIP_IO_ERROR;}
    return CLIP_OK;
}
