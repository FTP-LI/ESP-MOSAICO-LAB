// SPDX-License-Identifier: GPL-3.0-only
#include "usb_mic.h"
#include "usb_layout.h"
#include <stdatomic.h>
#include <string.h>
#include "tinyusb.h"
#include "tusb.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
static uint8_t fs[512],hs[512];
static tusb_desc_device_t device;
static atomic_bool streaming,host_mute;
static atomic_int host_db;
static atomic_uint generation,dropped,short_packets;
static portMUX_TYPE fifo_lock=portMUX_INITIALIZER_UNLOCKED;
static tinyusb_event_cb_t iris_event;
static void *iris_event_arg;
static void usb_event(tinyusb_event_t *event,void *arg){
    (void)arg;
    if(event->id==TINYUSB_EVENT_ATTACHED||event->id==TINYUSB_EVENT_DETACHED){
        atomic_store(&streaming,false);atomic_fetch_add(&generation,1);
    }
    if(iris_event)iris_event(event,iris_event_arg);
}
_Static_assert(CFG_TUD_CDC==2 && CFG_TUD_AUDIO==1,"Mic-Lab requires two CDCs and UAC");
esp_err_t __real_tinyusb_driver_install(const tinyusb_config_t *config);
esp_err_t __wrap_tinyusb_driver_install(const tinyusb_config_t *config){
    if(!config||!config->descriptor.device||!config->descriptor.full_speed_config)return ESP_ERR_INVALID_ARG;
    const uint8_t *src=config->descriptor.full_speed_config;
    if(!mic_usb_extend(src,src[2]|src[3]<<8,fs,sizeof(fs),false))return ESP_ERR_NOT_SUPPORTED;
    tinyusb_config_t c=*config;device=*c.descriptor.device;device.bcdDevice=0x0301;
    // The pinned driver initializes its USB interrupt from this task.
    // mic_audio is pinned to the same core so a short critical section also
    // excludes ISR reads and USB task resets of the unprotected audio FIFO.
    c.task.xCoreID=0;
    c.task.priority=7; // Above audio task (6): it cannot preempt a USB reset.
    iris_event=c.event_cb;iris_event_arg=c.event_arg;
    c.event_cb=usb_event;c.event_arg=NULL;
    c.descriptor.device=&device;c.descriptor.full_speed_config=fs;
    if(c.descriptor.high_speed_config){src=c.descriptor.high_speed_config;
        if(!mic_usb_extend(src,src[2]|src[3]<<8,hs,sizeof(hs),true))return ESP_ERR_NOT_SUPPORTED;
        c.descriptor.high_speed_config=hs;}
    ESP_LOGI("mic_usb","CDC 0-3 + UAC2 4-5, mono 48kHz/16-bit, IN 0x85");
    return __real_tinyusb_driver_install(&c);
}
bool usb_mic_connected(void){return tud_mounted()&&!tud_suspended();}
bool usb_mic_active(void){return usb_mic_connected()&&atomic_load(&streaming);}
bool usb_mic_host_mute(void){return atomic_load(&host_mute);}
int usb_mic_host_db(void){return atomic_load(&host_db);}
unsigned usb_mic_dropped(void){return atomic_load(&dropped);}
unsigned usb_mic_short_packets(void){return atomic_load(&short_packets);}
void usb_mic_push(const int16_t *pcm,size_t samples,bool silence){
    static bool previous_active,previous_silence;
    static unsigned seen_generation;
    portENTER_CRITICAL(&fifo_lock);
    bool active=usb_mic_active();silence|=usb_mic_host_mute();
    unsigned now=atomic_load(&generation);
    if(active!=previous_active||silence!=previous_silence||now!=seen_generation){
        tud_audio_clear_ep_in_ff();previous_active=active;previous_silence=silence;seen_generation=now;
    }
    if(!active){portEXIT_CRITICAL(&fifo_lock);return;}
    tu_fifo_t *fifo=tud_audio_get_ep_in_ff();
    if(!fifo){portEXIT_CRITICAL(&fifo_lock);return;}
    tu_fifo_set_overwritable(fifo,false);
    static const int16_t zero[480]={0};
    if(samples>480)samples=480;
    uint16_t bytes=(uint16_t)(samples*2),written=tud_audio_write(silence?zero:pcm,bytes);
    portEXIT_CRITICAL(&fifo_lock);
    if(written!=bytes)atomic_fetch_add(&dropped,(bytes-written)/2);
}
bool tud_audio_tx_done_isr(uint8_t port,uint16_t bytes,uint8_t function,uint8_t ep,uint8_t alt){
    (void)port;(void)function;(void)ep;
    // Flow control legitimately uses 47/48/49 samples. Smaller packets are
    // diagnostic only: startup and extra host polls can also produce them.
    if(alt&&bytes<94)atomic_fetch_add(&short_packets,1);
    return true;
}
bool tud_audio_set_itf_cb(uint8_t port,tusb_control_request_t const *r){
    (void)port;if(r->wIndex!=5||r->wValue>1)return false;
    atomic_store(&streaming,r->wValue==1);atomic_fetch_add(&generation,1);return true;
}
bool tud_audio_set_itf_close_ep_cb(uint8_t port,tusb_control_request_t const *r){
    (void)port;if(r->wIndex==5){atomic_store(&streaming,false);atomic_fetch_add(&generation,1);}return true;
}
bool tud_audio_set_req_entity_cb(uint8_t port,tusb_control_request_t const *r,uint8_t *data){
    (void)port;
    if((r->wIndex&255)!=4||(r->wValue&255)!=0||r->bRequest!=AUDIO20_CS_REQ_CUR)return false;
    unsigned entity=r->wIndex>>8,ctrl=r->wValue>>8;
    if(entity==2&&ctrl==AUDIO20_FU_CTRL_MUTE&&r->wLength==1&&data[0]<=1){atomic_store(&host_mute,data[0]);return true;}
    if(entity==2&&ctrl==AUDIO20_FU_CTRL_VOLUME&&r->wLength==2){
        int16_t raw=(int16_t)((uint16_t)data[0]|(uint16_t)data[1]<<8);
        if(raw>0||raw<-60*256||raw%256)return false;
        atomic_store(&host_db,raw/256);return true;
    }
    return false;
}
bool tud_audio_get_req_entity_cb(uint8_t port,tusb_control_request_t const *r){
    if((r->wIndex&255)!=4||(r->wValue&255)!=0)return false;
    unsigned entity=r->wIndex>>8,ctrl=r->wValue>>8;
    uint8_t buf[14]={0};uint16_t n=0;
    if(entity==4 && ctrl==AUDIO20_CS_CTRL_SAM_FREQ){
        if(r->bRequest==AUDIO20_CS_REQ_CUR){buf[0]=0x80;buf[1]=0xbb;n=4;}
        else if(r->bRequest==AUDIO20_CS_REQ_RANGE){buf[0]=1;buf[2]=buf[6]=0x80;buf[3]=buf[7]=0xbb;n=14;}
    }else if(entity==4&&ctrl==AUDIO20_CS_CTRL_CLK_VALID&&r->bRequest==AUDIO20_CS_REQ_CUR){buf[0]=1;n=1;
    }else if(entity==1&&ctrl==AUDIO20_TE_CTRL_CONNECTOR&&r->bRequest==AUDIO20_CS_REQ_CUR){buf[0]=1;n=6;
    }else if(entity==2&&ctrl==AUDIO20_FU_CTRL_MUTE&&r->bRequest==AUDIO20_CS_REQ_CUR){buf[0]=usb_mic_host_mute();n=1;
    }else if(entity==2&&ctrl==AUDIO20_FU_CTRL_VOLUME){
        if(r->bRequest==AUDIO20_CS_REQ_CUR){int16_t v=(int16_t)(usb_mic_host_db()*256);buf[0]=(uint8_t)v;buf[1]=(uint8_t)(v>>8);n=2;}
        else if(r->bRequest==AUDIO20_CS_REQ_RANGE){buf[0]=1;buf[2]=0;buf[3]=0xc4;buf[6]=0;buf[7]=1;n=8;}
    }
    return n&&tud_audio_buffer_and_schedule_control_xfer(port,r,buf,n);
}
