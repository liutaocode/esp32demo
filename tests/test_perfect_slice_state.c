#include "perfect_slice_state.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

static void aim(slice_state_t *s, unsigned x) { s->phase = (x - 1) * 1000U; }
static void finish_cut(slice_state_t *s) { slice_tick(s, SLICE_REVEAL_MS); assert(slice_next(s)); }

int main(void)
{
    slice_state_t s = {0};
    assert(!slice_cut(&s) && !slice_next(&s));
    for (unsigned mode = 0; mode < 2; mode++) {
        s.mode = mode; slice_start(&s);
        for (unsigned round = 0; round < SLICE_ROUNDS; round++) {
            assert(s.round == round && s.page == SLICE_PLAY);
            aim(&s, slice_target_x(&s));
            assert(slice_cut(&s) && s.perfect && s.perfects == round + 1);
            unsigned score = s.score;
            assert(!slice_cut(&s) && !slice_next(&s) && s.score == score);
            slice_tick(&s, 399); assert(!slice_next(&s));
            finish_cut(&s);
        }
        assert(s.page == SLICE_RESULT && s.score == 1150 && s.new_best && s.best[mode] == 1150);
        assert(!slice_next(&s) && !slice_cut(&s));
    }
    for (unsigned mode = 0; mode < 2; mode++) {
        s.mode = mode; slice_start(&s);
        for (unsigned round = 0; round < SLICE_ROUNDS; round++) {
            slice_state_t base = s;
            for (unsigned x = 1; x < SLICE_WIDTH; x++) {
                slice_state_t a = base; aim(&a, x); assert(slice_cut(&a));
                assert(a.cut == x && a.points <= 120 && a.score >= base.score);
                assert(a.perfect == (a.error <= 2));
                if (a.error >= 20) assert(a.points == 0 && a.streak == 0);
            }
            aim(&s, 1); assert(slice_cut(&s)); finish_cut(&s);
        }
        assert(!s.new_best && s.best[mode] == 1150);
    }
    slice_start(&s);
    slice_tick(&s, 777); unsigned phase = s.phase;
    slice_pause(&s); assert(s.page == SLICE_PAUSED);
    slice_tick(&s, UINT32_MAX); assert(s.phase == phase && !slice_cut(&s));
    slice_pause(&s); assert(s.page == SLICE_PLAY);
    /* Reflection and elapsed-time partitioning, including large deltas. */
    for (unsigned i = 0; i < 50000; i++) {
        slice_tick(&s, i);
        assert(slice_position(&s) >= 1 && slice_position(&s) < SLICE_WIDTH);
    }
    slice_state_t a = s, b = s;
    slice_tick(&a, 10000);
    for (unsigned i = 0; i < 1000; i++) slice_tick(&b, 10);
    assert(a.phase == b.phase);
    slice_tick(&s, UINT32_MAX); assert(slice_position(&s) < SLICE_WIDTH);
    aim(&s, slice_target_x(&s)); slice_cut(&s); slice_tick(&s, 200);
    slice_pause(&s); slice_tick(&s, 1000); assert(s.reveal_ms == 200);
    slice_pause(&s); slice_tick(&s, UINT32_MAX); assert(s.reveal_ms == 400);
    slice_home(&s); assert(s.page == SLICE_HOME && s.best[0] == 1150);
    puts("Perfect Slice state: exhaustive cuts, scoring, records, reflection, pause and input guards PASS");
}
