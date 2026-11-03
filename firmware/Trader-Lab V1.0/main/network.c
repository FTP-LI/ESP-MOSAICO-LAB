// SPDX-License-Identifier: GPL-3.0-only
#include "network.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_crt_bundle.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static SemaphoreHandle_t lock;
static tl_config_t desired;
static EXT_RAM_BSS_ATTR tl_net_view_t view;
static unsigned revision;
static atomic_bool scan_requested, refresh_requested, global_refresh_requested;
static atomic_llong synchronized_at;
static RTC_NOINIT_ATTR volatile struct { uint32_t magic,stage; } boot_step;
unsigned tl_network_boot_stage(void) { return boot_step.magic==0x544c4e31?boot_step.stage:0; }
#define BODY_CAP 65536
static void take(void) { xSemaphoreTake(lock,portMAX_DELAY); }
static void give(void) { xSemaphoreGive(lock); }
static void wifi_event(void *arg,esp_event_base_t base,int32_t id,void *data)
{
    (void)arg;
    if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event=data;
        take(); view.connected=true; view.disconnect_reason=0;
        snprintf(view.ip,sizeof(view.ip),IPSTR,IP2STR(&event->ip_info.ip));
        snprintf(view.message,sizeof(view.message),"网络已连接"); view.generation++; give();
        atomic_store(&refresh_requested,true);
        atomic_store(&global_refresh_requested,true);
    } else if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) {
        take(); view.connected=false; view.ip[0]=0;
        memset(view.fetched_ok,0,sizeof(view.fetched_ok));
        memset(view.index_ok,0,sizeof(view.index_ok));
        view.disconnect_reason=data?((wifi_event_sta_disconnected_t *)data)->reason:0;
        snprintf(view.message,sizeof(view.message),"Wi-Fi 已断开 · 自动重试"); view.generation++; give();
    } else if(base==WIFI_EVENT && id==WIFI_EVENT_SCAN_DONE) {
        wifi_event_sta_scan_done_t *event=data;
        wifi_ap_record_t *records=calloc(TL_MAX_AP,sizeof(*records)); uint16_t count=TL_MAX_AP;
        bool ok=event && event->status==0 && records && esp_wifi_scan_get_ap_records(&count,records)==ESP_OK;
        if(!ok) { count=0; (void)esp_wifi_clear_ap_list(); }
        take(); view.scanning=false; view.ap_count=0;
        for(unsigned i=0;i<count;i++) {
            if(!records[i].ssid[0] || memchr(records[i].ssid,'\n',32) || memchr(records[i].ssid,'\r',32)) continue;
            bool duplicate=false;
            for(unsigned j=0;j<view.ap_count;j++) if(!strncmp(view.aps[j].ssid,(char *)records[i].ssid,32)) duplicate=true;
            if(duplicate) continue;
            tl_ap_t *ap=&view.aps[view.ap_count++]; memcpy(ap->ssid,records[i].ssid,32); ap->ssid[32]=0;
            ap->rssi=records[i].rssi; ap->secured=records[i].authmode!=WIFI_AUTH_OPEN;
        }
        snprintf(view.message,sizeof(view.message),ok?"扫描完成":"扫描失败 · 可手动输入"); view.generation++; give(); free(records);
    }
}
static void time_sync(struct timeval *tv) { (void)tv; atomic_store(&synchronized_at,esp_timer_get_time()); }
void tl_network_configure(const tl_config_t *c)
{
    if(!lock) return;
    take();
    if(memcmp(&desired,c,sizeof(*c))) {
        bool changed=desired.count!=c->count;
        for(unsigned i=0;i<c->count&&!changed;i++) changed=strcmp(desired.watch[i].code,c->watch[i].code)!=0;
        desired=*c; revision++;
        if(changed) { memset(view.quotes,0,sizeof(view.quotes)); memset(view.fetched_ok,0,sizeof(view.fetched_ok)); }
        view.generation++; atomic_store(&refresh_requested,true);
    }
    give();
}
void tl_network_scan(void) { atomic_store(&scan_requested,true); }
void tl_network_refresh(void) { atomic_store(&refresh_requested,true); atomic_store(&global_refresh_requested,true); }
void tl_network_snapshot(tl_net_view_t *v)
{
    if(!lock) { memset(v,0,sizeof(*v)); return; }
    take(); *v=view; give();
    int64_t synced=atomic_load(&synchronized_at);
    v->clock_ok=synced>0 && esp_timer_get_time()-synced<86400000000LL;
}
static void wifi_worker(void *arg)
{
    (void)arg;
    struct { char ssid[33],password[65]; } active={0},c;
    int64_t retry_at=0,scan_started=0; bool sntp=false;
    for(;;) {
        bool connected,scanning;
        take(); memcpy(c.ssid,desired.ssid,sizeof(c.ssid)); memcpy(c.password,desired.password,sizeof(c.password));
        connected=view.connected; scanning=view.scanning; give();
        int64_t now=esp_timer_get_time();
        if(strcmp(c.ssid,active.ssid)||strcmp(c.password,active.password)) {
            (void)esp_wifi_disconnect();
            wifi_config_t config={0}; memcpy(config.sta.ssid,c.ssid,strlen(c.ssid)); memcpy(config.sta.password,c.password,strlen(c.password));
            config.sta.pmf_cfg.capable=true; config.sta.pmf_cfg.required=false;
            esp_err_t err=esp_wifi_set_config(WIFI_IF_STA,&config);
            active=c; connected=false; retry_at=0;
            take(); view.connected=false; memset(view.fetched_ok,0,sizeof(view.fetched_ok));
            snprintf(view.message,sizeof(view.message),err==ESP_OK?"正在连接 Wi-Fi":"Wi-Fi 配置失败"); view.generation++; give();
        }
        if(atomic_exchange(&scan_requested,false) && !scanning) {
            esp_err_t err=esp_wifi_scan_start(NULL,false);
            take(); view.scanning=err==ESP_OK; view.generation++;
            if(err!=ESP_OK) snprintf(view.message,sizeof(view.message),"连接处理中 · 请稍后再扫描");
            give(); scan_started=now; scanning=err==ESP_OK;
        }
        if(scanning && now-scan_started>20000000) {
            (void)esp_wifi_scan_stop(); take(); view.scanning=false; view.generation++; give(); scanning=false;
        }
        if(c.ssid[0] && !connected && !scanning && now>=retry_at) {
            (void)esp_wifi_connect(); retry_at=now+30000000;
        }
        if(connected && !sntp) {
            esp_sntp_config_t config=ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,ESP_SNTP_SERVER_LIST("ntp.aliyun.com","pool.ntp.org"));
            config.sync_cb=time_sync;
            sntp=esp_netif_sntp_init(&config)==ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
static char *get_body(const char *url,const char *referer,int *status)
{
    *status=0;
    esp_http_client_config_t config={.url=url,.timeout_ms=5000,.crt_bundle_attach=esp_crt_bundle_attach,
        .disable_auto_redirect=true,.buffer_size=2048,.user_agent="Mozilla/5.0 Trader-Lab/0.1"};
    esp_http_client_handle_t client=esp_http_client_init(&config); if(!client) return NULL;
    esp_http_client_set_header(client,"Referer",referer); esp_http_client_set_header(client,"Accept-Encoding","identity");
    char *body=calloc(1,BODY_CAP+1); bool ok=false;
    int64_t deadline=esp_timer_get_time()+12000000;
    if(!body || esp_http_client_open(client,0)!=ESP_OK) goto done;
    int64_t length=esp_http_client_fetch_headers(client); *status=esp_http_client_get_status_code(client);
    if(length<0 || length>BODY_CAP || *status!=200) goto done;
    int used=0;
    while(used<BODY_CAP && esp_timer_get_time()<deadline) {
        int n=esp_http_client_read(client,body+used,BODY_CAP-used);
        if(n<0) goto done;
        if(!n) break;
        used+=n;
    }
    if(used>=BODY_CAP || !esp_http_client_is_complete_data_received(client)) goto done;
    body[used]=0; ok=true;
done:
    esp_http_client_close(client); esp_http_client_cleanup(client);
    if(!ok) { free(body); body=NULL; }
    return body;
}
/* One HTTP worker serializes ETF and global requests: no parallel TLS peaks. */
static void fetch_global(void)
{
    tl_index_quote_t parsed[TL_GLOBAL_COUNT]={0};int status=0;
    const char *url="https://qt.gtimg.cn/q=sh000001,sz399001,sz399006,hkHSI,gzN225,gzKS11,usINX,usIXIC,usDJI,gzGDAXI,gzFTSE,gzFCHI";
    char *body=get_body(url,"https://gu.qq.com/",&status);
    int received=body?tl_global_tencent(body,parsed):0;free(body);
    bool failed=false;int64_t wall=time(NULL);
    take();view.global_requests++;
    for(unsigned i=0;i<TL_GLOBAL_COUNT;i++) {
        bool ok=view.connected&&tl_global_valid(&parsed[i],wall)&&parsed[i].time>=view.indices[i].time;
        if(ok)view.indices[i]=parsed[i];else failed=true;
        view.index_ok[i]=ok;
    }
    if(failed)view.global_failures++;
    view.generation++;give();
    if(failed)ESP_LOGW("tl_global","http=%d parsed=%d/12",status,received);
}
static void *json_external_alloc(size_t size) { return heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT); }
static void fetch_worker(void *arg)
{
    (void)arg;int64_t next=0,last=0,primary_retry=0,global_next=0;unsigned failures=0;
    typedef struct {tl_quote_t results[TL_MAX_WATCH],parsed[TL_MAX_WATCH];} scratch_t;
    scratch_t *scratch=heap_caps_calloc(1,sizeof(*scratch),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!scratch){take();snprintf(view.message,sizeof(view.message),"行情缓冲不足 · 请重启");view.generation++;give();vTaskDelete(NULL);return;}
    for(;;) {
        tl_config_t c;unsigned request_revision;bool connected,scanning;
        take();c=desired;request_revision=revision;connected=view.connected;scanning=view.scanning;give();
        int64_t now=esp_timer_get_time(),synced=atomic_load(&synchronized_at);
        if(!connected||scanning||!synced||now-synced>=86400000000LL){vTaskDelay(pdMS_TO_TICKS(250));continue;}
        if(atomic_exchange(&global_refresh_requested,false))global_next=0;
        bool manual=atomic_load(&refresh_requested);
        if(!c.count||now-last<5000000||(!manual&&now<next)) {
            if(now>=global_next){fetch_global();global_next=esp_timer_get_time()+120000000;}
            vTaskDelay(pdMS_TO_TICKS(250));continue;
        }
        atomic_store(&refresh_requested,false);last=now;
        take();view.fetching=true;give();
        memset(scratch,0,sizeof(*scratch));tl_quote_t *results=scratch->results,*parsed=scratch->parsed;
        bool accepted[TL_MAX_WATCH]={0},fresh[TL_MAX_WATCH]={0};int status=0;unsigned received=0,fresh_count=0;
        for(int provider=1;provider<=2&&fresh_count<c.count;provider++) {
            if(provider==1&&now<primary_retry)continue;
            char url[768];size_t at;
            if(provider==1)at=(size_t)snprintf(url,sizeof(url),"https://push2.eastmoney.com/api/qt/ulist.np/get?fltt=2&invt=2&fields=f12,f14,f2,f3,f18,f124&secids=");
            else at=(size_t)snprintf(url,sizeof(url),"https://qt.gtimg.cn/q=");
            bool first=true;
            for(unsigned i=0;i<c.count;i++)if(!fresh[i]) {
                int n=snprintf(url+at,sizeof(url)-at,"%s%s%s",first?"":",",provider==1?(c.watch[i].code[0]=='5'?"1.":"0."):(c.watch[i].code[0]=='5'?"sh":"sz"),c.watch[i].code);
                if(n<0||(size_t)n>=sizeof(url)-at){at=sizeof(url);break;}at+=(size_t)n;first=false;
            }
            if(at>=sizeof(url))continue;
            char *body=get_body(url,provider==1?"https://quote.eastmoney.com/":"https://gu.qq.com/",&status);
            take();view.requests++;view.http_status=status;give();
            if(provider==1&&(status==403||status==429||status==401||status==514))primary_retry=esp_timer_get_time()+600000000;
            if(!body)continue;
            int count=provider==1?tl_parse_eastmoney(body,parsed,TL_MAX_WATCH):tl_parse_tencent(body,parsed,TL_MAX_WATCH);
            free(body);int64_t wall=time(NULL);
            for(int j=0;j<count;j++)for(unsigned i=0;i<c.count;i++) {
                if(strcmp(parsed[j].code,c.watch[i].code)||fresh[i]||parsed[j].time>wall+5||wall-parsed[j].time>16*86400||
                   (accepted[i]&&parsed[j].time<results[i].time))continue;
                if(!accepted[i])received++;
                results[i]=parsed[j];accepted[i]=true;
                tl_quality_t quality=tl_quality(&results[i],wall,true,true);
                if(quality==TL_CURRENT||quality==TL_CLOSED){fresh[i]=true;fresh_count++;}
            }
        }
        take();
        if(request_revision==revision&&view.connected) {
            for(unsigned i=0;i<c.count;i++) {
                bool monotonic=accepted[i]&&results[i].time>=view.quotes[i].time;
                if(monotonic)view.quotes[i]=results[i];
                view.fetched_ok[i]=monotonic;
            }
            if(fresh_count<c.count)view.failures++;
            snprintf(view.message,sizeof(view.message),fresh_count==c.count?"报价已更新":fresh_count?"部分报价不可用":received?"收到旧报价 · 稍后重试":"行情获取失败 · 将自动重试");
            view.generation++;
        }
        view.fetching=false;give();
        failures=fresh_count?0:(failures<4?failures+1:4);
        int delay=failures?(30<<failures):(tl_market_session(time(NULL))>0?30:300);if(delay>300)delay=300;
        next=esp_timer_get_time()+(int64_t)delay*1000000;
    }
}
bool tl_network_start(const tl_config_t *c)
{
    boot_step.magic=0x544c4e31;boot_step.stage=1;
    cJSON_Hooks json_hooks={.malloc_fn=json_external_alloc,.free_fn=free};cJSON_InitHooks(&json_hooks);
    lock=xSemaphoreCreateMutex(); if(!lock) return false;
    desired=*c; revision=1; snprintf(view.message,sizeof(view.message),c->ssid[0]?"正在连接 Wi-Fi":"请在设置中连接 Wi-Fi");
    boot_step.stage=2;esp_err_t err=esp_netif_init(); if(err!=ESP_OK) return false;
    boot_step.stage=3;
    err=esp_event_loop_create_default(); if(err!=ESP_OK && err!=ESP_ERR_INVALID_STATE) return false;
    boot_step.stage=4;
    if(!esp_netif_create_default_wifi_sta()) return false;
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    /* Credentials belong to traderlab; do not load/write another app's global
     * Wi-Fi driver settings before WIFI_STORAGE_RAM has taken effect. */
    init.nvs_enable=0;
    boot_step.stage=5;if(esp_wifi_init(&init)!=ESP_OK) return false;
    boot_step.stage=6;if(esp_wifi_set_storage(WIFI_STORAGE_RAM)!=ESP_OK) return false;
    boot_step.stage=7;if(esp_wifi_set_mode(WIFI_MODE_STA)!=ESP_OK) return false;
    boot_step.stage=8;
    if(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL)!=ESP_OK || esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event,NULL)!=ESP_OK) return false;
    boot_step.stage=9;
    if(esp_wifi_start()!=ESP_OK) return false;
    boot_step.stage=10;
    (void)esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    boot_step.stage=11;
    return xTaskCreate(wifi_worker,"tl_wifi",6144,NULL,3,NULL)==pdPASS && xTaskCreate(fetch_worker,"tl_quotes",16384,NULL,2,NULL)==pdPASS;
}
