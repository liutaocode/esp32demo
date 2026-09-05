#include "stack_rush_state.h"

#include <stdlib.h>
#include <string.h>

static void spawn(stack_rush_state_t *s)
{
    s->moving.width = s->tower[s->count - 1].width;
    int span = STACK_RUSH_FIELD - s->moving.width;
    s->phase = (s->floors % 2U) ? (uint32_t)span * 1000U : 0U;
    s->moving.x = (int16_t)(s->phase / 1000U);
    s->page = STACK_MOVING;
}

void stack_rush_init(stack_rush_state_t *s)
{
    *s = (stack_rush_state_t){ .page = STACK_READY };
}

void stack_rush_select(stack_rush_state_t *s)
{
    if (s->page == STACK_READY) s->mode ^= 1U;
}

void stack_rush_start(stack_rush_state_t *s)
{
    if (s->page != STACK_READY && s->page != STACK_RESULT) return;
    uint8_t mode = s->mode, best[2] = { s->best[0], s->best[1] };
    stack_rush_init(s);
    s->mode = mode;
    memcpy(s->best, best, sizeof(best));
    s->count = 1;
    s->tower[0] = (stack_rush_block_t){
        (STACK_RUSH_FIELD - STACK_RUSH_WIDTH) / 2, STACK_RUSH_WIDTH
    };
    spawn(s);
}

uint16_t stack_rush_speed(const stack_rush_state_t *s)
{
    unsigned speed = (s->mode ? 100U : 65U) + s->floors * 3U;
    unsigned cap = s->mode ? 200U : 150U;
    return (uint16_t)(speed < cap ? speed : cap);
}

static void finish(stack_rush_state_t *s)
{
    s->new_best = s->floors > s->best[s->mode];
    if (s->new_best) s->best[s->mode] = s->floors;
    s->page = STACK_RESULT;
}

void stack_rush_tick(stack_rush_state_t *s, uint32_t elapsed_ms)
{
    if (s->page == STACK_SETTLING) {
        /* New floors start at the edge after the reveal, even after a stall. */
        uint32_t remaining = STACK_RUSH_SETTLE_MS - s->settle_ms;
        if (elapsed_ms < remaining) {
            s->settle_ms += elapsed_ms;
        } else {
            s->settle_ms = STACK_RUSH_SETTLE_MS;
            if (s->won) finish(s);
            else spawn(s);
        }
        return;
    }
    if (s->page != STACK_MOVING) return;
    uint32_t span = (STACK_RUSH_FIELD - s->moving.width) * 1000U;
    uint32_t period = span * 2U;
    s->phase = (uint32_t)(((uint64_t)s->phase +
                           (uint64_t)elapsed_ms * stack_rush_speed(s)) % period);
    uint32_t position = s->phase <= span ? s->phase : period - s->phase;
    s->moving.x = (int16_t)(position / 1000U);
}

bool stack_rush_drop(stack_rush_state_t *s)
{
    if (s->page != STACK_MOVING) return false;
    stack_rush_block_t top = s->tower[s->count - 1];
    int delta = s->moving.x - top.x;
    int overlap = top.width - abs(delta);
    s->debris = (stack_rush_block_t){0};
    s->perfect = false;
    s->restored = false;
    /* An edge touch or a complete miss loses, even within snap tolerance. */
    if (overlap <= 0) {
        finish(s);
        return true;
    }
    s->perfect = abs(delta) <= 2;
    if (s->perfect) {
        s->perfects++;
        s->streak++;
        if (s->streak % 3U == 0 && top.width < STACK_RUSH_WIDTH) {
            int growth = STACK_RUSH_WIDTH - top.width;
            if (growth > 8) growth = 8;
            top.x -= growth / 2;
            top.width += growth;
            if (top.x < 0) top.x = 0;
            if (top.x + top.width > STACK_RUSH_FIELD)
                top.x = STACK_RUSH_FIELD - top.width;
            s->restored = true;
        }
    } else {
        s->streak = 0;
        s->debris.width = (int16_t)abs(delta);
        s->debris.x = delta > 0 ? top.x + top.width : s->moving.x;
        if (delta > 0) top.x = s->moving.x;
        top.width = (int16_t)overlap;
    }
    if (s->count == STACK_RUSH_HISTORY) {
        memmove(s->tower, s->tower + 1,
                (STACK_RUSH_HISTORY - 1) * sizeof(s->tower[0]));
        s->count--;
    }
    s->tower[s->count++] = top;
    s->floors++;
    s->won = s->floors == STACK_RUSH_GOAL;
    s->settle_ms = 0;
    s->page = STACK_SETTLING;
    return true;
}

void stack_rush_pause(stack_rush_state_t *s)
{
    if (s->page == STACK_PAUSED) s->page = s->resume_page;
    else if (s->page == STACK_MOVING || s->page == STACK_SETTLING) {
        s->resume_page = s->page;
        s->page = STACK_PAUSED;
    }
}

void stack_rush_home(stack_rush_state_t *s)
{
    s->page = STACK_READY;
}
