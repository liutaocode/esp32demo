#include "needle_rush_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void settle(nr_state_t *s)
{
    for (unsigned i = 0; i < 30 && (s->page == NR_FLIGHT || s->page == NR_FEEDBACK); i++) nr_tick(s, 20);
}
static bool safe(const nr_state_t *s)
{
    for (unsigned i = 0; i < s->count; i++)
        if (nr_distance(nr_angle(s, i), NR_IMPACT) <= NR_CLEARANCE) return false;
    return true;
}
int main(void)
{
    assert(nr_distance(359000, 1000) == 2000);
    assert(nr_distance(1000, 359000) == 2000);
    assert(nr_distance(0, 180000) == 180000);
    nr_state_t s = {0}; nr_start(&s, 1234);
    assert(s.lives == 3 && s.remaining == 5 && s.count == 2);
    for (int d = -NR_CLEARANCE - 1; d <= NR_CLEARANCE + 1; d++) {
        nr_start(&s, 0); s.count = 1; s.pins[0] = NR_IMPACT + d;
        assert(nr_fire(&s));
        assert(s.hit == (d >= -NR_CLEARANCE && d <= NR_CLEARANCE));
        assert(!nr_fire(&s));
    }
    nr_start(&s, 0); s.phase = 90000;
    nr_fire(&s); assert(s.hit);
    nr_tick(&s, 60);
    nr_pause(&s); nr_state_t paused = s;
    nr_tick(&s, UINT32_MAX); assert(memcmp(&s, &paused, sizeof(s)) == 0);
    nr_pause(&s); assert(s.page == NR_FLIGHT && s.elapsed == 60);
    settle(&s); assert(s.lives == 2 && s.score == 0 && s.count == 2);
    nr_fire(&s); settle(&s); nr_fire(&s); settle(&s);
    assert(s.page == NR_RESULT && !s.won && s.lives == 0);
    assert(!nr_fire(&s));
    s.mode = 1; nr_start(&s, 0); s.phase = 90000;
    nr_fire(&s); settle(&s); assert(s.page == NR_RESULT);
    nr_start(&s, 51); int32_t phase = s.phase;
    nr_tick(&s, UINT32_MAX);
    assert(nr_distance(phase, s.phase) == (unsigned)nr_speed(&s) * 60);

    /* An automated player completes every level by waiting for real free
       space, without assigning pin positions or skipping the state machine. */
    for (unsigned mode = 0; mode < 2; mode++) for (unsigned seed = 0; seed < 100; seed++) {
        memset(&s, 0, sizeof(s)); s.mode = mode; nr_start(&s, seed * 97);
        unsigned ticks = 0, shots = 0;
        while (s.page != NR_RESULT && ticks++ < 100000) {
            if (s.page == NR_SPIN && safe(&s)) {
                nr_state_t copy = s;
                nr_fire(&s); nr_fire(&copy);
                assert(!s.hit && memcmp(&s, &copy, sizeof(s)) == 0);
                shots++;
            }
            nr_tick(&s, 20);
            assert(s.count <= NR_MAX_PINS && s.remaining <= 8);
            assert(s.phase >= 0 && s.phase < NR_TURN);
        }
        assert(s.won && s.page == NR_RESULT && s.level == 9);
        assert(shots == 62 && s.score == 720 && s.best_streak == 62);
        assert(s.new_best && s.best[mode] == 720 && s.best[mode ^ 1U] == 0);
        unsigned challenge = s.challenge;
        nr_home(&s); nr_start(&s, challenge);
        nr_state_t fresh = {0}; fresh.mode = mode; nr_start(&fresh, challenge);
        assert(s.phase == fresh.phase && s.count == fresh.count && s.best[mode] == 720);
    }
    puts("Needle Rush state: collision boundaries, pause, retries, timing, 200 complete games PASS");
}
