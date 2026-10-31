// Exercise real application callbacks and the pinned TinyUSB FIFO on a host.
// The critical-section stub checks scope, not physical ISR exclusion.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../main/usb_mic.c"
int test_critical;
static bool mounted=true,suspended;
static tu_fifo_t fifo;
static uint8_t fifo_data[1920],reply[64];
static uint16_t reply_size;
static tinyusb_config_t installed;
static unsigned forwarded;
static void original_event(tinyusb_event_t *event,void *arg){(void)event;assert(arg==&forwarded);++forwarded;}
bool tud_mounted(void){return mounted;}
bool tud_suspended(void){return suspended;}
esp_err_t __real_tinyusb_driver_install(const tinyusb_config_t *cfg){
    assert(cfg->task.xCoreID==0&&cfg->task.priority>6);installed=*cfg;return ESP_OK;
}
tu_fifo_t *tud_audio_n_get_ep_in_ff(uint8_t id){(void)id;assert(test_critical);return &fifo;}
bool tud_audio_n_clear_ep_in_ff(uint8_t id){(void)id;assert(test_critical);tu_fifo_clear(&fifo);return true;}
uint16_t tud_audio_n_write(uint8_t id,const void *data,uint16_t len){(void)id;assert(test_critical);return tu_fifo_write_n(&fifo,data,len);}
bool tud_audio_buffer_and_schedule_control_xfer(uint8_t port,const tusb_control_request_t *r,void *data,uint16_t len){
    (void)port;(void)r;assert(len<=sizeof(reply));memcpy(reply,data,len);reply_size=len;return true;
}
int main(void){
    static const uint8_t descriptors[]={
        TUD_CONFIG_DESCRIPTOR(1,4,0,TUD_CONFIG_DESC_LEN+2*TUD_CDC_DESC_LEN,0,100),
        TUD_CDC_DESCRIPTOR(0,4,0x81,8,0x02,0x82,64),
        TUD_CDC_DESCRIPTOR(2,5,0x83,8,0x04,0x84,64)};
    tusb_desc_device_t device_fixture={0};
    tinyusb_config_t cfg={.descriptor={.device=&device_fixture,.full_speed_config=descriptors},.event_cb=original_event,.event_arg=&forwarded};
    assert(__wrap_tinyusb_driver_install(&cfg)==ESP_OK);
    tu_fifo_config(&fifo,fifo_data,sizeof(fifo_data),true);
    tusb_control_request_t r={.wIndex=5,.wValue=1};
    assert(tud_audio_set_itf_cb(0,&r)&&usb_mic_active());
    int16_t pcm[480];for(int i=0;i<480;++i)pcm[i]=1234;
    usb_mic_push(pcm,480,false);usb_mic_push(pcm,480,false);usb_mic_push(pcm,480,false);
    assert(usb_mic_dropped()==480&&tu_fifo_count(&fifo)==1920);
    usb_mic_push(pcm,480,true);assert(tu_fifo_count(&fifo)==960);
    int16_t out[480];assert(tu_fifo_read_n(&fifo,out,sizeof(out))==sizeof(out));
    for(int i=0;i<480;++i)assert(out[i]==0);
    r=(tusb_control_request_t){.wIndex=0x204,.wValue=0x100,.bRequest=AUDIO20_CS_REQ_CUR,.wLength=1};
    uint8_t value[2]={1,0};assert(tud_audio_set_req_entity_cb(0,&r,value)&&usb_mic_host_mute());
    usb_mic_push(pcm,480,false);tu_fifo_read_n(&fifo,out,sizeof(out));
    for(int i=0;i<480;++i)assert(out[i]==0);
    assert(tud_audio_get_req_entity_cb(0,&r)&&reply_size==1&&reply[0]==1);
    r.wValue=0x101;assert(!tud_audio_set_req_entity_cb(0,&r,value));
    r.wValue=0x200;r.wLength=2;value[0]=0;value[1]=0xfa;
    assert(tud_audio_set_req_entity_cb(0,&r,value)&&usb_mic_host_db()==-6);
    value[0]=1;assert(!tud_audio_set_req_entity_cb(0,&r,value));
    value[0]=0;value[1]=0xc3;assert(!tud_audio_set_req_entity_cb(0,&r,value));
    r.bRequest=AUDIO20_CS_REQ_RANGE;assert(tud_audio_get_req_entity_cb(0,&r)&&reply_size==8&&reply[3]==0xc4&&reply[7]==1);
    r.wIndex=0x404;r.wValue=0x100;r.bRequest=AUDIO20_CS_REQ_CUR;
    assert(tud_audio_get_req_entity_cb(0,&r)&&reply_size==4&&reply[0]==0x80&&reply[1]==0xbb);
    assert(!tud_audio_set_req_entity_cb(0,&r,value));
    mounted=false;usb_mic_push(pcm,480,false);assert(tu_fifo_count(&fifo)==0);
    tinyusb_event_t e={.id=TINYUSB_EVENT_DETACHED};installed.event_cb(&e,installed.event_arg);
    mounted=true;e.id=TINYUSB_EVENT_ATTACHED;installed.event_cb(&e,installed.event_arg);
    assert(!usb_mic_active()&&forwarded==2);
    unsigned before=usb_mic_dropped();usb_mic_push(pcm,480,false);
    assert(tu_fifo_count(&fifo)==0&&usb_mic_dropped()==before);
    tud_audio_tx_done_isr(0,96,0,0x85,1);assert(usb_mic_short_packets()==0);
    tud_audio_tx_done_isr(0,0,0,0x85,1);assert(usb_mic_short_packets()==1);
    assert(!test_critical);
    puts("usb control: mute/volume/clock, bounds, no-overwrite accounting and buffer clearing PASS");
}
