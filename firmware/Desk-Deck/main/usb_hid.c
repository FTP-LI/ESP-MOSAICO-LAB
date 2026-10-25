// SPDX-License-Identifier: GPL-3.0-only
#include "usb_hid.h"
#include <stdatomic.h>
#include <string.h>
#include "tinyusb.h"
#include "tusb.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

enum { REPORT_KEYBOARD = 1, REPORT_CONSUMER = 2, HID_INTERFACE = 4, HID_EP = 0x85 };
static const uint8_t report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_KEYBOARD)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(REPORT_CONSUMER))
};
_Static_assert(CFG_TUD_CDC == 2 && CFG_TUD_HID == 1, "Desk-Deck needs two CDCs and one HID");
static uint8_t fs_descriptor[256], hs_descriptor[256];
static tusb_desc_device_t device_descriptor;
static QueueHandle_t actions;
static atomic_bool ready;
static atomic_uint completed, dropped, reset_generation;

/* Preserve Iris' descriptor ownership, callbacks, CDC endpoints and shutdown.
 * The linker wrapper extends the supplied configuration before its only install.
 * Any upstream layout change fails explicitly instead of colliding with an EP. */
static bool extend_config(const uint8_t *source, uint8_t *target, uint8_t interval)
{
    if (!source || source[0] != TUD_CONFIG_DESC_LEN || source[1] != TUSB_DESC_CONFIGURATION || source[4] != 4) return false;
    size_t length = source[2] | ((size_t)source[3] << 8);
    if (length != TUD_CONFIG_DESC_LEN + 2 * TUD_CDC_DESC_LEN || length + TUD_HID_DESC_LEN > sizeof(fs_descriptor)) return false;
    for (size_t at = TUD_CONFIG_DESC_LEN; at < length;) {
        if (source[at] < 2 || at + source[at] > length) return false;
        if (source[at + 1] == TUSB_DESC_ENDPOINT && source[at + 2] == HID_EP) return false;
        at += source[at];
    }
    const uint8_t hid[] = {TUD_HID_DESCRIPTOR(HID_INTERFACE, 0, HID_ITF_PROTOCOL_NONE,
        sizeof(report_descriptor), HID_EP, 16, interval)};
    memcpy(target, source, length);
    memcpy(target + length, hid, sizeof(hid));
    length += sizeof(hid);
    target[2] = length & 0xff; target[3] = length >> 8; target[4] = 5;
    return true;
}

esp_err_t __real_tinyusb_driver_install(const tinyusb_config_t *config);
esp_err_t __wrap_tinyusb_driver_install(const tinyusb_config_t *config)
{
    if (!config || !config->descriptor.device ||
        !extend_config(config->descriptor.full_speed_config, fs_descriptor, 5)) return ESP_ERR_NOT_SUPPORTED;
    tinyusb_config_t composite = *config;
    device_descriptor = *config->descriptor.device;
    device_descriptor.bcdDevice = 0x0201;
    composite.descriptor.device = &device_descriptor;
    composite.descriptor.full_speed_config = fs_descriptor;
    if (config->descriptor.high_speed_config) {
        if (!extend_config(config->descriptor.high_speed_config, hs_descriptor, 7)) return ESP_ERR_NOT_SUPPORTED;
        composite.descriptor.high_speed_config = hs_descriptor;
    }
    ESP_LOGI("deck_usb", "composite: CDC interfaces 0-3 + HID interface 4, IN endpoint 0x85");
    return __real_tinyusb_driver_install(&composite);
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    return instance == 0 ? report_descriptor : NULL;
}
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type, uint8_t *buffer, uint16_t size)
{
    if (instance != 0 || type != HID_REPORT_TYPE_INPUT) return 0;
    uint16_t count = report_id == REPORT_KEYBOARD ? 8 : report_id == REPORT_CONSUMER ? 2 : 0;
    if (size < count) return 0;
    memset(buffer, 0, count); return count;
}
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type, uint8_t const *buffer, uint16_t size)
{
    (void)instance; (void)report_id; (void)type; (void)buffer; (void)size;
}

bool deck_hid_ready(void) { return atomic_load(&ready); }
unsigned deck_hid_completed(void) { return atomic_load(&completed); }
unsigned deck_hid_dropped(void) { return atomic_load(&dropped); }
void deck_hid_cancel_pending(void) { atomic_fetch_add(&reset_generation, 1); }
bool deck_hid_send(deck_hid_action_t action)
{
    if (!actions || !deck_hid_ready() || !tud_mounted() || tud_suspended() || (!action.key && !action.consumer)) return false;
    if (xQueueSend(actions, &action, 0) != pdTRUE) { atomic_fetch_add(&dropped, 1); return false; }
    return true;
}

static bool submit(deck_hid_action_t action, bool release)
{
    if (action.consumer) {
        uint16_t value = release ? 0 : action.consumer;
        return tud_hid_report(REPORT_CONSUMER, &value, sizeof(value));
    }
    uint8_t keys[6] = {release ? 0 : action.key};
    return tud_hid_keyboard_report(REPORT_KEYBOARD, release ? 0 : action.modifier, keys);
}

static void hid_worker(void *unused)
{
    (void)unused;
    deck_hid_action_t current = {0};
    unsigned phase = 0, reset = 0;
    bool connected = false;
    int64_t pressed_at = 0;
    for (;;) {
        bool mounted = tud_mounted() && !tud_suspended();
        if (!mounted) {
            atomic_store(&ready, false);
            xQueueReset(actions); phase = 0; connected = false;
        } else {
            if (!connected) { xQueueReset(actions); connected = true; phase = 1; }
            unsigned generation = atomic_load(&reset_generation);
            if (generation != reset) {
                reset = generation; xQueueReset(actions);
                /* Never drop a release already owed to the host. */
            }
            if (tud_hid_ready()) {
                if (phase == 1) {
                    if (submit((deck_hid_action_t){0}, true)) phase = 2;
                } else if (phase == 2) {
                    if (submit((deck_hid_action_t){.consumer=1}, true)) phase = 3;
                } else if (phase == 3) {
                    atomic_store(&ready, true); phase = 0;
                } else if (phase == 4 && esp_timer_get_time() - pressed_at >= 25000) {
                    if (submit(current, true)) phase = 5;
                } else if (phase == 5) {
                    atomic_fetch_add(&completed, 1); phase = 0;
                } else if (phase == 0 && xQueueReceive(actions, &current, 0) == pdTRUE) {
                    if (submit(current, false)) { phase = 4; pressed_at = esp_timer_get_time(); }
                    else atomic_fetch_add(&dropped, 1);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
esp_err_t deck_hid_start(void)
{
    actions = xQueueCreate(4, sizeof(deck_hid_action_t));
    if (!actions) return ESP_ERR_NO_MEM;
    if (xTaskCreate(hid_worker, "deck_hid", 3072, NULL, 5, NULL) != pdPASS) {
        vQueueDelete(actions); actions = NULL; return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
