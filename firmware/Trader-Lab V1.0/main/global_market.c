// SPDX-License-Identifier: GPL-3.0-only
#include "global_market.h"
#include "model.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
const tl_index_spec_t tl_indices[TL_GLOBAL_COUNT]={
    {"上证指数","中国","sh000001",2},{"深证成指","中国","sz399001",2},
    {"创业板指","中国","sz399006",2},{"恒生指数","香港","hkHSI",2},
    {"日经225","日本","gzN225",2},{"韩国综合","韩国","gzKS11",2},
    {"标普500","美国","usINX",2},{"纳斯达克","美国","usIXIC",2},
    {"道琼斯","美国","usDJI",2},{"德国DAX","德国","gzGDAXI",2},
    {"富时100","英国","gzFTSE",2},{"法国CAC40","法国","gzFCHI",2}
};
static bool number(const char *s,double *v)
{char *end;*v=strtod(s,&end);return *s&&! *end&&isfinite(*v);}
static bool make_quote(double p,double prev,int64_t time,unsigned source,tl_index_quote_t *out)
{
    if(!isfinite(p)||!isfinite(prev)||p<=0||prev<=0||p>1000000||prev>1000000||time<1735689600||time>4102444800LL)return false;
    *out=(tl_index_quote_t){.price=llround(p*100),.previous=llround(prev*100),.time=time,.source=source};return true;
}
/* US provider timestamps are New York local time, including DST. */
static int64_t provider_time(const char *s,bool us)
{
    char digits[15]={0};unsigned n=0;
    for(;*s;s++)if(*s>='0'&&*s<='9'){if(n==14)return 0;digits[n++]=*s;}
    int64_t china=tl_timestamp(digits);if(n!=14||!china)return 0;
    if(!us)return china;
    int year,month,day,hour;
    if(sscanf(digits,"%4d%2d%2d%2d",&year,&month,&day,&hour)!=4)return 0;
    char first[15];snprintf(first,sizeof(first),"%04d0301000000",year);
    int march_weekday=(int)((tl_timestamp(first)+28800)/86400+4)%7;
    snprintf(first,sizeof(first),"%04d1101000000",year);
    int november_weekday=(int)((tl_timestamp(first)+28800)/86400+4)%7;
    int start=8+(7-march_weekday)%7,end=1+(7-november_weekday)%7;
    bool dst=(month>3&&month<11)||(month==3&&(day>start||(day==start&&hour>=2)))||(month==11&&(day<end||(day==end&&hour<2)));
    return china+28800+(dst?4:5)*3600;
}
int tl_global_tencent(const char *body,tl_index_quote_t out[TL_GLOBAL_COUNT])
{
    int count=0;
    for(unsigned i=0;i<TL_GLOBAL_COUNT;i++) {
        if(tl_indices[i].provider!=2)continue;
        char key[32];snprintf(key,sizeof(key),"v_%s=\"",tl_indices[i].symbol);
        const char *line=strstr(body,key);if(!line)continue;line+=strlen(key);
        const char *end=strchr(line,'"');if(!end||end-line>4096)continue;
        char fields[33][80]={{0}};unsigned n=0;bool ok=true;
        bool compact=!strncmp(tl_indices[i].symbol,"gz",2);unsigned required=compact?7:33;
        while(line<end&&n<required){const char *next=memchr(line,'~',end-line);if(!next)break;size_t len=next-line;if(len>=80){ok=false;break;}memcpy(fields[n++],line,len);line=next+1;}
        double p,prev,pct;tl_index_quote_t q;
        if(!ok||n!=required||!number(fields[3],&p)||!number(fields[4],&prev)||!number(fields[compact?5:32],&pct))continue;
        if(compact){if(strcmp(fields[0],tl_indices[i].symbol+2))continue;prev=p-prev;}
        if(prev>0&&fabs((p/prev-1)*100-pct)<.05&&make_quote(p,prev,provider_time(fields[compact?2:30],!compact&&i>=6&&i<=8),2,&q)){out[i]=q;count++;}
    }
    return count;
}
bool tl_global_valid(const tl_index_quote_t *q,int64_t now)
{return q->source&&q->price>0&&q->previous>0&&q->time<=now+300&&now-q->time<=16*86400;}
