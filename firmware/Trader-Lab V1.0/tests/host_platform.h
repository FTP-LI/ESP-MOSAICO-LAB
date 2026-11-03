// Host UI validation only; never part of the firmware component.
#pragma once
#include "lvgl.h"
#include <stdint.h>
#include <time.h>
static inline lv_indev_t *bsp_display_get_input_dev(void) { return NULL; }
static inline int64_t esp_timer_get_time(void) { return (int64_t)clock()*1000000/CLOCKS_PER_SEC; }
#define ESP_LOGI(...) ((void)0)
