#include "tusb.h"
#include "usb_layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint8_t source[]={
    TUD_CONFIG_DESCRIPTOR(1,4,0,TUD_CONFIG_DESC_LEN+2*TUD_CDC_DESC_LEN,0,100),
    TUD_CDC_DESCRIPTOR(0,4,0x81,8,0x02,0x82,64),
    TUD_CDC_DESCRIPTOR(2,5,0x83,8,0x04,0x84,64)
};
int main(void){
    uint8_t out[512],bad[sizeof(source)];
    for(int hs=0;hs<2;++hs){
        size_t n=mic_usb_extend(source,sizeof(source),out,sizeof(out),hs);
        assert(n==sizeof(source)+TUD_AUDIO20_MIC_ONE_CH_DESC_LEN && out[4]==6);
        assert((size_t)(out[2]|out[3]<<8)==n);
        assert(!memcmp(source+9,out+9,sizeof(source)-9));
        bool ep=false,feature=false;unsigned interfaces=0;
        for(size_t p=9;p<n;p+=out[p]){
            assert(out[p]>=2 && p+out[p]<=n);
            if(out[p+1]==4 && out[p+3]==0)++interfaces;
            if(p>=sizeof(source) && out[p+1]==0x24 && out[p+2]==6){
                feature=true;assert(out[p+5]==0x0f);
                for(size_t c=9;c<13;++c)assert(out[p+c]==0);
            }
            if(out[p+1]==5 && out[p+2]==0x85){ep=true;assert(out[p+4]==98 && out[p+5]==0 && out[p+6]==(hs?4:1));}
        }
        assert(ep && feature && interfaces==6);
    }
    assert(!mic_usb_extend(source,sizeof(source)-1,out,sizeof(out),false));
    assert(!mic_usb_extend(source,sizeof(source),out,10,false));
    memcpy(bad,source,sizeof(bad));bad[4]=3;
    assert(!mic_usb_extend(bad,sizeof(bad),out,sizeof(out),false));
    memcpy(bad,source,sizeof(bad));bad[9]=0;
    assert(!mic_usb_extend(bad,sizeof(bad),out,sizeof(out),false));
    memcpy(bad,source,sizeof(bad));
    for(size_t p=9;p<sizeof(bad);p+=bad[p])if(bad[p+1]==5){bad[p+2]=0x85;break;}
    assert(!mic_usb_extend(bad,sizeof(bad),out,sizeof(out),false));
    puts("usb: FS/HS UAC mono, interval, packet, retained CDC and malformed descriptor rejection PASS");
}
