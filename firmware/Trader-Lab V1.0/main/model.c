// SPDX-License-Identifier: GPL-3.0-only
#include "model.h"
#include "cJSON.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void tl_defaults(tl_config_t *c)
{
    memset(c,0,sizeof(*c)); c->magic=TL_CONFIG_MAGIC; c->brightness=40; c->volume=65; c->vibration=1;
    c->count=tl_seed_count;
    for(unsigned i=0;i<c->count;i++) snprintf(c->watch[i].code,sizeof(c->watch[i].code),"%s",tl_seed[i].code);
}
bool tl_migrate_v1(const tl_config_v1_t *old,tl_config_t *c)
{
    if(old->magic!=0x544c0101u || old->count>12) return false;
    tl_config_t valid={0};
    memcpy(&valid,old,offsetof(tl_config_t,watch)); valid.magic=TL_CONFIG_MAGIC;
    memcpy(valid.watch,old->watch,sizeof(old->watch));
    if(!tl_config_valid(&valid)) return false;
    tl_defaults(c);
    memcpy(c->ssid,valid.ssid,sizeof(c->ssid)); memcpy(c->password,valid.password,sizeof(c->password));
    c->brightness=valid.brightness;c->light=valid.light;c->volume=valid.volume;c->vibration=valid.vibration;
    for(unsigned j=0;j<valid.count;j++) {
        unsigned i;for(i=0;i<c->count;i++)if(!strcmp(c->watch[i].code,valid.watch[j].code))break;
        if(i==c->count && c->count<TL_MAX_WATCH)c->count++;
        if(i<TL_MAX_WATCH)c->watch[i]=valid.watch[j];
    }
    return tl_config_valid(c);
}
bool tl_code_valid(const char *code)
{
    if (!code || strlen(code)!=6) return false;
    for (int i=0;i<6;i++) if(code[i]<'0'||code[i]>'9') return false;
    return code[0]=='5' || !strncmp(code,"159",3);
}
bool tl_config_valid(const tl_config_t *c)
{
    if(c->magic!=TL_CONFIG_MAGIC || c->count>TL_MAX_WATCH || c->brightness<10 || c->brightness>100 ||
       c->volume>80 || c->light>1 || c->vibration>1 || !memchr(c->ssid,0,sizeof(c->ssid)) ||
       !memchr(c->password,0,sizeof(c->password))) return false;
    for(unsigned i=0;i<c->count;i++) {
        const tl_watch_t *w=&c->watch[i];
        if(w->code[6] || !tl_code_valid(w->code) || w->below>1 || w->enabled>1 || w->fired>1 ||
           w->threshold<0 || w->threshold>100000000 || (w->enabled && !w->threshold)) return false;
        for(unsigned j=0;j<i;j++) if(!strcmp(w->code,c->watch[j].code)) return false;
    }
    return true;
}
bool tl_price_parse(const char *s,int32_t *out)
{
    if(!s || !*s) return false;
    int64_t whole=0; int frac=0,digits=0;
    while(*s>='0'&&*s<='9') { whole=whole*10+(*s++-'0'); if(whole>100000) return false; digits++; }
    if(!digits) return false;
    if(*s=='.') {
        s++; digits=0;
        while(*s>='0'&&*s<='9') { if(++digits>3) return false; frac=frac*10+(*s++-'0'); }
        if(!digits) return false;
        while(digits++<3) frac*=10;
    }
    if(*s || whole*1000+frac<=0 || whole*1000+frac>100000000) return false;
    *out=(int32_t)(whole*1000+frac); return true;
}
void tl_price_text(int32_t p,char *out,size_t size) { snprintf(out,size,"%ld.%03ld",(long)(p/1000),(long)(p%1000)); }
bool tl_quote_valid(const tl_quote_t *q)
{
    return tl_code_valid(q->code) && q->price>0 && q->price<=100000000 && q->previous>0 &&
        q->previous<=100000000 && q->time>=1735689600 && q->time<=4102444800LL &&
        q->source>0 && q->source<=2 && strstr(q->name,"ETF")!=NULL;
}
static bool json_number(const cJSON *row,const char *key,double *number)
{
    const cJSON *value=cJSON_GetObjectItemCaseSensitive(row,key);
    if(!cJSON_IsNumber(value) || !isfinite(value->valuedouble)) return false;
    *number=value->valuedouble; return true;
}
int tl_parse_eastmoney(const char *body,tl_quote_t *out,int capacity)
{
    cJSON *json=cJSON_ParseWithOpts(body,NULL,true); if(!json) return 0;
    const cJSON *rows=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json,"data"),"diff");
    int used=0; const cJSON *row;
    cJSON_ArrayForEach(row,rows) {
        if(used>=capacity) break;
        const cJSON *code=cJSON_GetObjectItemCaseSensitive(row,"f12"),*name=cJSON_GetObjectItemCaseSensitive(row,"f14");
        double price,previous,percent,ts;
        if(!cJSON_IsString(code)||!tl_code_valid(code->valuestring)||!cJSON_IsString(name)||strlen(name->valuestring)>=64 ||
           !json_number(row,"f2",&price)||!json_number(row,"f18",&previous)||!json_number(row,"f3",&percent)||
           !json_number(row,"f124",&ts)||price<=0||previous<=0||price>100000||previous>100000||
           ts<1735689600||ts>4102444800.0||floor(ts)!=ts||fabs((price/previous-1)*100-percent)>.035) continue;
        tl_quote_t q={.price=(int32_t)llround(price*1000),.previous=(int32_t)llround(previous*1000),.time=(int64_t)ts,.source=1};
        snprintf(q.code,sizeof(q.code),"%s",code->valuestring); snprintf(q.name,sizeof(q.name),"%s",name->valuestring);
        if(tl_quote_valid(&q)) out[used++]=q;
    }
    cJSON_Delete(json); return used;
}
static bool leap(int year) { return year%4==0 && (year%100!=0 || year%400==0); }
int64_t tl_timestamp(const char *s)
{
    if(!s || strlen(s)!=14) return 0;
    int p[6]={0}; const int widths[]={4,2,2,2,2,2};
    for(int i=0;i<6;i++) for(int j=0;j<widths[i];j++) { if(*s<'0'||*s>'9') return 0; p[i]=p[i]*10+(*s++-'0'); }
    const int months[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(p[0]<2025||p[0]>2099||p[1]<1||p[1]>12||p[2]<1||p[2]>months[p[1]-1]+(p[1]==2&&leap(p[0]))||p[3]>23||p[4]>59||p[5]>59) return 0;
    int days=0; for(int y=1970;y<p[0];y++) days+=leap(y)?366:365;
    for(int m=1;m<p[1];m++) days+=months[m-1]+(m==2&&leap(p[0]));
    return (int64_t)(days+p[2]-1)*86400+p[3]*3600+p[4]*60+p[5]-8*3600;
}
int tl_parse_tencent(const char *body,tl_quote_t *out,int capacity)
{
    int count=0;
    for(const char *line=body; *line && count<capacity;) {
        const char *end=strchr(line,'\n'); if(!end) end=line+strlen(line);
        const char *equal=memchr(line,'=',(size_t)(end-line));
        if(equal && equal+2<end && equal[1]=='"') {
            const char *start=equal+2; char fields[33][96]={{0}}; int n=0; bool valid=true;
            while(start<end && n<33) {
                const char *next=memchr(start,'~',(size_t)(end-start)); if(!next) break;
                size_t len=(size_t)(next-start); if(len>=sizeof(fields[0])) { valid=false; break; }
                memcpy(fields[n++],start,len); start=next+1;
            }
            tl_quote_t q={.source=2};
            if(valid && n==33 && tl_code_valid(fields[2]) && tl_price_parse(fields[3],&q.price) && tl_price_parse(fields[4],&q.previous)) {
                snprintf(q.code,sizeof(q.code),"%.6s",fields[2]);
                q.time=tl_timestamp(fields[30]); tl_gbk_utf8(fields[1],strlen(fields[1]),q.name,sizeof(q.name));
                char *tail; double pct=strtod(fields[32],&tail);
                if(*fields[32] && !*tail && isfinite(pct) && fabs(((double)q.price/q.previous-1)*100-pct)<=.035 && tl_quote_valid(&q)) out[count++]=q;
            }
        }
        line=*end?end+1:end;
    }
    return count;
}
static struct tm china_time(int64_t stamp)
{
    time_t t=(time_t)(stamp+8*3600); struct tm value={0};
#ifdef _WIN32
    gmtime_s(&value,&t);
#else
    gmtime_r(&t,&value);
#endif
    return value;
}
/* SSE/SZSE 2026 published closure schedule. Unknown years fail closed for alerts.
 * Source: https://www.sse.com.cn/disclosure/dealinstruc/closed/ */
static int trading_day(struct tm t)
{
    if(t.tm_year!=126) return -1;
    if(t.tm_wday==0||t.tm_wday==6) return 0;
    int date=(t.tm_mon+1)*100+t.tm_mday;
    static const int closed[][2]={{101,103},{215,223},{404,406},{501,505},{619,621},{925,927},{1001,1007}};
    for(unsigned i=0;i<sizeof(closed)/sizeof(closed[0]);i++) if(date>=closed[i][0]&&date<=closed[i][1]) return 0;
    return 1;
}
int tl_market_session(int64_t now)
{
    struct tm t=china_time(now); int day=trading_day(t); if(day<=0) return day;
    int seconds=t.tm_hour*3600+t.tm_min*60+t.tm_sec;
    return (seconds>=34200&&seconds<=41415)||(seconds>=46800&&seconds<=54015);
}
tl_quality_t tl_quality(const tl_quote_t *q,int64_t now,bool clock_ok,bool fetched_ok)
{
    if(!tl_quote_valid(q)) return TL_NO_DATA;
    if(!clock_ok) return TL_CLOCK_UNKNOWN;
    if(tl_market_session(now)<0) return TL_CALENDAR_UNKNOWN;
    if(!fetched_ok) return TL_CONNECTION_FAILED;
    int64_t age=now-q->time;
    if(age< -5 || age>16*86400) return TL_STALE;
    if(tl_market_session(now)>0) return age<=120?TL_CURRENT:TL_STALE;
    /* Only call the latest session's closing quote closed, not any old cache. */
    int64_t anchor=now; struct tm t=china_time(anchor);
    int seconds=t.tm_hour*3600+t.tm_min*60+t.tm_sec;
    int close_hour=15,close_min=0;
    if(trading_day(t)>0 && seconds>=41415 && seconds<46800) { close_hour=11; close_min=30; }
    else if(trading_day(t)>0 && seconds>=54015) { /* today's close */ }
    else {
        for(int i=0;i<16;i++) { anchor-=86400; t=china_time(anchor); if(trading_day(t)>0) break; }
    }
    struct tm qt=china_time(q->time);
    int qs=qt.tm_hour*3600+qt.tm_min*60+qt.tm_sec, closing=close_hour*3600+close_min*60;
    /* Providers can publish their closing snapshot after 15:02 (for example
     * Tencent at 15:08). The future-time check above still applies; only the
     * latest session's day and a timestamp at/after its close are accepted. */
    return qt.tm_year==t.tm_year&&qt.tm_yday==t.tm_yday&&qs>=closing-120?TL_CLOSED:TL_STALE;
}
const char *tl_quality_text(tl_quality_t q)
{
    static const char *names[]={"等待报价","等待校时","交易日历待更新","连接失败 · 保留报价","报价已过期","休市报价","最新报价"};
    return q<=TL_CURRENT?names[q]:names[0];
}
const char *tl_source_text(unsigned source) { return source==1?"东方财富":source==2?"腾讯行情":"尚无来源"; }
bool tl_crossing(tl_crossing_t *s,const tl_watch_t *r,const tl_quote_t *q,bool eligible)
{
    if(!eligible||!r->enabled||r->fired||strcmp(r->code,q->code)) { s->baseline=false; return false; }
    /* last_time also acts as an arming barrier, even before a baseline exists. */
    if(q->time<=s->last_time) return false;
    bool continuous=s->baseline && q->source==s->source && q->time-s->last_time<=120 &&
        (q->time+28800)/86400==(s->last_time+28800)/86400;
    bool hit=continuous && (r->below?(s->last_price>r->threshold&&q->price<=r->threshold):(s->last_price<r->threshold&&q->price>=r->threshold));
    s->baseline=true; s->last_price=q->price; s->last_time=q->time; s->source=q->source;
    return hit;
}
