// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { TT_HOME, TT_RIGHT, TT_INVERTED, TT_LEFT, TT_FRONT, TT_BACK, TT_UNKNOWN } tt_face_t;
typedef enum { TT_HOME_RESTART, TT_HOME_CONTINUE, TT_HOME_RESUME } tt_policy_t;
typedef enum { TT_IDLE, TT_RUNNING, TT_PAUSED, TT_FINISHED } tt_state_t;

typedef struct {
    uint32_t seconds[6]; /* HOME is a clock; FRONT/BACK retain disabled slots for storage compatibility. */
    int8_t home_axis, right_axis; /* Signed accelerometer axes: +/-1, +/-2, +/-3. */
    bool calibrated, reverse_rotation;
    tt_policy_t policy;
} tt_config_t;

typedef struct {
    float filtered[3];
    bool initialized, settled;
    int8_t candidate, stable;
    int64_t candidate_since, last_sample;
} tt_orientation_t;

typedef struct {
    tt_state_t state;
    tt_face_t active, observed;
    int64_t deadline_ms, remaining_ms;
    uint32_t duration_s;
} tt_timer_t;

tt_config_t tt_config_default(void);
bool tt_config_valid(const tt_config_t *config);
void tt_orientation_sample(tt_orientation_t *state, const float accel[3],
                           const float gyro[3], bool valid, int64_t now_ms);
tt_face_t tt_face_from_axis(const tt_config_t *config, int axis);
int tt_rotation(tt_face_t face, bool reverse, int previous);
void tt_timer_init(tt_timer_t *timer);
bool tt_timer_start(tt_timer_t *timer, const tt_config_t *config, tt_face_t face, int64_t now_ms);
bool tt_timer_tick(tt_timer_t *timer, int64_t now_ms);
void tt_timer_face(tt_timer_t *timer, const tt_config_t *config, tt_face_t face, int64_t now_ms);
void tt_timer_toggle(tt_timer_t *timer, int64_t now_ms);
void tt_timer_stop(tt_timer_t *timer);
