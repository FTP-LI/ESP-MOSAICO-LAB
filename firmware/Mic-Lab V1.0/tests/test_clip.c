#include "clip_file.h"
#include "mic_core.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <direct.h>
int main(void){
    const char *dir="build-tests/clip";
    mkdir(dir);remove("build-tests/clip/latest.mic");remove("build-tests/clip/pending.mic");
    int16_t in[5]={-32768,32767,1,0,-22},out[5]={0};size_t n=99;
    assert(clip_file_load(dir,out,5,&n)==CLIP_MISSING && n==0);
    assert(clip_file_save(dir,in,5)==CLIP_OK);
    assert(clip_file_load(dir,out,5,&n)==CLIP_OK && n==5 && !memcmp(in,out,sizeof(in)));
    assert(clip_file_save(dir,in,0)==CLIP_INVALID);
    assert(clip_file_save(dir,in,MIC_MAX_SAMPLES+1)==CLIP_INVALID);
    assert(clip_file_load(dir,out,5,&n)==CLIP_OK && n==5);
    assert(clip_file_load(dir,out,4,&n)==CLIP_INVALID && n==0);
    FILE *f=fopen("build-tests/clip/pending.mic","wb");assert(f);fputs("incomplete save",f);fclose(f);
    assert(clip_file_load(dir,out,5,&n)==CLIP_OK);
    in[0]=123;assert(clip_file_save(dir,in,5)==CLIP_OK);
    assert(clip_file_load(dir,out,5,&n)==CLIP_OK && out[0]==123);
    // A real open failure must not replace the previous committed clip.
    assert(mkdir("build-tests/clip/pending.mic")==0);
    assert(clip_file_save(dir,in,5)==CLIP_IO_ERROR);
    assert(clip_file_load(dir,out,5,&n)==CLIP_OK && out[0]==123);
    assert(_rmdir("build-tests/clip/pending.mic")==0);
    f=fopen("build-tests/clip/latest.mic","r+b");assert(f);fseek(f,8,SEEK_SET);fputc(0,f);fclose(f);
    assert(clip_file_load(dir,out,5,&n)==CLIP_INVALID && n==0);
    assert(clip_file_save(dir,in,5)==CLIP_OK);
    f=fopen("build-tests/clip/latest.mic","r+b");assert(f);fseek(f,34,SEEK_SET);fputc(17,f);fclose(f);
    assert(clip_file_load(dir,out,5,&n)==CLIP_INVALID && n==0);
    assert(clip_file_save(dir,in,5)==CLIP_OK);
    f=fopen("build-tests/clip/latest.mic","ab");assert(f);fputc(1,f);fclose(f);
    assert(clip_file_load(dir,out,5,&n)==CLIP_INVALID);
    f=fopen("build-tests/clip/latest.mic","wb");assert(f);fputs("MICLAB01",f);fclose(f);
    assert(clip_file_load(dir,out,5,&n)==CLIP_INVALID);
    int16_t *full=calloc(MIC_MAX_SAMPLES,sizeof(int16_t));assert(full);
    assert(clip_file_save(dir,full,MIC_MAX_SAMPLES)==CLIP_OK);
    assert(clip_file_load(dir,full,MIC_MAX_SAMPLES,&n)==CLIP_OK && n==MIC_MAX_SAMPLES);free(full);
    puts("clip: persistence, replacement, incomplete temp, capacity, corruption and full 30 seconds PASS");
}
