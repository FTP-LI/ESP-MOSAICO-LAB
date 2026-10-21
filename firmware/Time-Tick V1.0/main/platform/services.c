// SPDX-License-Identifier: GPL-3.0-only
#include "services.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static QueueHandle_t config_queue;
static QueueHandle_t location_queue;
static SemaphoreHandle_t view_lock;
static EventGroupHandle_t wifi_events;
static tt_services_view_t view;
typedef struct { tt_settings_t settings; uint32_t revision; } service_request_t;
static uint32_t requested_revision; /* Protected by view_lock. */
static tt_settings_t desired;
static bool scan_requested, ready;
static int disconnect_reason;
#define CONNECTED BIT0
#define RESPONSE_CAPACITY 8192

static void message(const char *text, uint32_t revision)
{
    xSemaphoreTake(view_lock, portMAX_DELAY);
    if (revision == requested_revision) snprintf(view.message, sizeof(view.message), "%s", text);
    xSemaphoreGive(view_lock);
}

/* The Wi-Fi task owns driver calls. The event loop only publishes snapshots. */
static void scan_done(bool success)
{
    /* Default ESP event-loop stack is small; scan records belong on the heap. */
    wifi_ap_record_t *records=success?calloc(TT_WIFI_MAX_APS,sizeof(*records)):NULL;
    uint16_t count=TT_WIFI_MAX_APS;
    success=success && records;
    if (success) success=esp_wifi_scan_get_ap_records(&count,records)==ESP_OK;
    if (!success) { count=0; (void)esp_wifi_clear_ap_list(); }
    xSemaphoreTake(view_lock,portMAX_DELAY);
    view.ap_count=0;
    for (unsigned i=0;i<count;++i) {
        if (!records[i].ssid[0]) continue;
        bool duplicate=false;
        for (unsigned j=0;j<view.ap_count;++j)
            if (!strncmp(view.aps[j].ssid,(char *)records[i].ssid,32)) { duplicate=true; break; }
        if (duplicate) continue;
        tt_wifi_ap_t *ap=&view.aps[view.ap_count++];
        memcpy(ap->ssid,records[i].ssid,32); ap->ssid[32]=0;
        ap->rssi=records[i].rssi; ap->secured=records[i].authmode!=WIFI_AUTH_OPEN;
    }
    /* ESP-IDF returns records strongest first; deduplication retains that AP. */
    view.scan_state=success?TT_SCAN_DONE:TT_SCAN_FAILED;
    ++view.scan_generation;
    xSemaphoreGive(view_lock);
    free(records);
}
static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base==WIFI_EVENT && id==WIFI_EVENT_SCAN_DONE) {
        const wifi_event_sta_scan_done_t *event=data;
        scan_done(event && event->status==0); return;
    }
    if (base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_events,CONNECTED);
        xSemaphoreTake(view_lock,portMAX_DELAY); disconnect_reason=0; xSemaphoreGive(view_lock);
    }
    if (base==WIFI_EVENT && (id==WIFI_EVENT_STA_DISCONNECTED || id==WIFI_EVENT_STA_STOP)) {
        xEventGroupClearBits(wifi_events,CONNECTED);
        xSemaphoreTake(view_lock,portMAX_DELAY);
        disconnect_reason=id==WIFI_EVENT_STA_DISCONNECTED && data?((wifi_event_sta_disconnected_t *)data)->reason:0;
        xSemaphoreGive(view_lock);
    }
}

static cJSON *get_json(const char *url)
{
    esp_http_client_config_t config = {
        .url = url, .timeout_ms = 10000, .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_auto_redirect = true, .buffer_size = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return NULL;
    cJSON *json = NULL;
    char *body = calloc(1, RESPONSE_CAPACITY + 1);
    if (!body || esp_http_client_open(client, 0) != ESP_OK) goto done;
    int64_t length = esp_http_client_fetch_headers(client);
    if (length < 0 || length > RESPONSE_CAPACITY || esp_http_client_get_status_code(client) != 200)
        goto done;
    int used = 0, count;
    while (used < RESPONSE_CAPACITY) {
        count = esp_http_client_read(client, body + used, RESPONSE_CAPACITY - used);
        if (count < 0) goto done;
        if (count == 0) break;
        used += count;
    }
    if (used == RESPONSE_CAPACITY || !esp_http_client_is_complete_data_received(client)) goto done;
    body[used] = 0;
    json = cJSON_ParseWithOpts(body, NULL, true);
done:
    free(body);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return json;
}

static void location_worker(void *unused)
{
    (void)unused;
    tt_location_request_t request;
    for (;;) {
        if (!xQueueReceive(location_queue,&request,portMAX_DELAY)) continue;
        tt_location_view_t result={.id=request.id,.kind=request.kind,.state=TT_LOCATION_FAILED};
        if (!(xEventGroupGetBits(wifi_events)&CONNECTED)) result.state=TT_LOCATION_OFFLINE;
        else {
            char url[512];
            if (tt_location_url(&request,url,sizeof(url))) {
                cJSON *json=get_json(url);
                char *body=json?cJSON_PrintUnformatted(json):NULL;
                tt_location_parse(body,&result);
                cJSON_free(body); cJSON_Delete(json);
            }
        }
        xSemaphoreTake(view_lock,portMAX_DELAY);
        if (view.lookup.id==request.id) view.lookup=result;
        xSemaphoreGive(view_lock);
    }
}

static void url_encode(const char *input, char *out)
{
    static const char hex[] = "0123456789ABCDEF";
    while (*input) {
        unsigned char c = (unsigned char)*input++;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') *out++ = (char)c;
        else { *out++ = '%'; *out++ = hex[c >> 4]; *out++ = hex[c & 15]; }
    }
    *out = 0;
}

static bool number(cJSON *object, const char *key, double low, double high, double *out)
{
    cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsNumber(value) || !isfinite(value->valuedouble) ||
        value->valuedouble < low || value->valuedouble > high) return false;
    *out = value->valuedouble;
    return true;
}

static bool fetch_weather(const char *city, bool *located, double *lat, double *lon, char *location,
                          uint32_t revision)
{
    char url[512], encoded[240];
    if (!*located) {
        url_encode(city, encoded);
        snprintf(url, sizeof(url),
                 "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=2&language=en&format=json",
                 encoded);
        cJSON *json = get_json(url);
        if (!json) { message("City lookup unavailable; retrying", revision); return false; }
        cJSON *results = cJSON_GetObjectItemCaseSensitive(json, "results");
        int count = cJSON_GetArraySize(results);
        cJSON *item = cJSON_GetArrayItem(results, 0);
        cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
        bool valid = count == 1 && cJSON_IsString(name) &&
            number(item, "latitude", -90, 90, lat) && number(item, "longitude", -180, 180, lon);
        if (valid) snprintf(location, 80, "%s", name->valuestring);
        cJSON_Delete(json);
        if (!valid) {
            message(count > 1 ? "City ambiguous: add country/region" : "City not found; check Settings", revision);
            return false;
        }
        *located = true;
    }
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.6f&longitude=%.6f"
             "&current=temperature_2m,relative_humidity_2m,weather_code&timeformat=unixtime",
             *lat, *lon);
    cJSON *json = get_json(url);
    if (!json) { message("Weather unavailable; keeping last update", revision); return false; }
    cJSON *current = cJSON_GetObjectItemCaseSensitive(json, "current");
    double temperature, humidity, code, timestamp;
    bool valid = number(current, "temperature_2m", -100, 80, &temperature) &&
        number(current, "relative_humidity_2m", 0, 100, &humidity) &&
        number(current, "weather_code", 0, 99, &code) &&
        number(current, "time", 1700000000, 4102444800.0, &timestamp);
    if (valid) {
        xSemaphoreTake(view_lock, portMAX_DELAY);
        if (revision != requested_revision) {
            xSemaphoreGive(view_lock);
            cJSON_Delete(json);
            return false;
        }
        view.temperature = (float)temperature;
        view.humidity = (int)humidity;
        view.weather_code = (int)code;
        view.updated = (time_t)timestamp;
        view.weather_valid = true;
        snprintf(view.location, sizeof(view.location), "%s", location);
        snprintf(view.message, sizeof(view.message), "Open-Meteo");
        xSemaphoreGive(view_lock);
    } else message("Invalid weather response; keeping last update", revision);
    cJSON_Delete(json);
    return valid;
}

static void worker(void *unused)
{
    (void)unused;
    tt_settings_t active={0}; service_request_t request;
    uint32_t revision=0;
    bool started=false, sntp_started=false, trying=false,auth_blocked=false;
    unsigned retry_ms=30000;
    int64_t next_connect=0, connect_deadline=0, scan_deadline=0;
    const char *failure="Connection timed out; please retry";
    for (;;) {
        if (xQueueReceive(config_queue,&request,pdMS_TO_TICKS(100))) {
            revision=request.revision; auth_blocked=false; retry_ms=30000;
            bool changed=strcmp(active.ssid,request.settings.ssid) || strcmp(active.password,request.settings.password);
            active=request.settings;
            /* An explicit retry must also restart an unchanged failed password. */
            if (changed || !started || !(xEventGroupGetBits(wifi_events)&CONNECTED)) {
                if (started) (void)esp_wifi_stop();
                xEventGroupClearBits(wifi_events,CONNECTED);
                wifi_config_t wifi={0};
                memcpy(wifi.sta.ssid,active.ssid,strlen(active.ssid));
                memcpy(wifi.sta.password,active.password,strlen(active.password));
                /* Starting STA with no credentials allows first-boot scans. */
                esp_err_t err=active.ssid[0]?esp_wifi_set_config(WIFI_IF_STA,&wifi):ESP_OK;
                started=err==ESP_OK && esp_wifi_start()==ESP_OK;
                trying=false; next_connect=0; failure="Connection timed out; please retry";
                xSemaphoreTake(view_lock,portMAX_DELAY);
                disconnect_reason=0;
                if (view.scan_state==TT_SCAN_RUNNING) { view.scan_state=TT_SCAN_FAILED; ++view.scan_generation; }
                xSemaphoreGive(view_lock);
            }
        }
        int64_t now=esp_timer_get_time()/1000;
        bool connected=(xEventGroupGetBits(wifi_events)&CONNECTED)!=0;
        xSemaphoreTake(view_lock,portMAX_DELAY);
        bool scan=scan_requested; scan_requested=false;
        bool scanning=view.scan_state==TT_SCAN_RUNNING;
        int reason=disconnect_reason;
        xSemaphoreGive(view_lock);
        if (scan && !scanning) {
            if (trying && !connected) { (void)esp_wifi_disconnect(); trying=false; }
            xSemaphoreTake(view_lock,portMAX_DELAY);
            view.scan_state=TT_SCAN_RUNNING; ++view.scan_generation;
            xSemaphoreGive(view_lock);
            if (!started || esp_wifi_scan_start(NULL,false)!=ESP_OK) scan_done(false);
            else { scanning=true; scan_deadline=now+15000; }
        }
        if (scanning && now>=scan_deadline) {
            (void)esp_wifi_scan_stop(); scan_done(false); scanning=false;
        }
        const char *net_message="Select a network below";
        tt_net_state_t state=TT_NET_IDLE;
        if (!started) { state=TT_NET_FAILED; net_message="Wi-Fi could not start"; }
        else if (connected) { state=TT_NET_CONNECTED; trying=false; retry_ms=30000; auth_blocked=false; net_message="Connected"; }
        else if (active.ssid[0] && !scanning) {
            bool auth=reason==WIFI_REASON_AUTH_FAIL || reason==WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || reason==WIFI_REASON_HANDSHAKE_TIMEOUT;
            bool absent=reason==WIFI_REASON_NO_AP_FOUND;
            if (trying && (auth || absent || now>=connect_deadline)) {
                failure=auth?"Password rejected; please retry":absent?"Network not found; refresh the list":"Connection timed out; please retry";
                trying=false; auth_blocked=auth; next_connect=now+retry_ms;
                if (retry_ms<300000) retry_ms=retry_ms>150000?300000:retry_ms*2;
                (void)esp_wifi_disconnect();
            }
            if (!trying && !auth_blocked && now>=next_connect) {
                xSemaphoreTake(view_lock,portMAX_DELAY); disconnect_reason=0; xSemaphoreGive(view_lock);
                trying=esp_wifi_connect()==ESP_OK; connect_deadline=now+20000; next_connect=now+30000;
            }
            state=trying?TT_NET_CONNECTING:TT_NET_FAILED;
            net_message=trying?"Connecting...":failure;
        }
        xSemaphoreTake(view_lock,portMAX_DELAY);
        if (revision==requested_revision) {
            view.connected=connected; view.network_state=state;
            snprintf(view.ssid,sizeof(view.ssid),"%s",active.ssid);
            snprintf(view.network_message,sizeof(view.network_message),"%s",net_message);
        }
        xSemaphoreGive(view_lock);
        if (connected && !sntp_started) {
            esp_sntp_config_t ntp=ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
            sntp_started=esp_netif_sntp_init(&ntp)==ESP_OK;
        }
    }
}
/* HTTPS may take seconds. Keeping it off the Wi-Fi task makes scan/connect
 * responsive even while a weather server is unavailable. */
static void weather_worker(void *unused)
{
    (void)unused;
    uint32_t revision=UINT32_MAX; bool located=false;
    double latitude=0,longitude=0; char location[80]="";
    int64_t next_weather=0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(500));
        xSemaphoreTake(view_lock,portMAX_DELAY);
        tt_settings_t settings=desired; uint32_t current=requested_revision;
        bool connected=view.connected;
        xSemaphoreGive(view_lock);
        if (revision!=current) { revision=current; located=false; next_weather=0; }
        if (!connected) { message("Set Wi-Fi in Settings",revision); continue; }
        if (time(NULL)<1700000000) { message("Waiting for time sync",revision); continue; }
        if (settings.precise_location) {
            latitude=settings.latitude_e6/1000000.0; longitude=settings.longitude_e6/1000000.0;
            snprintf(location,sizeof(location),"%s",settings.city[0]?settings.city:"Selected location");
            located=true;
        } else if (!settings.city[0]) { message("Set a weather city in Settings",revision); continue; }
        if (esp_timer_get_time()/1000>=next_weather) {
            bool success=fetch_weather(settings.city,&located,&latitude,&longitude,location,revision);
            next_weather=esp_timer_get_time()/1000+(success?900000:60000);
        }
    }
}

esp_err_t tt_services_start(const tt_settings_t *settings)
{
    view_lock = xSemaphoreCreateMutex();
    config_queue = xQueueCreate(1, sizeof(service_request_t));
    location_queue = xQueueCreate(1, sizeof(tt_location_request_t));
    wifi_events = xEventGroupCreate();
    if (!view_lock || !config_queue || !location_queue || !wifi_events) return ESP_ERR_NO_MEM;
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    if (!esp_netif_create_default_wifi_sta()) return ESP_ERR_NO_MEM;
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    if ((err = esp_wifi_init(&config)) != ESP_OK) return err;
    if ((err = esp_wifi_set_storage(WIFI_STORAGE_RAM)) != ESP_OK) return err;
    if ((err = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK) return err;
    if ((err = esp_wifi_set_ps(WIFI_PS_MIN_MODEM)) != ESP_OK) return err;
    if ((err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL)) != ESP_OK) return err;
    if ((err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL)) != ESP_OK) return err;
    if (xTaskCreate(worker,"time_wifi",6144,NULL,3,NULL)!=pdPASS) return ESP_ERR_NO_MEM;
    if (xTaskCreate(location_worker,"time_location",8192,NULL,1,NULL)!=pdPASS) return ESP_ERR_NO_MEM;
    ready=true;
    tt_services_configure(settings);
    return xTaskCreate(weather_worker,"time_weather",8192,NULL,1,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM;
}
void tt_services_locate(const tt_location_request_t *request)
{
    if (!view_lock || !location_queue) return;
    xSemaphoreTake(view_lock,portMAX_DELAY);
    view.lookup=(tt_location_view_t){.id=request->id,.kind=request->kind,.state=TT_LOCATION_LOADING};
    xQueueOverwrite(location_queue,request);
    xSemaphoreGive(view_lock);
}
void tt_services_configure(const tt_settings_t *settings)
{
    if (!config_queue || !view_lock) return;
    xSemaphoreTake(view_lock, portMAX_DELAY);
    /* Display/pet/sound changes must not discard a good weather cache or
     * create an extra HTTPS lookup. A failed network still permits a retry. */
    if (view.connected && !strcmp(desired.ssid,settings->ssid) &&
        !strcmp(desired.password,settings->password) && !strcmp(desired.city,settings->city) &&
        desired.precise_location==settings->precise_location &&
        desired.latitude_e6==settings->latitude_e6 && desired.longitude_e6==settings->longitude_e6) {
        desired=*settings; xSemaphoreGive(view_lock); return;
    }
    if (!ready) {
        view.network_state=TT_NET_FAILED;
        snprintf(view.ssid,sizeof(view.ssid),"%s",settings->ssid);
        snprintf(view.network_message,sizeof(view.network_message),"Wi-Fi could not start");
        xSemaphoreGive(view_lock); return;
    }
    service_request_t request = {.settings = *settings, .revision = ++requested_revision};
    desired=*settings;
    view.network_state=TT_NET_CONNECTING;
    snprintf(view.network_message,sizeof(view.network_message),"Connecting...");
    view.weather_valid = false;
    view.connected = false;
    view.ssid[0] = 0;
    snprintf(view.message, sizeof(view.message), "Applying settings");
    xQueueOverwrite(config_queue, &request);
    xSemaphoreGive(view_lock);
}
void tt_services_scan(void)
{
    if (!view_lock) return;
    xSemaphoreTake(view_lock,portMAX_DELAY);
    if (ready) scan_requested=true;
    else { view.scan_state=TT_SCAN_FAILED; ++view.scan_generation; }
    xSemaphoreGive(view_lock);
}
void tt_services_snapshot(tt_services_view_t *snapshot)
{
    if (!view_lock) {
        *snapshot=(tt_services_view_t){.scan_state=TT_SCAN_FAILED,.network_state=TT_NET_FAILED,
            .network_message="Wi-Fi could not start",.message="Network unavailable; offline timers still work"};
        return;
    }
    xSemaphoreTake(view_lock, portMAX_DELAY);
    *snapshot = view;
    if (!ready) {
        snapshot->scan_state=TT_SCAN_FAILED; snapshot->network_state=TT_NET_FAILED;
        snprintf(snapshot->network_message,sizeof(snapshot->network_message),"Wi-Fi could not start");
    }
    xSemaphoreGive(view_lock);
}
