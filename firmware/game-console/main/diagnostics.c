// SPDX-License-Identifier: GPL-3.0-only
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include "bsp/esp_mosaico.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "diagnostics";
enum { GAME_KEYS = 9, AI_KEY = 9, BOOT_KEY = 10, KEY_COUNT = 11 };
static const char *const key_names[] = {
    "UP", "DOWN", "LEFT", "RIGHT", "A", "B", "START", "SELECT", "MENU", "AI", "BOOT"
};
static const gpio_num_t key_pins[] = {
    GPIO_NUM_55, GPIO_NUM_53, GPIO_NUM_19, GPIO_NUM_48, GPIO_NUM_18,
    GPIO_NUM_13, GPIO_NUM_17, GPIO_NUM_12, GPIO_NUM_16,
    BSP_BUTTON_AI_GPIO, BSP_BUTTON_BOOT_GPIO
};
static QueueHandle_t tone_queue, audio_result;
static lv_obj_t *key_tiles[GAME_KEYS], *status_label, *audio_label, *color_tiles[5];
static unsigned pattern, touch_count;
static bool display_ready;

static void request_tone(void)
{
    const uint8_t request = 1;
    xQueueOverwrite(tone_queue, &request);
}

static void audio_task(void *arg)
{
    (void)arg;
    esp_codec_dev_handle_t speaker = bsp_audio_codec_speaker_init();
    esp_codec_dev_sample_info_t fmt = {
        .sample_rate = BSP_AUDIO_DEFAULT_SAMPLE_RATE,
        .bits_per_sample = BSP_AUDIO_DEFAULT_BITS_PER_SAMPLE,
        .channel = BSP_AUDIO_DEFAULT_CHANNELS,
    };
    int result = speaker ? esp_codec_dev_open(speaker, &fmt) : ESP_FAIL;
    if (result == ESP_CODEC_DEV_OK) {
        result = esp_codec_dev_set_out_vol(speaker, 30);
    }
    bool ready = result == ESP_CODEC_DEV_OK;
    xQueueOverwrite(audio_result, &result);
    ESP_LOGI(TAG, "Speaker init: %d; AI or touch TONE plays 250 ms at 440 Hz", result);
    int16_t samples[240 * BSP_AUDIO_DEFAULT_CHANNELS];
    uint8_t request;
    while (true) {
        xQueueReceive(tone_queue, &request, portMAX_DELAY);
        if (!ready) {
            xQueueOverwrite(audio_result, &result);
            continue;
        }
        for (int block = 0; block < 50; ++block) {
            for (int frame = 0; frame < 240; ++frame) {
                int n = block * 240 + frame;
                float envelope = fminf(1.0f, fminf(n / 480.0f, (11999 - n) / 480.0f));
                int16_t v = (int16_t)(3000.0f * envelope * sinf(2.0f * 3.14159265f * 440 * n / 48000));
                samples[2 * frame] = samples[2 * frame + 1] = v;
            }
            result = esp_codec_dev_write(speaker, samples, sizeof(samples));
            if (result != ESP_CODEC_DEV_OK) {
                ready = false;
                break;
            }
        }
        xQueueOverwrite(audio_result, &result);
        ESP_LOGI(TAG, "Tone write result: %d (audibility requires physical verification)", result);
    }
}

static void update_pattern(void)
{
    static const uint32_t colors[] = {0xff0000, 0x00ff00, 0x0000ff, 0xffffff, 0x000000};
    for (int i = 0; i < 5; ++i) {
        lv_obj_set_style_bg_color(color_tiles[i], lv_color_hex(colors[pattern ? pattern - 1 : i]), 0);
    }
}

static void touch_action(lv_event_t *event)
{
    ++touch_count;
    if ((intptr_t)lv_event_get_user_data(event) == 0) {
        request_tone();
    } else {
        pattern = (pattern + 1) % 6;
        update_pattern();
    }
}

static lv_obj_t *label_at(lv_obj_t *parent, const char *text, int x, int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    return label;
}

static void create_screen(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);
    label_at(screen, "ESP-Mosaico V1.2 / Diagnostics", 20, 16);
    label_at(screen, "AI: tone   BOOT: colors   Touch: buttons", 20, 42);
    for (int i = 0; i < 5; ++i) {
        color_tiles[i] = lv_obj_create(screen);
        lv_obj_set_pos(color_tiles[i], 20 + i * 88, 72);
        lv_obj_set_size(color_tiles[i], 88, 72);
        lv_obj_set_style_radius(color_tiles[i], 0, 0);
        lv_obj_set_style_border_width(color_tiles[i], 1, 0);
        lv_obj_set_style_border_color(color_tiles[i], lv_color_white(), 0);
    }
    update_pattern();
    for (int i = 0; i < GAME_KEYS; ++i) {
        key_tiles[i] = lv_obj_create(screen);
        lv_obj_set_pos(key_tiles[i], 20 + (i % 3) * 148, 160 + (i / 3) * 54);
        lv_obj_set_size(key_tiles[i], 140, 46);
        lv_obj_set_style_bg_color(key_tiles[i], lv_color_hex(0x263442), 0);
        lv_obj_t *label = lv_label_create(key_tiles[i]);
        lv_label_set_text(label, key_names[i]);
        lv_obj_center(label);
    }
    audio_label = label_at(screen, "Audio: initializing", 20, 326);
    status_label = label_at(screen, "Inputs: initializing", 20, 350);
    for (int i = 0; i < 2; ++i) {
        lv_obj_t *button = lv_button_create(screen);
        lv_obj_set_pos(button, 20 + i * 224, 414);
        lv_obj_set_size(button, 216, 46);
        lv_obj_add_event_cb(button, touch_action, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text(label, i ? "COLORS" : "TONE (250 ms)");
        lv_obj_center(label);
    }
}

void app_main(void)
{
    bsp_board_variant_t variant;
    esp_err_t err = bsp_board_variant_get(&variant);
    if (err != ESP_OK || variant != BSP_BOARD_VARIANT_V1_2) {
        ESP_LOGE(TAG, "V1.2 eFuse identification required: err=%s. No pin override applied.", esp_err_to_name(err));
        return;
    }
    tone_queue = xQueueCreate(1, sizeof(uint8_t));
    audio_result = xQueueCreate(1, sizeof(int));
    if (!tone_queue || !audio_result) {
        ESP_LOGE(TAG, "Cannot allocate diagnostic queues");
        return;
    }
    display_ready = bsp_display_start() != NULL;
    if (display_ready && bsp_display_lock(1000)) {
        create_screen();
        bsp_display_unlock();
        ESP_ERROR_CHECK(bsp_display_brightness_set(50));
    } else {
        display_ready = false;
        ESP_LOGE(TAG, "Display unavailable; continue serial input/audio diagnostics");
    }
    int first = AI_KEY;
#if CONFIG_DIAG_GAMEPAD_ENABLE
    first = 0;
#endif
    uint64_t mask = 0;
    for (int i = first; i < KEY_COUNT; ++i) mask |= 1ULL << key_pins[i];
    gpio_config_t cfg = {.pin_bit_mask = mask, .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&cfg));
    if (xTaskCreate(audio_task, "diag_audio", 6144, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Cannot create audio task");
        const int failure = ESP_ERR_NO_MEM;
        xQueueOverwrite(audio_result, &failure);
    }
    ESP_LOGI(TAG, "INPUT mode=%s, sample=5ms debounce=20ms; POWER is not a game key", first ? "onboard-only" : "H2 rev-0.1");
    bool raw[KEY_COUNT] = {0}, stable[KEY_COUNT] = {0};
    int64_t changed_at[KEY_COUNT] = {0};
    int64_t last_ui = 0, last_log = 0;
    bool next_pattern = false;
    TickType_t wake = xTaskGetTickCount();
    while (true) {
        int64_t now = esp_timer_get_time();
        unsigned bits = 0;
        for (int i = first; i < KEY_COUNT; ++i) {
            bool down = gpio_get_level(key_pins[i]) == 0;
            if (down != raw[i]) {
                raw[i] = down;
                changed_at[i] = now;
            }
            if (raw[i] != stable[i] && now - changed_at[i] >= 20000) {
                stable[i] = raw[i];
                ESP_LOGI(TAG, "KEY %-6s %s", key_names[i], stable[i] ? "DOWN" : "UP");
                if (stable[i] && (i == AI_KEY || i == 4)) request_tone();
                if (stable[i] && (i == BOOT_KEY || i == 8)) next_pattern = true;
            }
            if (stable[i]) bits |= 1U << i;
        }
        if (display_ready && (next_pattern || now - last_ui >= 50000) && bsp_display_lock(10)) {
            if (next_pattern) {
                pattern = (pattern + 1) % 6;
                update_pattern();
                next_pattern = false;
            }
            for (int i = 0; i < GAME_KEYS; ++i) {
                lv_obj_set_style_bg_color(key_tiles[i], lv_color_hex(stable[i] ? 0x18a558 : 0x263442), 0);
            }
            lv_label_set_text_fmt(status_label, "%s | bits=%03X | AI=%d BOOT=%d\nUp %lus | heap %lu KiB | touch %u",
                first ? "H2 OFF" : "H2 ON", bits & 0x1ff, stable[AI_KEY], stable[BOOT_KEY],
                (unsigned long)(now / 1000000), (unsigned long)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024), touch_count);
            int result;
            if (xQueueReceive(audio_result, &result, 0)) {
                lv_label_set_text_fmt(audio_label, "Audio API: %s (%d) / verify by listening", result == ESP_CODEC_DEV_OK ? "OK" : "ERROR", result);
            }
            bsp_display_unlock();
            last_ui = now;
        }
        if (now - last_log >= 5000000) {
            ESP_LOGI(TAG, "uptime=%lus keys=0x%03X heap=%lu display=%s", (unsigned long)(now / 1000000), bits,
                (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), display_ready ? "started" : "failed");
            last_log = now;
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(5));
    }
}
