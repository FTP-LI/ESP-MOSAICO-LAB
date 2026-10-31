#pragma once
#include "tusb.h"
#include "esp_err.h"
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_NOT_SUPPORTED 2
typedef enum { TINYUSB_EVENT_ATTACHED, TINYUSB_EVENT_DETACHED } tinyusb_event_id_t;
typedef struct { tinyusb_event_id_t id;uint8_t rhport; } tinyusb_event_t;
typedef void (*tinyusb_event_cb_t)(tinyusb_event_t *,void *);
typedef struct {
    struct { int xCoreID,priority; } task;
    struct {
        const tusb_desc_device_t *device;
        const uint8_t *full_speed_config,*high_speed_config;
    } descriptor;
    tinyusb_event_cb_t event_cb;
    void *event_arg;
} tinyusb_config_t;
