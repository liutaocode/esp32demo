#include "cloud_hop_state.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void land(ch_state_t *s, int offset)
{
    s->cursor = s->target_x + offset;
    assert(ch_jump(s)); assert(!ch_jump(s));
    ch_tick(s, CH_FLIGHT_MS - 1); assert(s->page == CH_FLY);
    ch_tick(s, 1); assert(s->page == CH_LANDED);
}

int main(void)
{
    ch_state_t s = {0};
    ch_start(&s, 6789);
    assert(s.page == CH_AIM && s.lives == 3 && s.score == 0);
    land(&s, CH_PERFECT); assert(s.perfect && s.score == 25 && s.level == 1);
    ch_tick(&s, CH_FEEDBACK_MS);
    land(&s, -CH_PERFECT); assert(s.perfect && s.score == 55 && s.streak == 2);
    ch_tick(&s, CH_FEEDBACK_MS);
    land(&s, CH_PERFECT + 1); assert(s.hit && !s.perfect && s.score == 65 && !s.streak);
    ch_tick(&s, CH_FEEDBACK_MS);
    int x = s.target_x, w = s.target_width;
    land(&s, w / 2); assert(s.hit && ch_miss_distance(&s) == 0);
    ch_tick(&s, CH_FEEDBACK_MS);
    x = s.target_x; w = s.target_width;
    land(&s, w / 2 + 1);
    assert(!s.hit && s.lives == 2 && ch_miss_distance(&s) == 1);
    ch_tick(&s, CH_FEEDBACK_MS); assert(s.target_x == x && s.target_width == w);
    land(&s, -w / 2 - 1); ch_tick(&s, CH_FEEDBACK_MS); assert(s.lives == 1);
    land(&s, w); ch_tick(&s, CH_FEEDBACK_MS);
    assert(s.page == CH_RESULT && !s.lives && s.new_best && s.best[0] == 75);
    assert(!ch_jump(&s)); ch_pause(&s); assert(s.page == CH_RESULT);

    for (unsigned mode = 0; mode < 2; mode++) {
        s.mode = mode; ch_start(&s, 42);
        for (unsigned level = 0; level < CH_GOAL; level++) {
            ch_state_t before = s;
            ch_pause(&s); assert(s.page == CH_PAUSED);
            ch_tick(&s, UINT_MAX); assert(s.cursor == before.cursor && !ch_jump(&s));
            ch_pause(&s); assert(s.page == CH_AIM);
            for (unsigned i = 0; i < 200; i++) {
                ch_tick(&s, 19);
                assert(s.cursor >= CH_CURSOR_MIN && s.cursor <= CH_CURSOR_MAX);
            }
            land(&s, 0); assert(s.level == level + 1);
            ch_tick(&s, UINT_MAX);
        }
        assert(s.page == CH_RESULT && s.level == 25 && s.perfects == 25);
        assert(s.score == 1175 && s.best[mode] == 1175 && s.best_streak == 25);
        uint16_t record = s.best[mode]; ch_start(&s, 42); assert(s.best[mode] == record);
        land(&s, 100); ch_tick(&s, CH_FEEDBACK_MS);
        if (mode) assert(s.page == CH_RESULT && !s.new_best);
    }
    /* Same challenge produces identical geometry, phases and speeds regardless
       of retries, elapsed time, or another player's score. */
    for (unsigned seed = 0; seed < 10000; seed += 7) {
        ch_state_t a = {0}, b = {0}; ch_start(&a, seed); ch_start(&b, seed);
        for (unsigned level = 0; level < CH_GOAL; level++) {
            assert(a.target_x == b.target_x && a.target_width == b.target_width && a.cursor == b.cursor);
            assert(a.target_x - a.target_width / 2 >= 0);
            assert(a.target_x + a.target_width / 2 < CH_FIELD);
            a.cursor = a.target_x; assert(ch_jump(&a));
            for (unsigned t = 0; t < CH_FLIGHT_MS; t++) {
                a.phase_ms = t; ch_point_t p = ch_pose(&a);
                assert(p.x >= 11 && p.x < CH_FIELD - 11 && p.y >= 0 && p.y <= 106);
            }
            ch_pause(&a); ch_point_t p = ch_pose(&a);
            ch_tick(&a, UINT_MAX); assert(p.x == ch_pose(&a).x && p.y == ch_pose(&a).y);
            ch_pause(&a); ch_tick(&a, UINT_MAX);
            land(&b, 0); ch_tick(&a, CH_FEEDBACK_MS); ch_tick(&b, CH_FEEDBACK_MS);
        }
    }
    puts("Cloud Hop state: boundaries, scoring, lives, pause, overflow, 1429 courses PASS");
}
