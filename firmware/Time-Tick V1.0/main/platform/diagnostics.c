// SPDX-License-Identifier: GPL-3.0-only
#include "diagnostics.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bsp/display.h"
#include "esp_heap_caps.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "esp_pm.h"
#include "power_monitor.h"

static atomic_uint frames,render_us,render_max_us,flush_us;
static int64_t render_start,flush_start;
static uint8_t *capture;
static size_t capture_size;
static unsigned ui_attempts,ui_missed,loop_max_ms;
static int64_t previous,next_report;
static esp_task_wdt_user_handle_t ui_watchdog;
static bool main_watched;
static atomic_bool display_stress;
static atomic_uint stress_steps;
static atomic_bool power_report;

/* Read-only diagnostic; survives USB unplug/replug within this boot. No
 * background flash writes, fabricated runtime or charging-current estimates. */
static esp_err_t power_test_rpc(const esp_iris_rpc_request_t *request,uint8_t *response,
                               size_t capacity,size_t *used,void *ctx)
{
    (void)ctx;
    if (request->payload_size!=5 || memcmp(request->payload,"power",5)) return ESP_ERR_INVALID_ARG;
    if (capacity<384) return ESP_ERR_INVALID_SIZE;
    tt_power_usage_t u; tt_power_usage_snapshot(&u);
    tt_power_view_t p; tt_power_snapshot(&p);
    *used=snprintf((char *)response,capacity,
        "{\"valid\":%s,\"current_ma\":%d,\"battery_run_active\":%s,\"sample_end_ms\":%lu,"
        "\"sampled_seconds\":%lu,\"average_ma_x10\":%llu,\"used_uah\":%llu,"
        "\"settled_seconds\":%lu,\"settled_ma_x10\":%llu,\"pm_log_queued\":true}",
        p.valid?"true":"false",p.current_ma,u.active?"true":"false",(unsigned long)u.last_ms,
        (unsigned long)(u.elapsed_ms/1000),(unsigned long long)(u.elapsed_ms?u.charge_ma_ms*10/u.elapsed_ms:0),
        (unsigned long long)(u.charge_ma_ms/3600),(unsigned long)(u.settled_ms/1000),
        (unsigned long long)(u.settled_ms?u.settled_ma_ms*10/u.settled_ms:0));
    atomic_store(&power_report,true); return ESP_OK;
}

/* Explicit gateway-only test, never run on boot. No NVS writes or timer edits.
 * Private RPC 0x5454/1: UTF-8 "start", "stop", or "status". */
static esp_err_t display_test_rpc(const esp_iris_rpc_request_t *request,uint8_t *response,
                                  size_t capacity,size_t *used,void *ctx)
{
    (void)ctx;
    if (capacity<96) return ESP_ERR_INVALID_SIZE;
    if (request->payload_size==5 && !memcmp(request->payload,"start",5)) {
        bool idle=false;
        if (!atomic_compare_exchange_strong(&display_stress,&idle,true)) return ESP_ERR_INVALID_STATE;
    } else if (request->payload_size==4 && !memcmp(request->payload,"stop",4)) atomic_store(&display_stress,false);
    else if (request->payload_size!=6 || memcmp(request->payload,"status",6)) return ESP_ERR_INVALID_ARG;
    *used=snprintf((char *)response,capacity,"{\"running\":%s,\"steps\":%u,\"total\":60}",
        atomic_load(&display_stress)?"true":"false",atomic_load(&stress_steps));
    return ESP_OK;
}
void tt_diagnostics_display_request(int *rotation,unsigned *brightness)
{
    static int64_t started;
    static unsigned previous_step;
    if (!atomic_load(&display_stress)) { started=0; return; }
    int64_t now=esp_timer_get_time()/1000;
    if (!started) {
        started=now; previous_step=UINT32_MAX; atomic_store(&stress_steps,0);
        ESP_LOGI("tt_display_test","start: 60 rotations/brightness changes; settings unchanged");
    }
    unsigned step=(unsigned)((now-started)/1000);
    if (step>=60) {
        atomic_store(&display_stress,false); started=0;
        ESP_LOGI("tt_display_test","complete: %u/60 requested steps; restoring normal display",atomic_load(&stress_steps));
        return;
    }
    static const unsigned levels[]={20,75,40,100};
    *rotation=(int)(step%4)*90; *brightness=levels[step%4];
    if (step!=previous_step) {
        previous_step=step; atomic_fetch_add(&stress_steps,1);
        ESP_LOGI("tt_display_test","step=%u rotation=%d brightness=%u",step+1,*rotation,*brightness);
    }
}

static void render_event(lv_event_t *event)
{
    lv_event_code_t code=lv_event_get_code(event);
    if (code==LV_EVENT_FLUSH_START) flush_start=esp_timer_get_time();
    else if (code==LV_EVENT_FLUSH_FINISH) atomic_fetch_add(&flush_us,(unsigned)(esp_timer_get_time()-flush_start));
    else if (code==LV_EVENT_RENDER_START) render_start=esp_timer_get_time();
    else {
        unsigned elapsed=(unsigned)(esp_timer_get_time()-render_start);
        atomic_fetch_add(&frames,1);
        atomic_fetch_add(&render_us,elapsed);
        unsigned max=atomic_load(&render_max_us);
        while (elapsed>max && !atomic_compare_exchange_weak(&render_max_us,&max,elapsed)) {}
    }
}
/* The BSP TE path has one full RGB565 buffer; flush completes under its LVGL
 * lock and leaves panel byte order in that buffer. Copy only on an explicit
 * Iris screenshot request, never during normal animation. */
static esp_err_t screen_begin(const esp_iris_media_desc_t *requested,
                              esp_iris_media_desc_t *actual,uint32_t *size,void *ctx)
{
    (void)requested;
    lv_display_t *display=ctx;
    if (capture) return ESP_ERR_INVALID_STATE;
    uint8_t *copy=heap_caps_malloc(480*480*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if (!copy) return ESP_ERR_NO_MEM;
    if (!bsp_display_lock(500)) { free(copy); return ESP_ERR_TIMEOUT; }
    lv_draw_buf_t *buf=lv_display_get_buf_active(display);
    bool valid=lv_display_get_render_mode(display)==LV_DISPLAY_RENDER_MODE_FULL &&
        buf && buf->header.cf==LV_COLOR_FORMAT_RGB565 &&
        buf->header.w==480 && buf->header.h==480 && buf->header.stride==960;
    if (valid) {
        for (size_t i=0;i<480*480*2;i+=2) { copy[i]=buf->data[i+1]; copy[i+1]=buf->data[i]; }
    }
    bsp_display_unlock();
    if (!valid) { free(copy); return ESP_ERR_NOT_SUPPORTED; }
    capture=copy; capture_size=480*480*2;
    *actual=(esp_iris_media_desc_t){.width=480,.height=480,.stride=960,.format=ESP_IRIS_PIXEL_FORMAT_RGB565};
    *size=capture_size;
    return ESP_OK;
}
static esp_err_t screen_read(uint32_t offset,uint8_t *out,size_t capacity,size_t *used,void *ctx)
{
    (void)ctx;
    if (!capture || offset>=capture_size || !out || !used) return ESP_ERR_INVALID_ARG;
    *used=capture_size-offset; if (*used>capacity) *used=capacity;
    memcpy(out,capture+offset,*used); return ESP_OK;
}
static void screen_end(void *ctx) { (void)ctx; free(capture); capture=NULL; capture_size=0; }
void tt_diagnostics_start(lv_display_t *display)
{
    /* Idle-task monitoring alone misses a task blocked on a semaphore: both
     * cores can idle while the timer/display are frozen. Capture such stalls
     * through the existing panic/coredump + Iris Recovery path. */
    const esp_task_wdt_config_t watchdog={.timeout_ms=15000,
        .idle_core_mask=(1u<<portNUM_PROCESSORS)-1u,.trigger_panic=true};
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_task_wdt_reconfigure(&watchdog));
    main_watched=esp_task_wdt_add(NULL)==ESP_OK;
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_task_wdt_add_user("tt_ui_heartbeat",&ui_watchdog));
    lv_display_add_event_cb(display,render_event,LV_EVENT_RENDER_START,NULL);
    lv_display_add_event_cb(display,render_event,LV_EVENT_RENDER_READY,NULL);
    lv_display_add_event_cb(display,render_event,LV_EVENT_FLUSH_START,NULL);
    lv_display_add_event_cb(display,render_event,LV_EVENT_FLUSH_FINISH,NULL);
    const esp_iris_screen_backend_t screen={.begin=screen_begin,.read=screen_read,.end=screen_end,.user_ctx=display};
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_screen_register(&screen));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_rpc_register(0x5454,1,display_test_rpc,NULL));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_rpc_register(0x5454,3,power_test_rpc,NULL));
}
void tt_diagnostics_ui(bool acquired)
{
    ++ui_attempts;
    if (!acquired) ++ui_missed;
}
void tt_diagnostics_ui_heartbeat(void)
{
    if (ui_watchdog) (void)esp_task_wdt_reset_user(ui_watchdog);
}
void tt_diagnostics_tick(int64_t now)
{
    if (atomic_exchange(&power_report,false)) (void)esp_pm_dump_locks(stdout);
    if (main_watched) (void)esp_task_wdt_reset();
    if (previous && now-previous>(int64_t)loop_max_ms) loop_max_ms=(unsigned)(now-previous);
    previous=now;
    if (!next_report) next_report=now+10000;
    if (now<next_report) return;
    unsigned count=atomic_exchange(&frames,0),total=atomic_exchange(&render_us,0);
    unsigned max=atomic_exchange(&render_max_us,0);
    unsigned transfer=atomic_exchange(&flush_us,0);
    /* Counts are rendered refreshes, not LCD scan FPS; an idle UI may render zero. */
    ESP_LOGI("tt_perf","render=%u avg=%uus flush=%uus max=%uus ui_miss=%u/%u loop_max=%ums internal=%u largest=%u psram=%u",
        count,count?total/count:0,count?transfer/count:0,max,ui_missed,ui_attempts,loop_max_ms,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ui_attempts=ui_missed=loop_max_ms=0; next_report=now+10000;
}
