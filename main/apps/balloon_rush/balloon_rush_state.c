#include "balloon_rush_state.h"
#include <string.h>

static void result(br_state_t *s, bool burst)
{
    s->burst = burst;
    s->score = burst ? 0 : s->pot;
    s->new_best = s->score > s->best;
    if (s->new_best) s->best = s->score;
    s->page = BR_RESULT;
}

void br_home(br_state_t *s)
{
    uint32_t best = s->best;
    memset(s, 0, sizeof(*s));
    s->best = best;
    s->page = BR_HOME;
}

static void next(br_state_t *s)
{
    s->round++;
    s->seed = s->seed * 1664525U + 1013904223U;
    s->half_width = 190 - (s->round - 1) * 15;
    s->target = 330 + s->seed % 341;
    s->needle = 0;
    s->phase = s->elapsed = s->cooldown = 0;
    s->page = BR_AIM;
}

void br_start(br_state_t *s, uint32_t seed)
{
    br_home(s);
    s->seed = seed;
    next(s);
}

uint32_t br_reward(const br_state_t *s) { return 100U * s->round * s->round; }
uint32_t br_remaining(const br_state_t *s)
{
    return s->elapsed >= BR_TIMEOUT ? 0 : BR_TIMEOUT - s->elapsed;
}

void br_tick(br_state_t *s, uint32_t ms)
{
    if (s->page == BR_CHOICE) {
        s->cooldown = ms >= s->cooldown ? 0 : s->cooldown - ms;
    } else if (s->page == BR_AIM) {
        if (ms >= br_remaining(s)) {
            s->elapsed = BR_TIMEOUT;
            result(s, true);
            return;
        }
        s->elapsed += ms;
        uint32_t speed = 340 + (s->round - 1) * 40;
        s->phase = (s->phase + ms * speed) % 2000000U;
        uint32_t pos = s->phase / 1000;
        s->needle = pos <= 1000 ? pos : 2000 - pos;
    }
}

bool br_stop(br_state_t *s)
{
    if (s->page != BR_AIM) return false;
    unsigned distance = s->needle > s->target ? s->needle - s->target : s->target - s->needle;
    if (distance > s->half_width) {
        result(s, true);
    } else {
        s->perfect = distance <= 30;
        s->perfects += s->perfect;
        s->clears++;
        s->pot += br_reward(s) + (s->perfect ? br_reward(s) / 2 : 0);
        s->page = BR_CHOICE;
        s->cooldown = BR_FEEDBACK;
        if (s->round == BR_ROUNDS) result(s, false);
    }
    return true;
}

bool br_continue(br_state_t *s)
{
    if (s->page != BR_CHOICE || s->cooldown) return false;
    next(s);
    return true;
}

bool br_collect(br_state_t *s)
{
    if (s->page != BR_CHOICE || s->cooldown) return false;
    result(s, false);
    return true;
}

void br_pause(br_state_t *s)
{
    if (s->page == BR_PAUSED) s->page = s->resume;
    else if (s->page == BR_AIM || s->page == BR_CHOICE) {
        s->resume = s->page;
        s->page = BR_PAUSED;
    }
}
