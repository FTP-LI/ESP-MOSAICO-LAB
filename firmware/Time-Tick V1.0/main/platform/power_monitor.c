// SPDX-License-Identifier: GPL-3.0-only
#include "power_monitor.h"
#include "bsp/esp_mosaico.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include <string.h>
#define TT_BATTERY_CAPACITY_MAH 68 /* Original cell, confirmed by owner 2026-10-21. */
static QueueHandle_t samples;
typedef struct { tt_power_view_t view; tt_power_usage_t usage; } power_sample_t;
static i2c_master_dev_handle_t gauge;
static esp_err_t word(uint8_t reg,uint16_t *value)
{
    uint8_t bytes[2];
    esp_err_t error=i2c_master_transmit_receive(gauge,&reg,1,bytes,2,30);
    esp_rom_delay_us(100); /* TI TRM 5.3: >=66 us between gauge packets. */
    if (error==ESP_OK) *value=(uint16_t)(bytes[0]|bytes[1]<<8);
    return error;
}
static esp_err_t write_bytes(const uint8_t *data,size_t size)
{
    esp_err_t error=i2c_master_transmit(gauge,data,size,30);
    esp_rom_delay_us(100); return error;
}
static esp_err_t control(uint16_t command)
{
    uint8_t packet[]={0x00,(uint8_t)command,(uint8_t)(command>>8)};
    return write_bytes(packet,sizeof(packet));
}
static esp_err_t wait_status(uint16_t mask,uint16_t expected)
{
    uint16_t value=0;
    for (unsigned i=0;i<25;++i) {
        esp_err_t error=word(0x3a,&value); if (error!=ESP_OK) return error;
        if ((value&mask)==expected) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGW("tt_power","status timeout: op=0x%04x mask=0x%04x expected=0x%04x",value,mask,expected);
    return ESP_ERR_TIMEOUT;
}
static esp_err_t memory_read(uint16_t address,uint8_t *data,size_t size)
{
    uint8_t select[]={0x3e,(uint8_t)address,(uint8_t)(address>>8)};
    esp_err_t error=write_bytes(select,sizeof(select)); if (error!=ESP_OK) return error;
    uint8_t reg=0x3e,bytes[6]; /* Only the two/four-byte fields below are used. */
    if (size>sizeof(bytes)-2) return ESP_ERR_INVALID_SIZE;
    for (unsigned i=0;i<10;++i) {
        vTaskDelay(pdMS_TO_TICKS(10));
        error=i2c_master_transmit_receive(gauge,&reg,1,bytes,size+2,30); esp_rom_delay_us(100);
        if (error!=ESP_OK) return error;
        if (bytes[0]==(uint8_t)address && bytes[1]==(uint8_t)(address>>8)) {
            memcpy(data,bytes+2,size); return ESP_OK;
        }
    }
    return ESP_ERR_TIMEOUT;
}
static esp_err_t memory_write(uint16_t address,const uint8_t *data,size_t size)
{
    if (size>4) return ESP_ERR_INVALID_SIZE;
    uint8_t packet[7]={0x3e,(uint8_t)address,(uint8_t)(address>>8)},sum=0;
    memcpy(packet+3,data,size);
    for (size_t i=1;i<size+3;++i) sum+=packet[i];
    esp_err_t error=write_bytes(packet,size+3); if (error!=ESP_OK) return error;
    vTaskDelay(pdMS_TO_TICKS(10));
    uint8_t commit[]={0x60,(uint8_t)(255-sum),(uint8_t)(size+4)};
    error=write_bytes(commit,sizeof(commit)); vTaskDelay(pdMS_TO_TICKS(10));
    return error;
}
static esp_err_t configure_capacity(void)
{
    /* TI SLUUBD4A section 6, RAM only. Do not copy an 80mAh cell's CEDV curve,
     * touch OTP, charger/protection settings or the current/voltage calibration.
     * Preserve learned FCC whenever DesignCapacity already matches the cell. */
    esp_err_t error=ESP_OK; bool entered=false,backup=false,changed=false;
    uint8_t old_capacity[4],old_near[2],actual[4];
    const uint8_t capacity[]={0,TT_BATTERY_CAPACITY_MAH,0,TT_BATTERY_CAPACITY_MAH};
    const uint8_t near_full[]={0,4}; /* ~6% of 68mAh; old 200mAh exceeds the cell. */
    uint16_t status,device;
#define TRY(call) do { error=(call); if (error!=ESP_OK) { ESP_LOGW("tt_power","setup step %s: %s",#call,esp_err_to_name(error)); goto cleanup; } } while (0)
    TRY(control(0x0001)); vTaskDelay(pdMS_TO_TICKS(20)); TRY(word(0x40,&device));
    if (device!=0x0220) return ESP_ERR_NOT_SUPPORTED;
    TRY(word(0x3a,&status));
    ESP_LOGI("tt_power","capacity setup: device=0x%04x op=0x%04x",device,status);
    if ((status&6)==6) {
        TRY(control(0x0414)); vTaskDelay(pdMS_TO_TICKS(10));
        TRY(control(0x3672)); vTaskDelay(pdMS_TO_TICKS(10));
        error=wait_status(6,4);
        if (error==ESP_ERR_TIMEOUT) {
            /* TI SLUUBD4A 3.3 lists the ROM key as 8000/8000, while
             * chapter 6 and the BSP use 0414/3672. Try only these documented
             * defaults, once, before giving up; never change stored keys. */
            TRY(control(0x8000)); vTaskDelay(pdMS_TO_TICKS(10));
            TRY(control(0x8000)); vTaskDelay(pdMS_TO_TICKS(10));
        } else if (error!=ESP_OK) goto cleanup;
        TRY(wait_status(6,4));
    }
    TRY(word(0x3a,&status));
    if ((status&6)!=2) {
        TRY(control(0xffff)); vTaskDelay(pdMS_TO_TICKS(10)); TRY(control(0xffff));
        vTaskDelay(pdMS_TO_TICKS(1000)); TRY(wait_status(6,2));
    }
    TRY(control(0x0090)); entered=true;
    vTaskDelay(pdMS_TO_TICKS(2000)); TRY(wait_status(0x0400,0x0400));
    TRY(memory_read(0x929d,old_capacity,4)); TRY(memory_read(0x926b,old_near,2)); backup=true;
    ESP_LOGI("tt_power","capacity RAM before: FCC=%u design=%u near_full=%u; target=68/68/4mAh",
        old_capacity[0]*256u+old_capacity[1],old_capacity[2]*256u+old_capacity[3],old_near[0]*256u+old_near[1]);
    changed=true; TRY(memory_write(0x929d,capacity,4)); TRY(memory_write(0x926b,near_full,2));
    TRY(memory_read(0x929d,actual,4));
    if (memcmp(actual,capacity,4)) { error=ESP_ERR_INVALID_RESPONSE; goto cleanup; }
    TRY(memory_read(0x926b,actual,2));
    if (memcmp(actual,near_full,2)) { error=ESP_ERR_INVALID_RESPONSE; goto cleanup; }
    TRY(control(0x0091)); vTaskDelay(pdMS_TO_TICKS(2000));
    TRY(wait_status(0x0400,0)); entered=false;
cleanup:
    if (entered) {
        if (error!=ESP_OK && backup && changed) {
            esp_err_t a=memory_write(0x929d,old_capacity,4),b=memory_write(0x926b,old_near,2);
            ESP_LOGW("tt_power","capacity rollback: %s / %s",esp_err_to_name(a),esp_err_to_name(b));
        }
        esp_err_t exit_error=control(0x0092); vTaskDelay(pdMS_TO_TICKS(2000));
        if (exit_error==ESP_OK) exit_error=wait_status(0x0400,0);
        if (exit_error!=ESP_OK) ESP_LOGE("tt_power","CONFIG UPDATE exit failed: %s",esp_err_to_name(exit_error));
    }
    esp_err_t seal=control(0x0030);
    vTaskDelay(pdMS_TO_TICKS(1000)); if (seal==ESP_OK) seal=wait_status(6,6);
    if (seal!=ESP_OK) { ESP_LOGE("tt_power","gauge seal failed: %s",esp_err_to_name(seal)); error=seal; }
    if (error==ESP_OK) {
        uint16_t design=0,full=0; error=word(0x3c,&design);
        if (error==ESP_OK) error=word(0x12,&full);
        if (error==ESP_OK && (design!=TT_BATTERY_CAPACITY_MAH || full!=TT_BATTERY_CAPACITY_MAH)) error=ESP_ERR_INVALID_RESPONSE;
        if (error==ESP_OK) ESP_LOGI("tt_power","capacity verified: design=%u full=%u mAh; sealed; cell curve not calibrated",design,full);
    }
#undef TRY
    return error;
}
static void worker(void *unused)
{
    (void)unused; unsigned failures=0,sample_count=0; bool attempted=false;
    tt_power_usage_t usage={0};
    for (;;) {
        uint16_t flags=0,soc=0,mv=0,ma=0,design=0;
        tt_power_view_t p={0};
        bool ok=word(0x0a,&flags)==ESP_OK && word(0x08,&mv)==ESP_OK &&
            word(0x0c,&ma)==ESP_OK && word(0x2c,&soc)==ESP_OK && word(0x3c,&design)==ESP_OK &&
            tt_power_decode(&p,flags,soc,mv,(int16_t)ma,(uint32_t)(esp_timer_get_time()/1000));
        if (ok) {
            if (design!=TT_BATTERY_CAPACITY_MAH && !attempted && p.state==TT_POWER_CHARGING && mv>=3500) {
                attempted=true;
                esp_err_t result=configure_capacity();
                ESP_LOGI("tt_power","68mAh capacity setup result: %s",esp_err_to_name(result));
                /* Old SOC/flags predate the gauge reinitialization. */
                vTaskDelay(pdMS_TO_TICKS(1000)); continue;
            }
            if (p.state!=TT_POWER_ABSENT && design!=TT_BATTERY_CAPACITY_MAH) {
                p.valid=false; /* Never present the known wrong 3000mAh SOC as charge. */
                if (sample_count%6==0) ESP_LOGW("tt_power","capacity=%umAh differs from 68mAh; SOC hidden until charging/configured",design);
            }
            failures=0;
            ESP_LOGI("tt_power","state=%d soc=%u voltage=%dmV current=%dmA flags=0x%04x",p.state,p.percent,p.voltage_mv,p.current_ma,flags);
            /* Read capacity estimates once a minute; not a runtime calibration. */
            if (sample_count++%6==0) {
                uint16_t remaining,full,design,average,temperature;
                if (word(0x10,&remaining)==ESP_OK && word(0x12,&full)==ESP_OK &&
                    word(0x3c,&design)==ESP_OK && word(0x14,&average)==ESP_OK && word(0x06,&temperature)==ESP_OK)
                    ESP_LOGI("tt_power","gauge remaining=%umAh full=%umAh design=%umAh avg_current=%dmA temperature_dK=%u",
                        remaining,full,design,(int16_t)average,temperature);
            }
        } else if (!failures++) ESP_LOGW("tt_power","Battery reading unavailable; no estimated percentage used");
        tt_power_usage_sample(&usage,&p);
        const power_sample_t sample={.view=p,.usage=usage};
        xQueueOverwrite(samples,&sample);
        vTaskDelay(pdMS_TO_TICKS(ok?10000:60000));
    }
}
esp_err_t tt_power_start(void)
{
    if (samples) return ESP_OK;
    esp_err_t error=bsp_i2c_init(); if (error!=ESP_OK) return error;
    const i2c_device_config_t config={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=BSP_BATTERY_I2C_ADDR,
        .scl_speed_hz=100000}; /* TI incremental multi-byte writes require <=100kHz. */
    error=i2c_master_bus_add_device(bsp_i2c_get_handle(),&config,&gauge); if (error!=ESP_OK) return error;
    /* Only the battery worker accesses this handle. RAM setup runs while charging
     * and preserves a matching gauge's learned capacity across MCU restarts. */
    samples=xQueueCreate(1,sizeof(power_sample_t));
    if (samples && xTaskCreate(worker,"tt_battery",3072,NULL,1,NULL)==pdPASS) return ESP_OK;
    if (samples) vQueueDelete(samples);
    samples=NULL;
    i2c_master_bus_rm_device(gauge); gauge=NULL; return ESP_ERR_NO_MEM;
}
void tt_power_snapshot(tt_power_view_t *p)
{
    *p=(tt_power_view_t){0};
    power_sample_t sample;
    if (samples && xQueuePeek(samples,&sample,0)==pdTRUE) *p=sample.view;
    if (p->valid && (uint32_t)(esp_timer_get_time()/1000)-p->sampled_ms>30000) p->valid=false;
}
void tt_power_usage_snapshot(tt_power_usage_t *usage)
{
    *usage=(tt_power_usage_t){0}; power_sample_t sample;
    if (samples && xQueuePeek(samples,&sample,0)==pdTRUE) *usage=sample.usage;
}
