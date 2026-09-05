#include "perfect_slice_state.h"

static void prepare_round(slice_state_t *s)
{
    static const unsigned targets[SLICE_ROUNDS] = {50, 50, 40, 60, 30, 70, 45, 55, 35, 65};
    s->target = targets[s->round];
    s->phase = s->round % 2 ? (SLICE_WIDTH - 2) * 1000U : 0;
    s->page = SLICE_PLAY;
    s->reveal_ms = 0;
}

void slice_home(slice_state_t *s) { s->page = SLICE_HOME; }

void slice_start(slice_state_t *s)
{
    unsigned mode = s->mode & 1U, a = s->best[0], b = s->best[1];
    *s = (slice_state_t){.mode = mode, .best = {a, b}};
    prepare_round(s);
}

unsigned slice_speed(const slice_state_t *s)
{
    return (s->mode ? 100U : 64U) + s->round * (s->mode ? 8U : 5U);
}

unsigned slice_position(const slice_state_t *s)
{
    const uint32_t span = (SLICE_WIDTH - 2) * 1000U;
    uint32_t p = s->phase % (2U * span);
    return 1U + (p > span ? 2U * span - p : p) / 1000U;
}

unsigned slice_target_x(const slice_state_t *s)
{
    return (s->target * SLICE_WIDTH + 50U) / 100U;
}

void slice_tick(slice_state_t *s, uint32_t ms)
{
    if (s->page == SLICE_PLAY) {
        uint64_t next = s->phase + (uint64_t)slice_speed(s) * ms;
        s->phase = next % (2U * (SLICE_WIDTH - 2) * 1000U);
    } else if (s->page == SLICE_REVEAL) {
        s->reveal_ms = ms >= SLICE_REVEAL_MS - s->reveal_ms ?
                       SLICE_REVEAL_MS : s->reveal_ms + ms;
    }
}

bool slice_cut(slice_state_t *s)
{
    if (s->page != SLICE_PLAY) return false;
    s->cut = slice_position(s);
    unsigned actual = (s->cut * 100U + SLICE_WIDTH / 2) / SLICE_WIDTH;
    s->error = actual > s->target ? actual - s->target : s->target - actual;
    s->perfect = s->error <= 2;
    s->streak = s->perfect ? s->streak + 1 : 0;
    if (s->perfect) s->perfects++;
    unsigned bonus = s->streak > 5 ? 20 : s->streak ? (s->streak - 1) * 5 : 0;
    s->points = (s->perfect ? 100 : s->error < 20 ? 100 - s->error * 5 : 0) + bonus;
    s->score += s->points;
    s->reveal_ms = 0;
    s->page = SLICE_REVEAL;
    return true;
}

bool slice_next(slice_state_t *s)
{
    if (s->page != SLICE_REVEAL || s->reveal_ms < SLICE_REVEAL_MS) return false;
    if (++s->round == SLICE_ROUNDS) {
        s->page = SLICE_RESULT;
        s->new_best = s->score > s->best[s->mode];
        if (s->new_best) s->best[s->mode] = s->score;
    } else prepare_round(s);
    return true;
}

void slice_pause(slice_state_t *s)
{
    if (s->page == SLICE_PAUSED) s->page = s->resume;
    else if (s->page == SLICE_PLAY || s->page == SLICE_REVEAL) {
        s->resume = s->page;
        s->page = SLICE_PAUSED;
    }
}
