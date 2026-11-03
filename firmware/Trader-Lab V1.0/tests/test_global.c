// SPDX-License-Identifier: GPL-3.0-only
#include "global_market.h"
#include "model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { if(!(x)){fprintf(stderr,"global FAIL line %d: %s\n",__LINE__,#x);exit(1);}checks++; } while(0)
static void tencent(char *out,size_t capacity,const char *symbol,const char *stamp,const char *pct)
{
    size_t used=(size_t)snprintf(out,capacity,"v_%s=\"",symbol);
    for(unsigned i=0;i<34;i++) {
        const char *v=i==3?"101":i==4?"100":i==30?stamp:i==32?pct:"0";
        used+=(size_t)snprintf(out+used,capacity-used,"%s~",v);
    }
    snprintf(out+used,capacity-used,"\";");
}
static char *fixture(const char *dir,const char *name)
{
    char path[512];snprintf(path,sizeof(path),"%s/%s",dir,name);
    FILE *f=fopen(path,"rb");CHECK(f);fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
    CHECK(size>0&&size<65536);char *data=calloc(1,size+1);CHECK(data);
    CHECK(fread(data,1,size,f)==(size_t)size);fclose(f);return data;
}
int main(int argc,char **argv)
{
    tl_index_quote_t q[TL_GLOBAL_COUNT]={0};char body[2048];
    tencent(body,sizeof(body),"sh000001","20261027150000","1.00");
    CHECK(tl_global_tencent(body,q)==1);CHECK(q[0].price==10100&&q[0].previous==10000);
    CHECK(q[0].time==tl_timestamp("20261027150000"));
    CHECK(tl_global_valid(&q[0],q[0].time+86400));CHECK(!tl_global_valid(&q[0],q[0].time-301));CHECK(!tl_global_valid(&q[0],q[0].time+17*86400));
    tencent(body,sizeof(body),"hkHSI","2026/10/27 16:00:00","1.00");CHECK(tl_global_tencent(body,q)==1);CHECK(q[3].time==tl_timestamp("20261027160000"));
    tencent(body,sizeof(body),"usINX","2026-10-26 16:43:41","1.00");CHECK(tl_global_tencent(body,q)==1);CHECK(q[6].time==tl_timestamp("20261027044341"));
    tencent(body,sizeof(body),"usINX","2026-11-02 16:00:00","1.00");CHECK(tl_global_tencent(body,q)==1);CHECK(q[6].time==tl_timestamp("20261103050000"));
    tencent(body,sizeof(body),"usINX","2026-03-09 16:00:00","1.00");CHECK(tl_global_tencent(body,q)==1);CHECK(q[6].time==tl_timestamp("20260310040000"));
    tencent(body,sizeof(body),"usINX","2026-03-06 16:00:00","1.00");CHECK(tl_global_tencent(body,q)==1);CHECK(q[6].time==tl_timestamp("20260307050000"));
    tencent(body,sizeof(body),"usINX","2026-02-30 16:00:00","1.00");CHECK(!tl_global_tencent(body,q));
    tencent(body,sizeof(body),"sh000001","20261027150000","9.00");CHECK(!tl_global_tencent(body,q));
    CHECK(!tl_global_tencent("v_sh000001=\"0~bad\";",q));
    const char *compact="v_gzN225=\"N225~Nikkei~2026-10-27 14:30:01~63268.15~1078.50~1.73~AP~\";";
    CHECK(tl_global_tencent(compact,q)==1);CHECK(q[4].price==6326815&&q[4].previous==6218965);
    CHECK(q[4].time==tl_timestamp("20261027143001"));
    strcpy(body,compact);memcpy(strstr(body,"1.73"),"9.99",4);CHECK(!tl_global_tencent(body,q));
    strcpy(body,compact);memcpy(strstr(body,"N225~"),"AXJO",4);CHECK(!tl_global_tencent(body,q));
    strcpy(body,compact);memcpy(strstr(body,"63268.15"),"-0000.25",8);CHECK(!tl_global_tencent(body,q));
    strcpy(body,compact);memcpy(strstr(body,"2026-10-27"),"2026-02-30",10);CHECK(!tl_global_tencent(body,q));
    CHECK(tl_global_tencent("v_gzGDAXI=\"GDAXI~DAX~2026-10-27 16:45:57~25364.33~-46.51~-0.18~EU~\";",q)==1);
    CHECK(q[9].previous==2541084&&q[9].time==tl_timestamp("20261027164557"));
    if(argc>1) {
        memset(q,0,sizeof(q));char *data=fixture(argv[1],"tencent-global.txt");CHECK(tl_global_tencent(data,q)==7);free(data);
        data=fixture(argv[1],"tencent-global-gz.txt");CHECK(tl_global_tencent(data,q)==4);free(data);
        data=fixture(argv[1],"tencent-global-regional.txt");CHECK(tl_global_tencent(data,q)==1);free(data);
        for(unsigned i=0;i<TL_GLOBAL_COUNT;i++)CHECK(tl_global_valid(&q[i],q[i].time)); /* Recorded fixtures need not be current. */
    }
    printf("PASS global %u checks\n",checks);return 0;
}
