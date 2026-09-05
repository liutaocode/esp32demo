#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "balloon_rush_state.h"

static void hit(br_state_t *s, bool perfect)
{
    s->needle = s->target + (perfect ? 0 : s->half_width);
    assert(br_stop(s));
}
int main(void)
{
    br_state_t s = {0};
    br_start(&s, 1);
    assert(s.round == 1 && s.pot == 0 && s.page == BR_AIM);
    assert(!br_collect(&s) && !br_continue(&s));
    hit(&s, true);
    assert(s.pot == 150 && s.perfects == 1 && s.page == BR_CHOICE);
    assert(!br_stop(&s) && !br_continue(&s) && !br_collect(&s));
    br_tick(&s, BR_FEEDBACK - 1); assert(!br_collect(&s));
    br_tick(&s, 1); assert(br_collect(&s));
    assert(s.score == 150 && s.best == 150 && s.new_best);
    br_home(&s); assert(s.best == 150);
    br_start(&s, 2); hit(&s, false); br_tick(&s, BR_FEEDBACK);
    assert(br_continue(&s) && s.round == 2 && s.pot == 100);
    s.needle = s.target - s.half_width - 1;
    assert(br_stop(&s) && s.burst && s.score == 0 && s.best == 150);
    assert(!s.new_best && !br_collect(&s));
    br_start(&s, 3);
    for (unsigned r = 1; r <= BR_ROUNDS; r++) {
        assert(s.round == r && s.target >= s.half_width && s.target + s.half_width <= 1000);
        hit(&s, true);
        if (r < BR_ROUNDS) { br_tick(&s, BR_FEEDBACK); assert(br_continue(&s)); }
    }
    assert(s.page == BR_RESULT && !s.burst && s.score == 30600 && s.best == 30600);
    assert(s.clears == 8 && s.perfects == 8 && !br_continue(&s));
    br_start(&s, 4); br_tick(&s, 400);
    uint32_t phase = s.phase, remaining = br_remaining(&s);
    br_pause(&s); br_tick(&s, UINT_MAX);
    assert(s.page == BR_PAUSED && s.phase == phase && br_remaining(&s) == remaining);
    assert(!br_stop(&s)); br_pause(&s); assert(s.page == BR_AIM);
    br_tick(&s, remaining - 1); assert(s.page == BR_AIM);
    br_tick(&s, 1); assert(s.page == BR_RESULT && s.burst && br_remaining(&s) == 0);
    br_start(&s, 5); hit(&s, false); br_pause(&s); br_tick(&s, 5000);
    assert(s.cooldown == BR_FEEDBACK); br_pause(&s); br_tick(&s, UINT_MAX);
    assert(br_collect(&s) && s.score == 100);
    /* Motion must be invariant to frame partitioning, including reflections. */
    br_state_t a = {0}, b = {0}; br_start(&a, 7); br_start(&b, 7);
    br_tick(&a, 7890); for (int i = 0; i < 789; i++) br_tick(&b, 10);
    assert(a.needle == b.needle && a.phase == b.phase && a.elapsed == b.elapsed);
    /* Every target and edge is playable; just outside either edge bursts. */
    for (uint32_t seed = 0; seed < 500; seed++) {
        br_start(&s, seed);
        for (unsigned r = 1; r <= BR_ROUNDS; r++) {
            br_state_t edge = s;
            edge.needle = edge.target - edge.half_width;
            assert(br_stop(&edge) && !edge.burst);
            edge = s; edge.needle = edge.target + edge.half_width + 1;
            assert(br_stop(&edge) && edge.burst);
            hit(&s, false);
            if (r < BR_ROUNDS) { br_tick(&s, 450); assert(br_continue(&s)); }
        }
        assert(s.score == 20400 && s.clears == BR_ROUNDS);
    }
    br_start(&s, 6); br_tick(&s, UINT_MAX); assert(s.burst);
    puts("Balloon Rush state: scoring, risk, eight rounds, edge hits, timing, pause PASS");
}
