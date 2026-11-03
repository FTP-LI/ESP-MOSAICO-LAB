// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>
#include <stdbool.h>
#define TL_GLOBAL_COUNT 12
typedef struct { const char *name,*region,*symbol; uint8_t provider; } tl_index_spec_t;
typedef struct { int32_t price,previous; int64_t time; uint8_t source; } tl_index_quote_t; /* points * 100 */
extern const tl_index_spec_t tl_indices[TL_GLOBAL_COUNT];
int tl_global_tencent(const char *body,tl_index_quote_t out[TL_GLOBAL_COUNT]);
bool tl_global_valid(const tl_index_quote_t *q,int64_t now);
