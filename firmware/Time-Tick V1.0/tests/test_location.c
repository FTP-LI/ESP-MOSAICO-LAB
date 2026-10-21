// SPDX-License-Identifier: GPL-3.0-only
#include "location_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    tt_location_request_t request={.id=9,.kind=TT_LOCATION_SEARCH,.language=TT_ZH,.query="杭州, CN&count=99"};
    char url[512];
    assert(tt_location_url(&request,url,sizeof(url)));
    assert(strstr(url,"%E6%9D%AD%E5%B7%9E") && strstr(url,"%26count%3D99"));
    assert(strstr(url,"&count=5&language=zh"));
    assert(!tt_location_url(&request,url,12));
    memset(request.query,'a',sizeof(request.query));
    assert(!tt_location_url(&request,url,sizeof(url)));
    strcpy(request.query,"x"); assert(!tt_location_url(&request,url,sizeof(url)));
    request.kind=TT_LOCATION_IP;
    assert(tt_location_url(&request,url,sizeof(url)) && strstr(url,"https://ipwho.is/"));
    tt_location_view_t result={.id=9,.kind=TT_LOCATION_SEARCH};
    tt_location_parse("{\"results\":[{\"name\":\"Same\",\"admin1\":\"One\",\"country\":\"CN\",\"latitude\":30,\"longitude\":120},"
                      "{\"name\":\"Same\",\"admin1\":\"Two\",\"country\":\"CN\",\"latitude\":31,\"longitude\":121},"
                      "{\"name\":\"Duplicate\",\"latitude\":30,\"longitude\":120},"
                      "{\"name\":\"Invalid\",\"latitude\":91,\"longitude\":120},"
                      "{\"name\":\"Missing\",\"latitude\":30}]}",&result);
    assert(result.id==9 && result.state==TT_LOCATION_READY && result.count==2);
    assert(strstr(result.places[1].region,"Two") && result.places[1].latitude_e6==31000000);
    tt_location_parse("{}",&result); assert(result.state==TT_LOCATION_EMPTY && result.count==0);
    tt_location_parse("{\"error\":true}",&result); assert(result.state==TT_LOCATION_FAILED);
    tt_location_parse("{\"results\":false}",&result); assert(result.state==TT_LOCATION_FAILED);
    tt_location_parse("{}", &result);
    tt_location_parse("{}garbage",&result); assert(result.state==TT_LOCATION_FAILED);
    tt_location_parse(NULL,&result); assert(result.state==TT_LOCATION_FAILED && !result.count);
    result.kind=TT_LOCATION_IP;
    tt_location_parse("{\"success\":false,\"city\":\"Wrong\",\"latitude\":30,\"longitude\":120}",&result);
    assert(result.state==TT_LOCATION_FAILED && !result.count);
    tt_location_parse("{\"success\":true,\"city\":\"杭州\",\"region\":\"浙江省\",\"country\":\"中国\",\"latitude\":30.2741,\"longitude\":120.1551}",&result);
    assert(result.state==TT_LOCATION_READY && result.count==1);
    assert(result.places[0].latitude_e6==30274100 && result.places[0].longitude_e6==120155100);
    assert(!strcmp(result.places[0].name,"杭州"));
    tt_location_parse("{\"success\":true,\"city\":\"\\nSafe\\tName\",\"latitude\":-90,\"longitude\":180}",&result);
    assert(!strcmp(result.places[0].name,"SafeName") && result.places[0].latitude_e6==-90000000);
    puts("PASS: bounded encoded queries, coordinate validation, provider errors, ambiguous results, deduplication and UTF-8 place names");
    return 0;
}
