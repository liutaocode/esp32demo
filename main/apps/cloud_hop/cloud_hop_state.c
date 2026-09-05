#include "cloud_hop_state.h"

#include <stdlib.h>
#include <string.h>

static uint32_t mix(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    return x ^ (x >> 16);
}

static uint32_t period(const ch_state_t *s)
{
    return (s->mode ? 1550U : 2300U) - s->level * 24U;
}

static void target(ch_state_t *s)
{
    /* Course depends on round + level, never on timing or past mistakes. */
    uint32_t r = mix(s->round_seed + s->level * 7919U);
    s->target_x = 44 + (int)(r % 110);
    s->target_width = (s->mode ? 56 : 70) - s->level;
    s->cursor_phase = (r >> 8) % period(s);
    s->phase_ms = 0;
    s->page = CH_AIM;
    ch_tick(s, 0);
}

void ch_home(ch_state_t *s) { s->page = CH_HOME; }

void ch_start(ch_state_t *s, uint16_t challenge)
{
    uint16_t best0 = s->best[0], best1 = s->best[1];
    uint8_t mode = s->mode == 1;
    memset(s, 0, sizeof(*s));
    s->best[0] = best0; s->best[1] = best1; s->mode = mode;
    s->challenge = challenge % 10000;
    s->round_seed = mix(s->challenge + 1U);
    s->lives = mode ? 1 : 3;
    s->current_x = CH_FIELD / 2;
    target(s);
}

bool ch_jump(ch_state_t *s)
{
    if (s->page != CH_AIM) return false;
    s->landing_x = s->cursor;
    int distance = abs(s->landing_x - s->target_x);
    s->hit = distance <= s->target_width / 2;
    s->perfect = distance <= CH_PERFECT;
    s->phase_ms = 0;
    s->page = CH_FLY;
    return true;
}

static void finish(ch_state_t *s)
{
    s->page = CH_RESULT;
    s->new_best = s->score > s->best[s->mode];
    if (s->new_best) s->best[s->mode] = s->score;
}

void ch_tick(ch_state_t *s, uint32_t elapsed_ms)
{
    if (s->page == CH_AIM) {
        uint32_t p = period(s);
        s->cursor_phase = (s->cursor_phase + elapsed_ms % p) % p;
        uint32_t triangular = s->cursor_phase < p / 2 ? s->cursor_phase : p - s->cursor_phase;
        s->cursor = CH_CURSOR_MIN + (int)(triangular * (CH_CURSOR_MAX - CH_CURSOR_MIN) * 2 / p);
    } else if (s->page == CH_FLY || s->page == CH_LANDED) {
        uint32_t duration = s->page == CH_FLY ? CH_FLIGHT_MS : CH_FEEDBACK_MS;
        if (elapsed_ms < duration - s->phase_ms) { s->phase_ms += elapsed_ms; return; }
        s->phase_ms = 0;
        if (s->page == CH_FLY) {
            if (s->hit) {
                s->level++;
                if (s->perfect) {
                    s->perfects++; s->streak++;
                    if (s->streak > s->best_streak) s->best_streak = s->streak;
                    s->score += 20 + (s->streak > 6 ? 6 : s->streak) * 5;
                } else { s->streak = 0; s->score += 10; }
            } else { s->lives--; s->streak = 0; }
            s->page = CH_LANDED;
        } else if (!s->lives || s->level == CH_GOAL) finish(s);
        else {
            if (s->hit) s->current_x = s->landing_x;
            target(s);
        }
        /* Never skip visible feedback after a long scheduler delay. */
    }
}

void ch_pause(ch_state_t *s)
{
    if (s->page == CH_PAUSED) s->page = s->resume;
    else if (s->page == CH_AIM || s->page == CH_FLY || s->page == CH_LANDED) {
        s->resume = s->page; s->page = CH_PAUSED;
    }
}

ch_point_t ch_pose(const ch_state_t *s)
{
    ch_page_t p = s->page == CH_PAUSED ? s->resume : s->page;
    if (p == CH_FLY) {
        int t = (int)s->phase_ms, d = CH_FLIGHT_MS;
        return (ch_point_t){ s->current_x + (s->landing_x - s->current_x) * t / d,
            106 - 78 * t / d - 4 * 55 * t * (d - t) / (d * d) };
    }
    if (p == CH_LANDED) return (ch_point_t){s->landing_x,
        s->hit ? 28 : (int16_t)(28 + 78 * s->phase_ms / CH_FEEDBACK_MS)};
    return (ch_point_t){s->current_x, 106};
}

int ch_miss_distance(const ch_state_t *s)
{
    int n = abs(s->landing_x - s->target_x) - s->target_width / 2;
    return n > 0 ? n : 0;
}
