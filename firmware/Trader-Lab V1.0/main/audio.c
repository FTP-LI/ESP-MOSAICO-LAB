// SPDX-License-Identifier: GPL-3.0-only
// Codec setup adapted from Espressif onboard/audio.c, Copyright 2026 Espressif
// Systems (Shanghai) CO LTD, Apache-2.0; see docs/licenses/Apache-2.0.txt.
// Playback path and anti-click synthesis adapted from Time-Tick; see REFERENCE.md.
#include "audio.h"
#include "alarm_model.h"
#include "bsp/esp_mosaico.h"
#include "esp_codec_dev_defaults.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <stdatomic.h>
typedef struct { unsigned volume,generation; } request_t;
static QueueHandle_t requests;
static atomic_uint generation;
static atomic_bool failed;
static esp_codec_dev_handle_t speaker;
static i2s_chan_handle_t tx;
static bool init_attempted;
/* The stock BSP opens RX and TX together. This app owns a TX-only stream and
 * closes it after each cue, so an unused microphone DMA cannot stay awake.
 * Codec settings follow the pinned BSP's onboard/audio.c (Apache-2.0). */
static esp_codec_dev_handle_t create_speaker(void)
{
    if(bsp_i2c_init()!=ESP_OK || bsp_power_set_vcc_3v3(true)!=ESP_OK || bsp_power_set_codec_3v3(true)!=ESP_OK) return NULL;
    vTaskDelay(pdMS_TO_TICKS(10));
    i2s_chan_config_t channel=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);
    channel.auto_clear=true; channel.dma_desc_num=6; channel.dma_frame_num=240;
    if(i2s_new_channel(&channel,&tx,NULL)!=ESP_OK) return NULL;
    i2s_std_config_t format={.clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(48000),
        .slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO),
        .gpio_cfg={.mclk=BSP_AUDIO_I2S_MCLK,.bclk=BSP_AUDIO_I2S_SCLK,.ws=BSP_AUDIO_I2S_LRCLK,.dout=BSP_AUDIO_I2S_SDOUT,.din=GPIO_NUM_NC}};
    format.clk_cfg.mclk_multiple=I2S_MCLK_MULTIPLE_256;
    if(i2s_channel_init_std_mode(tx,&format)!=ESP_OK || i2s_channel_enable(tx)!=ESP_OK) return NULL;
    audio_codec_i2s_cfg_t data_config={.port=I2S_NUM_0,.tx_handle=tx};
    const audio_codec_data_if_t *data=audio_codec_new_i2s_data(&data_config);
    const audio_codec_gpio_if_t *gpio=audio_codec_new_gpio();
    audio_codec_i2c_cfg_t control_config={.addr=BSP_AUDIO_CODEC_I2C_ADDR,.bus_handle=bsp_i2c_get_handle(),.clock_speed_hz=0};
    const audio_codec_ctrl_if_t *control=audio_codec_new_i2c_ctrl(&control_config);
    if(!data || !gpio || !control) return NULL;
    es8311_codec_cfg_t codec_config={.ctrl_if=control,.gpio_if=gpio,.codec_mode=ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin=BSP_AUDIO_PA_CTRL,.master_mode=false,.use_mclk=true,.digital_mic=false,
        .hw_gain={.pa_voltage=5.0f,.codec_dac_voltage=3.3f},.no_dac_ref=true};
    const audio_codec_if_t *codec=es8311_codec_new(&codec_config); if(!codec) return NULL;
    esp_codec_dev_cfg_t device_config={.dev_type=ESP_CODEC_DEV_TYPE_OUT,.codec_if=codec,.data_if=data};
    return esp_codec_dev_new(&device_config);
}
static void idle(void)
{
    if(tx) { i2s_chan_info_t info; if(i2s_channel_get_info(tx,&info)==ESP_OK && info.is_enabled) (void)i2s_channel_disable(tx); }
}
static bool current(request_t r) { return r.generation==atomic_load(&generation); }
static void worker(void *arg)
{
    (void)arg; request_t r;
    esp_codec_dev_sample_info_t format={.sample_rate=48000,.bits_per_sample=16,.channel=2};
    for(;;) {
        if(!xQueueReceive(requests,&r,portMAX_DELAY) || !current(r)) continue;
        if(!init_attempted) { init_attempted=true; speaker=create_speaker(); }
        bool ok=speaker && esp_codec_dev_open(speaker,&format)==ESP_CODEC_DEV_OK;
        if(ok) ok=esp_codec_dev_set_out_vol(speaker,(int)tt_alarm_codec_volume(r.volume))==ESP_CODEC_DEV_OK;
        unsigned count; const tt_alarm_note_t *notes=tt_alarm_notes(TT_RING_MELODY,&count);
        int16_t pcm[960];
        for(unsigned n=0;ok && current(r) && n<count;n++) {
            unsigned total=notes[n].duration_ms*48,frames=total+notes[n].gap_ms*48;
            if(n==count-1) frames+=1920; /* Drain the final DMA buffers before closing PA. */
            for(unsigned at=0;ok && current(r) && at<frames;at+=480) {
                for(unsigned i=0;i<480;i++) pcm[i*2]=pcm[i*2+1]=tt_alarm_sample(TT_RING_MELODY,notes[n].hz,at+i,total);
                ok=esp_codec_dev_write(speaker,pcm,sizeof(pcm))==ESP_CODEC_DEV_OK;
            }
        }
        if(speaker) (void)esp_codec_dev_close(speaker);
        idle(); atomic_store(&failed,!ok);
        ESP_LOGI("tl_audio","playback=%s",ok?"ok":"failed");
    }
}
bool tl_audio_start(void)
{
    requests=xQueueCreate(1,sizeof(request_t));
    return requests && xTaskCreate(worker,"tl_audio",8192,NULL,3,NULL)==pdPASS;
}
void tl_audio_stop(void) { atomic_fetch_add(&generation,1); if(requests) xQueueReset(requests); }
void tl_audio_play(unsigned volume)
{
    if(!requests || !volume) return;
    request_t r={volume>80?80:volume,atomic_load(&generation)}; (void)xQueueOverwrite(requests,&r);
}
bool tl_audio_error(void) { return atomic_load(&failed); }
