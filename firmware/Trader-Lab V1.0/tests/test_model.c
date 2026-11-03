// SPDX-License-Identifier: GPL-3.0-only
#include "model.h"
#include "alarm_model.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } ++checks; } while(0)
static tl_quote_t quote(int32_t price,const char *stamp)
{
    tl_quote_t q={.price=price,.previous=4000,.time=tl_timestamp(stamp),.source=1};
    strcpy(q.code,"510300"); strcpy(q.name,"沪深300ETF"); return q;
}
int main(int argc,char **argv)
{
    tl_config_t c;tl_defaults(&c);CHECK(tl_config_valid(&c));CHECK(c.count==47);CHECK(!strcmp(c.watch[0].code,"512050"));c.count=65;CHECK(!tl_config_valid(&c));tl_defaults(&c);
    strcpy(c.watch[1].code,c.watch[0].code);CHECK(!tl_config_valid(&c));tl_defaults(&c);
    tl_config_v1_t old={.magic=0x544c0101u,.count=2,.brightness=30,.volume=65,.light=1};
    strcpy(old.ssid,"migration-test");strcpy(old.password,"local-only");
    strcpy(old.watch[0].code,"159915");old.watch[0].enabled=1;old.watch[0].threshold=2000;
    strcpy(old.watch[1].code,"513100");old.watch[1].fired=1;old.watch[1].threshold=3000;
    CHECK(tl_migrate_v1(&old,&c));CHECK(c.count==48);CHECK(!strcmp(c.ssid,old.ssid)&&!strcmp(c.password,old.password));
    CHECK(c.brightness==30&&c.volume==65&&c.light==1);
    CHECK(c.watch[5].enabled&&c.watch[5].threshold==2000);CHECK(c.watch[47].fired&&c.watch[47].threshold==3000);
    old.count=13;CHECK(!tl_migrate_v1(&old,&c));old.count=2;
    old.magic=0;CHECK(!tl_migrate_v1(&old,&c));old.magic=0x544c0101u;
    memset(old.ssid,'x',sizeof(old.ssid));CHECK(!tl_migrate_v1(&old,&c));tl_defaults(&c);
    c.watch[0].enabled=1;CHECK(!tl_config_valid(&c));c.watch[0].threshold=4000;CHECK(tl_config_valid(&c));
    CHECK(tl_code_valid("510300"));CHECK(tl_code_valid("159915"));CHECK(!tl_code_valid("600000"));CHECK(!tl_code_valid("sh510300"));CHECK(!tl_code_valid("51030x"));
    int32_t mills;CHECK(tl_price_parse("4.153",&mills)&&mills==4153);CHECK(tl_price_parse("0.001",&mills)&&mills==1);
    CHECK(!tl_price_parse("0",&mills));CHECK(!tl_price_parse("-1",&mills));CHECK(!tl_price_parse("NaN",&mills));
    CHECK(!tl_price_parse("1e3",&mills));CHECK(!tl_price_parse("1.2345",&mills));CHECK(!tl_price_parse("1.",&mills));
    CHECK(!tl_price_parse("100000.001",&mills));CHECK(!tl_price_parse("99999999999999999999",&mills));
    CHECK(tl_timestamp("20261026113002")==1792985402);
    CHECK(!tl_timestamp("20260230100000"));CHECK(!tl_timestamp("20261301100000"));CHECK(!tl_timestamp("2026102610000x"));
    CHECK(tl_market_session(tl_timestamp("20261026100000"))==1);
    CHECK(tl_market_session(tl_timestamp("20261026120000"))==0);
    CHECK(tl_market_session(tl_timestamp("20261026140000"))==1);
    CHECK(tl_market_session(tl_timestamp("20261010100000"))==0); /* Saturday makeup work is still closed. */
    CHECK(tl_market_session(tl_timestamp("20261001100000"))==0);
    CHECK(tl_market_session(tl_timestamp("20270104100000"))==-1);
    tl_quote_t q=quote(4100,"20261026100000");int64_t now=q.time;
    CHECK(tl_quality(&q,now,true,true)==TL_CURRENT);CHECK(tl_quality(&q,now,false,true)==TL_CLOCK_UNKNOWN);
    CHECK(tl_quality(&q,now,true,false)==TL_CONNECTION_FAILED);CHECK(tl_quality(&q,now+121,true,true)==TL_STALE);
    CHECK(tl_quality(&q,now-6,true,true)==TL_STALE);
    q=quote(4100,"20261026113002");CHECK(tl_quality(&q,tl_timestamp("20261026120000"),true,true)==TL_CLOSED);
    q=quote(4100,"20261023150000");CHECK(tl_quality(&q,tl_timestamp("20261026090000"),true,true)==TL_CLOSED);
    CHECK(tl_quality(&q,tl_timestamp("20261026100000"),true,true)==TL_STALE);
    q=quote(4100,"20261026150001");CHECK(tl_quality(&q,tl_timestamp("20261026180000"),true,true)==TL_CLOSED);
    q=quote(4100,"20261027150812");CHECK(tl_quality(&q,tl_timestamp("20261027153400"),true,true)==TL_CLOSED);
    CHECK(tl_quality(&q,tl_timestamp("20261028100000"),true,true)==TL_STALE);
    CHECK(tl_quality(&q,tl_timestamp("20261027150700"),true,true)==TL_STALE);
    CHECK(tl_quality(&q,tl_timestamp("20261027153400"),true,false)==TL_CONNECTION_FAILED);
    q=quote(4100,"20261027140000");CHECK(tl_quality(&q,tl_timestamp("20261027153400"),true,true)==TL_STALE);
    q=quote(4100,"20261023150812");CHECK(tl_quality(&q,tl_timestamp("20261026090000"),true,true)==TL_CLOSED);
    CHECK(tl_quality(&q,tl_timestamp("20261027153400"),true,true)==TL_STALE);
    tl_watch_t rule={.threshold=4100,.enabled=1};strcpy(rule.code,"510300");tl_crossing_t state={0};
    q=quote(4200,"20261026100000");CHECK(!tl_crossing(&state,&rule,&q,true)); /* no immediate alarm on arming */
    q=quote(4000,"20261026100030");CHECK(!tl_crossing(&state,&rule,&q,true));
    q=quote(4100,"20261026100100");CHECK(tl_crossing(&state,&rule,&q,true));
    CHECK(!tl_crossing(&state,&rule,&q,true));
    rule.fired=1;q=quote(4200,"20261026100130");CHECK(!tl_crossing(&state,&rule,&q,true));rule.fired=0;
    q=quote(4000,"20261026100200");CHECK(!tl_crossing(&state,&rule,&q,true));
    q=quote(4200,"20261026100230");CHECK(!tl_crossing(&state,&rule,&q,false));CHECK(!tl_crossing(&state,&rule,&q,true)); /* network gap resets */
    q=quote(4000,"20261026100300");CHECK(!tl_crossing(&state,&rule,&q,true));
    q=quote(4200,"20261026100330");q.source=2;CHECK(!tl_crossing(&state,&rule,&q,true)); /* source switch resets */
    q=quote(4000,"20261026100400");CHECK(!tl_crossing(&state,&rule,&q,true));
    q=quote(4200,"20261026100700");CHECK(!tl_crossing(&state,&rule,&q,true)); /* no bridge across >120 s */
    memset(&state,0,sizeof(state));rule.below=1;
    q=quote(4200,"20261026100000");CHECK(!tl_crossing(&state,&rule,&q,true));q=quote(4100,"20261026100030");CHECK(tl_crossing(&state,&rule,&q,true));
    q=quote(4200,"20261026095959");CHECK(!tl_crossing(&state,&rule,&q,true));CHECK(state.last_time==tl_timestamp("20261026100030"));
    state=(tl_crossing_t){.last_time=tl_timestamp("20261026100030")};rule.below=0;
    q=quote(4000,"20261026100030");CHECK(!tl_crossing(&state,&rule,&q,true));CHECK(!state.baseline);
    q=quote(4200,"20261026100100");CHECK(!tl_crossing(&state,&rule,&q,true));CHECK(state.baseline);
    tl_quote_t parsed[12];
    const char *valid="{\"data\":{\"diff\":[{\"f12\":\"510300\",\"f14\":\"测试ETF\",\"f2\":4.153,\"f18\":4.178,\"f3\":-0.60,\"f124\":1792985402}]}}";
    CHECK(tl_parse_eastmoney(valid,parsed,12)==1);CHECK(parsed[0].price==4153);CHECK(parsed[0].time==1792985402);
    CHECK(tl_parse_eastmoney("{",parsed,12)==0);CHECK(tl_parse_eastmoney("{\"data\":null}",parsed,12)==0);
    CHECK(tl_parse_eastmoney("{\"data\":{\"diff\":[{\"f12\":\"510300\",\"f14\":\"ETF\",\"f2\":null}]}}",parsed,12)==0);
    char bad[512];strcpy(bad,valid);char *pct=strstr(bad,"-0.60");memcpy(pct,"99.99",5);CHECK(tl_parse_eastmoney(bad,parsed,12)==0);
    strcpy(bad,valid);char *ts=strstr(bad,"1792985402");memcpy(ts,"0000000000",10);CHECK(tl_parse_eastmoney(bad,parsed,12)==0);
    char utf8[64];tl_gbk_utf8("\xc9\xcf\xd6\xa4 ETF",8,utf8,sizeof(utf8));CHECK(!strcmp(utf8,"上证 ETF"));
    char small[4];tl_gbk_utf8("\xc9\xcf\xd6\xa4",4,small,sizeof(small));CHECK(!strcmp(small,"上"));
    CHECK(tl_parse_tencent("v_sh510300=\"1~bad~510300~0~4.1\";",parsed,12)==0);
    for(unsigned frame=0;frame<48000;frame+=17){int value=tt_alarm_sample(TT_RING_MELODY,1047,frame,48000);CHECK(value>=-30001&&value<=30001);}
    CHECK(tt_alarm_sample(TT_RING_MELODY,1047,0,48000)==0);CHECK(tt_alarm_sample(TT_RING_MELODY,1047,47999,48000)==0);
    for(int i=1;i<argc;i++) {
        FILE *f=fopen(argv[i],"rb");CHECK(f!=NULL);fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);CHECK(size>0&&size<=65536);
        char *body=calloc(1,size+1);CHECK(body!=NULL);CHECK(fread(body,1,size,f)==(size_t)size);fclose(f);
        int n=strstr(argv[i],"eastmoney")?tl_parse_eastmoney(body,parsed,12):tl_parse_tencent(body,parsed,12);
        CHECK(n==2);for(int j=0;j<n;j++){CHECK(tl_quote_valid(&parsed[j]));printf("live-fixture code=%s price=%ld time=%lld source=%d\n",parsed[j].code,(long)parsed[j].price,(long long)parsed[j].time,parsed[j].source);}free(body);
    }
    printf("PASS %d checks\n",checks);return 0;
}
