// SPDX-License-Identifier: GPL-3.0-only
#include "timetick_core.h"
#include <math.h>
#include <stdlib.h>

tt_config_t tt_config_default(void)
{
    return (tt_config_t){.seconds = {0, 300, 600, 1500, 0, 0},
                         .home_axis = 2, .right_axis = 1, .policy = TT_HOME_RESTART};
}

bool tt_config_valid(const tt_config_t *c)
{
    if (!c || c->policy < TT_HOME_RESTART || c->policy > TT_HOME_RESUME ||
        abs(c->home_axis) < 1 || abs(c->home_axis) > 3 ||
        abs(c->right_axis) < 1 || abs(c->right_axis) > 3 ||
        abs(c->home_axis) == abs(c->right_axis) || c->seconds[TT_HOME] != 0 ||
        c->seconds[TT_FRONT] != 0 || c->seconds[TT_BACK] != 0) return false;
    for (int i = 1; i < 6; ++i) if (c->seconds[i] > 86400) return false;
    return true;
}

void tt_orientation_sample(tt_orientation_t *s, const float a[3],
                           const float g[3], bool valid, int64_t now)
{
    float norm = 0;
    for (int i = 0; i < 3; ++i) {
        valid = valid && isfinite(a[i]) && isfinite(g[i]) && fabsf(g[i]) < 35.0f;
        norm += a[i] * a[i];
    }
    valid = valid && norm >= 0.75f * 0.75f && norm <= 1.25f * 1.25f;
    if (!valid || (s->initialized && (now < s->last_sample || now - s->last_sample > 200))) {
        s->initialized = false;
        s->settled = false;
        s->candidate = 0;
    }
    s->last_sample = now;
    if (!valid) return;
    for (int i = 0; i < 3; ++i)
        s->filtered[i] = s->initialized ? s->filtered[i] * 0.8f + a[i] * 0.2f : a[i];
    s->initialized = true;
    int largest = 0;
    for (int i = 1; i < 3; ++i)
        if (fabsf(s->filtered[i]) > fabsf(s->filtered[largest])) largest = i;
    float dominant = fabsf(s->filtered[largest]), second = 0;
    for (int i = 0; i < 3; ++i)
        if (i != largest && fabsf(s->filtered[i]) > second) second = fabsf(s->filtered[i]);
    int candidate = dominant >= 0.82f && dominant - second >= 0.18f
                    ? (largest + 1) * (s->filtered[largest] >= 0 ? 1 : -1) : 0;
    if (!candidate || candidate != s->candidate) {
        s->candidate = candidate;
        s->candidate_since = now;
        s->settled = false;
    } else if (now - s->candidate_since >= 450) {
        s->stable = candidate;
        s->settled = true;
    }
}

tt_face_t tt_face_from_axis(const tt_config_t *c, int axis)
{
    if (!c->calibrated || !axis || !tt_config_valid(c)) return TT_UNKNOWN;
    if (axis == c->home_axis) return TT_HOME;
    if (axis == -c->home_axis) return TT_INVERTED;
    if (axis == c->right_axis) return TT_RIGHT;
    if (axis == -c->right_axis) return TT_LEFT;
    int right[3] = {0}, up[3] = {0}, normal[3];
    right[abs(c->right_axis)-1] = c->right_axis > 0 ? 1 : -1;
    up[abs(c->home_axis)-1] = c->home_axis > 0 ? 1 : -1;
    normal[0] = right[1]*up[2] - right[2]*up[1];
    normal[1] = right[2]*up[0] - right[0]*up[2];
    normal[2] = right[0]*up[1] - right[1]*up[0];
    for (int i = 0; i < 3; ++i) {
        if (axis == normal[i]*(i+1) && normal[i]) return TT_FRONT;
        if (axis == -normal[i]*(i+1) && normal[i]) return TT_UNKNOWN; /* Screen down never starts a preset. */
    }
    return TT_UNKNOWN;
}

int tt_rotation(tt_face_t face, bool reverse, int previous)
{
    int angle;
    switch (face) {
        case TT_HOME: angle = 0; break;
        case TT_RIGHT: angle = 270; break;
        case TT_INVERTED: angle = 180; break;
        case TT_LEFT: angle = 90; break;
        default: return previous; /* Gravity cannot determine in-plane yaw when flat. */
    }
    return reverse ? (360-angle)%360 : angle;
}

void tt_timer_init(tt_timer_t *t)
{
    *t = (tt_timer_t){.state = TT_IDLE, .active = TT_UNKNOWN, .observed = TT_UNKNOWN};
}

bool tt_timer_tick(tt_timer_t *t, int64_t now)
{
    if (t->state != TT_RUNNING) return false;
    t->remaining_ms = t->deadline_ms > now ? t->deadline_ms - now : 0;
    if (t->remaining_ms) return false;
    t->state = TT_FINISHED;
    return true;
}

void tt_timer_face(tt_timer_t *t, const tt_config_t *c, tt_face_t face, int64_t now)
{
    if (face < TT_HOME || face >= TT_FRONT || face == t->observed || !tt_config_valid(c) || !c->calibrated) return;
    tt_timer_tick(t, now);
    t->observed = face;
    if (face == TT_HOME) {
        if (c->policy != TT_HOME_CONTINUE && t->state == TT_RUNNING) t->state = TT_PAUSED;
        return;
    }
    if (!c->seconds[face]) { tt_timer_stop(t); return; }
    if (face == t->active && c->policy != TT_HOME_RESTART &&
        (t->state == TT_RUNNING || t->state == TT_PAUSED)) {
        if (t->state == TT_PAUSED) t->deadline_ms = now + t->remaining_ms;
        t->state = TT_RUNNING;
        return;
    }
    (void)tt_timer_start(t,c,face,now);
}

bool tt_timer_start(tt_timer_t *t, const tt_config_t *c, tt_face_t face, int64_t now)
{
    if (!tt_config_valid(c) || !c->calibrated || face<=TT_HOME || face>=TT_FRONT || !c->seconds[face]) return false;
    t->observed = t->active = face;
    t->duration_s = c->seconds[face];
    t->remaining_ms = (int64_t)t->duration_s * 1000;
    t->deadline_ms = now + t->remaining_ms;
    t->state = TT_RUNNING;
    return true;
}

void tt_timer_toggle(tt_timer_t *t, int64_t now)
{
    tt_timer_tick(t, now);
    if (t->state == TT_RUNNING) t->state = TT_PAUSED;
    else if (t->state == TT_PAUSED) {
        t->deadline_ms = now + t->remaining_ms;
        t->state = TT_RUNNING;
    }
}

void tt_timer_stop(tt_timer_t *t)
{
    t->state = TT_IDLE;
    t->remaining_ms = 0;
    t->active = TT_UNKNOWN;
}
