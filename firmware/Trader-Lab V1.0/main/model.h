// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define TL_MAX_WATCH 64
#define TL_HISTORY 60
#define TL_CONFIG_MAGIC 0x544c0201u
typedef struct {
    char code[7];
    int32_t threshold; /* CNY * 1000, exact ETF tick units. */
    uint8_t below, enabled, fired;
} tl_watch_t;
typedef struct {
    uint32_t magic;
    char ssid[33], password[65];
    uint8_t count, brightness, light, volume, vibration;
    tl_watch_t watch[TL_MAX_WATCH];
} tl_config_t;
typedef struct {
    uint32_t magic;
    char ssid[33], password[65];
    uint8_t count, brightness, light, volume, vibration;
    tl_watch_t watch[12];
} tl_config_v1_t;
typedef struct { const char *code,*name; } tl_seed_t;
extern const tl_seed_t tl_seed[];
extern const unsigned tl_seed_count;
const char *tl_seed_name(const char *code);
bool tl_migrate_v1(const tl_config_v1_t *old,tl_config_t *config);
typedef struct {
    char code[7], name[64];
    int32_t price, previous;
    int64_t time;
    uint8_t source; /* 1 Eastmoney, 2 Tencent. Zero means unavailable. */
} tl_quote_t;
typedef struct {
    int32_t last_price;
    int64_t last_time;
    uint8_t source;
    bool baseline;
} tl_crossing_t;
typedef enum { TL_NO_DATA, TL_CLOCK_UNKNOWN, TL_CALENDAR_UNKNOWN,
    TL_CONNECTION_FAILED, TL_STALE, TL_CLOSED, TL_CURRENT } tl_quality_t;
void tl_defaults(tl_config_t *config);
bool tl_config_valid(const tl_config_t *config);
bool tl_code_valid(const char *code);
bool tl_price_parse(const char *text, int32_t *mills);
void tl_price_text(int32_t mills, char *out, size_t size);
bool tl_quote_valid(const tl_quote_t *quote);
int tl_parse_eastmoney(const char *body, tl_quote_t *out, int capacity);
int tl_parse_tencent(const char *body, tl_quote_t *out, int capacity);
int64_t tl_timestamp(const char *yyyymmddhhmmss);
int tl_market_session(int64_t now); /* -1 unknown calendar, 0 closed, 1 open */
tl_quality_t tl_quality(const tl_quote_t *q, int64_t now, bool clock_ok, bool fetched_ok);
const char *tl_quality_text(tl_quality_t quality);
const char *tl_source_text(unsigned source);
bool tl_crossing(tl_crossing_t *state, const tl_watch_t *rule, const tl_quote_t *quote, bool eligible);
void tl_gbk_utf8(const char *bytes, size_t length, char *out, size_t capacity);
