// SPDX-License-Identifier: GPL-3.0-only
#include "usb_layout.h"
#include <string.h>
#include "tusb.h"
size_t mic_usb_extend(const uint8_t *s,size_t len,uint8_t *out,size_t cap,bool hs){
    const uint8_t audio[]={TUD_AUDIO20_MIC_ONE_CH_DESCRIPTOR(4,0,2,16,0x85,98)};
    if(!s||!out||len!=TUD_CONFIG_DESC_LEN+2*TUD_CDC_DESC_LEN||len+sizeof(audio)>cap||
       s[0]!=9||s[1]!=2||s[4]!=4||(size_t)(s[2]|s[3]<<8)!=len)return 0;
    unsigned interfaces=0;
    for(size_t at=9;at<len;at+=s[at]){
        if(s[at]<2||at+s[at]>len)return 0;
        if(s[at+1]==4){if(s[at]<9||s[at+2]>=4)return 0;++interfaces;}
        if(s[at+1]==5 && (s[at]<7||s[at+2]==0x85))return 0;
    }
    if(interfaces!=4)return 0;
    memcpy(out,s,len);memcpy(out+len,audio,sizeof(audio));size_t total=len+sizeof(audio);
    out[2]=(uint8_t)total;out[3]=(uint8_t)(total>>8);out[4]=6;
    for(size_t at=len;at<total;at+=out[at]){
        if(out[at+1]==5)out[at+6]=hs?4:1; /* 1ms at either bus speed */
        if(out[at+1]==0x24 && out[at+2]==6){ /* Master controls only. */
            memset(out+at+9,0,4);
        }
    }
    return total;
}
