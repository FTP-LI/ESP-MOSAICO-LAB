// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef enum { TT_PET_FEED, TT_PET_PLAY, TT_PET_CLEAN, TT_PET_REST, TT_PET_CUDDLE, TT_PET_ACTIONS } tt_pet_action_t;
typedef enum { TT_PET_OK, TT_PET_WAIT, TT_PET_FULL, TT_PET_TIRED, TT_PET_ASLEEP, TT_PET_CLEAN_ALREADY, TT_PET_SAVE_FAILED } tt_pet_result_t;
typedef struct {
    uint16_t food, joy, energy, hygiene, bond, reserved;
    uint32_t xp, seconds, meals, games, baths, cuddles;
    uint8_t resting, padding[3];
} tt_pet_state_t;
typedef struct { uint32_t sessions, seconds, longest, badges; } tt_focus_progress_t;
typedef struct { uint32_t schema; tt_pet_state_t pets[3]; tt_focus_progress_t focus; } tt_pet_book_t;
typedef struct { uint64_t elapsed_ms; int64_t last_ms, cooldown[3][TT_PET_ACTIONS]; bool started; } tt_pet_runtime_t;
void tt_pet_default(tt_pet_book_t *book);
bool tt_pet_decode(tt_pet_book_t *book, const void *data, size_t size);
bool tt_pet_valid(const tt_pet_book_t *book);
unsigned tt_pet_level(const tt_pet_state_t *pet);
const char *tt_pet_stage(const tt_pet_state_t *pet);
bool tt_pet_tick(tt_pet_book_t *book, tt_pet_runtime_t *runtime, unsigned active, int64_t now);
tt_pet_result_t tt_pet_act(tt_pet_book_t *book, tt_pet_runtime_t *runtime,
                          unsigned pet, tt_pet_action_t action, int64_t now);
const char *tt_pet_result_text(tt_pet_result_t result, tt_pet_action_t action, bool resting);
/* Only naturally completed sessions >=60 seconds earn focus rewards. */
uint32_t tt_pet_focus_complete(tt_pet_book_t *book,unsigned pet,uint32_t seconds);
unsigned tt_focus_badge_progress(const tt_focus_progress_t *progress,unsigned badge);
const char *tt_focus_badge_title(unsigned badge);
enum { TT_FOCUS_BADGES=6 };
