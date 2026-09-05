#include "down_100_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void run_ms(d100_state_t *s, unsigned ms)
{
    for (unsigned t = 0; t < ms; t += 10) d100_tick(s, 10);
}
static void fixture(d100_state_t *s, d100_tile_t tile)
{
    *s = (d100_state_t){0}; d100_start(s, 45);
    s->rows[0].tile[2] = tile;
}
static unsigned min_gems = 100, max_gems;
static void safe_route(unsigned seed, unsigned mode)
{
    d100_state_t s = {.mode = mode}; d100_start(&s, seed);
    unsigned ticks = 0, expected_gems = 0, checked_depth = 0, supplies = 0;
    unsigned spikes[5] = {0}, cracks[5] = {0}, combinations[5] = {0};
    unsigned last_spike[5] = {4, 20, 40, 60, 80};
    while (s.page == D100_PLAY && ticks++ < 20000) {
        for (int i = 0; i < D100_ROWS; i++) {
            d100_row_t *r = &s.rows[i];
            if (!r->depth || r->depth <= checked_depth) continue;
            /* Generated rows may occupy recycled slots: inspect in depth order. */
            if (r->depth != checked_depth + 1) continue;
            checked_depth = r->depth;
            assert(r->tile[1] == D100_SOLID || r->tile[1] == D100_GEM || r->tile[1] == D100_HEAL || r->tile[1] == D100_CRACK);
            if (r->depth < 100) assert((r->tile[0] == D100_HOLE) != (r->tile[2] == D100_HOLE));
            unsigned hazards = 0;
            for (int lane = 0; lane < 3; lane++) {
                expected_gems += r->tile[lane] == D100_GEM;
                supplies += r->tile[lane] == D100_HEAL;
                hazards += r->tile[lane] == D100_SPIKE || r->tile[lane] == D100_CRACK;
            }
            unsigned stage = d100_stage(r->depth), row_spikes = 0, row_cracks = 0;
            for (unsigned lane = 0; lane < 3; lane++) {
                row_spikes += r->tile[lane] == D100_SPIKE;
                row_cracks += r->tile[lane] == D100_CRACK;
            }
            assert(row_spikes <= 1 && row_cracks <= 1 && hazards <= 2);
            spikes[stage] += row_spikes; cracks[stage] += row_cracks;
            if (row_spikes) {
                const unsigned max_gap[] = {5, 4, 3, 2, 2};
                assert(r->depth - last_spike[stage] <= max_gap[stage]);
                last_spike[stage] = r->depth;
            }
            combinations[stage] += row_spikes && row_cracks;
            if (hazards) assert(r->depth > 4 && r->depth % 20 != 0);
            if (r->tile[1] == D100_CRACK) assert(stage >= 2 && row_spikes == 1);
        }
        /* Human-paced presses: 200 ms apart, steer to center while falling,
           visit every reachable reward, then leave by the edge exit. */
        if (ticks % 20 == 1 && s.support >= 0) {
            d100_row_t *r = &s.rows[s.support];
            int target = -1;
            for (int lane = 0; lane < 3; lane++) if (r->tile[lane] == D100_HOLE) target = lane;
            for (int lane = 0; lane < 3; lane++)
                if ((r->tile[lane] == D100_GEM || r->tile[lane] == D100_HEAL) && !(r->used & (1U << lane))) target = lane;
            assert(target >= 0 && target != s.lane);
            assert(d100_move(&s, target > s.lane ? 1 : -1));
        } else if (ticks % 20 == 1 && s.support < 0 && s.lane != 1)
            assert(d100_move(&s, s.lane < 1 ? 1 : -1));
        d100_tick(&s, 10);
        assert(s.lane >= 0 && s.lane < 3);
        assert(s.support < D100_ROWS && s.support >= -1);
        if (s.page == D100_PLAY) assert(s.feet > 25000 && s.feet <= 104000);
    }
    assert(s.page == D100_RESULT && s.floor == 100);
    assert(s.health == (mode ? 1 : 3));
    assert(checked_depth == 100 && s.gems == expected_gems);
    assert(expected_gems >= 21 && expected_gems <= 31 && supplies == 5);
    if (expected_gems < min_gems) min_gems = expected_gems;
    if (expected_gems > max_gems) max_gems = expected_gems;
    const unsigned expected_spikes[] = {3, 6, 9, 12, 14};
    const unsigned expected_cracks[] = {0, 3, 6, 10, 14};
    const unsigned expected_combinations[] = {0, 0, 3, 6, 10};
    for (unsigned stage = 0; stage < 5; stage++) {
        assert(spikes[stage] == expected_spikes[stage]);
        assert(cracks[stage] == expected_cracks[stage]);
        assert(combinations[stage] == expected_combinations[stage]);
    }
    assert(s.score == 2280 + (mode ? 50 : 150) + s.gems * 30);
    assert(s.max_combo == 100 && s.new_best && s.best[mode] == s.score);
    uint16_t score = s.score; d100_tick(&s, 1000); assert(s.score == score);
}
int main(void)
{
    d100_state_t s = {0}, copy;
    d100_start(&s, 9999); copy = s;
    d100_pause(&s); run_ms(&s, 1000); assert(s.active_ms == 0 && s.feet == copy.feet);
    assert(!d100_move(&s, -1)); d100_pause(&s); assert(s.page == D100_PLAY);
    d100_tick(&s, 1000000); assert(s.active_ms == 100); /* Clamp invisible catch-up. */
    d100_start(&s, 1); run_ms(&s, 3000);
    assert(s.page == D100_RESULT && s.notice == D100_CEILING && s.floor == 0);
    fixture(&s, D100_GEM); assert(d100_move(&s, 1));
    assert(s.gems == 1 && s.score == 30); d100_move(&s, -1); d100_move(&s, 1);
    assert(s.score == 30 && s.gems == 1); /* Cannot farm the same gem. */
    s.rows[0].tile[0] = D100_GEM;
    d100_move(&s, -1); d100_move(&s, -1);
    assert(s.gems == 2 && s.score == 60 && (s.rows[0].used & 5U) == 5U);
    fixture(&s, D100_SPIKE); d100_move(&s, 1); assert(s.health == 2 && s.immune_ms == 1000);
    run_ms(&s, 1100); d100_move(&s, -1); d100_move(&s, 1); assert(s.health == 2);
    s.mode = 1; d100_start(&s, 1); s.rows[0].tile[2] = D100_SPIKE;
    d100_move(&s, 1); assert(s.page == D100_RESULT && !s.health);
    fixture(&s, D100_HEAL); s.health = 1;
    s.rows[0].tile[1] = D100_HEAL;
    d100_move(&s, 1); assert(s.health == 2);
    assert(!(s.rows[0].used & 2U)); /* Neighbor supply remains unclaimed. */
    d100_move(&s, -1); assert(s.health == 3); /* Each pack restores a heart. */
    s.health = 1; d100_move(&s, 1); d100_move(&s, -1); assert(s.health == 1);
    fixture(&s, D100_CRACK); d100_move(&s, 1); run_ms(&s, 990);
    assert(s.support == 0); run_ms(&s, 10);
    assert(s.support == -1 && s.rows[0].tile[2] == D100_HOLE);
    /* Cracks keep collapsing after leaving, and cannot be reset by re-entry. */
    fixture(&s, D100_CRACK); d100_move(&s, 1); run_ms(&s, 200); d100_move(&s, -1);
    s.rows[0].tile[0] = D100_CRACK; d100_move(&s, -1);
    run_ms(&s, 800); assert(s.rows[0].tile[2] == D100_HOLE);
    assert(s.rows[0].tile[0] == D100_CRACK); /* Independent collapse timers. */
    run_ms(&s, 200); assert(s.rows[0].tile[0] == D100_HOLE);
    for (unsigned mode = 0; mode < 2; mode++) {
        s.mode = mode; s.floor = 0; int previous = d100_speed(&s);
        assert(previous == (mode ? 20 : 16));
        for (unsigned floor = 1; floor <= 120; floor++) {
            s.floor = floor; int speed = d100_speed(&s);
            assert(speed >= previous && speed <= previous + 2 && speed <= (mode ? 74 : 70));
            previous = speed;
        }
    }
    const unsigned delays[] = {1000, 900, 750, 650, 550};
    for (unsigned stage = 0; stage < 5; stage++) {
        assert(d100_stage(stage * 20 + 1) == stage);
        assert(d100_stage(stage * 20 + 20) == stage);
        assert(d100_crack_delay(stage * 20 + 1) == delays[stage]);
        fixture(&s, D100_CRACK); s.rows[0].depth = stage * 20 + 1;
        d100_move(&s, 1); run_ms(&s, delays[stage] - 10);
        assert(s.rows[0].tile[2] == D100_CRACK);
        run_ms(&s, 10); assert(s.rows[0].tile[2] == D100_HOLE);
    }
    s.mode = 0; s.floor = 60; assert(d100_speed(&s) == 46);
    s.floor = 80; assert(d100_speed(&s) == 58);
    s.floor = 100; assert(d100_speed(&s) == 70);
    d100_start(&s, 3); assert(d100_move(&s, -1)); assert(!d100_move(&s, -1));
    assert(!d100_move(&s, 0) && !d100_move(&s, 3));
    /* Timing chunking and same-map retry produce identical simulation states. */
    d100_start(&s, 812); copy = s;
    d100_move(&s, 1); d100_move(&copy, 1);
    for (int i = 0; i < 20; i++) { d100_tick(&s, 20); d100_tick(&copy, 7); d100_tick(&copy, 13); }
    assert(memcmp(&s, &copy, sizeof(s)) == 0);
    s.best[0] = 5000; d100_start(&s, 812); d100_start(&copy, 812);
    assert(memcmp(s.rows, copy.rows, sizeof(s.rows)) == 0 && s.best[0] == 5000);
    /* Exhaust every public map number in both modes, including the last floor. */
    for (unsigned seed = 0; seed < 10000; seed++) for (unsigned mode = 0; mode < 2; mode++) safe_route(seed, mode);
    printf("Down 100: 20,000 all-reward routes PASS; spikes 3/6/9/12/14, combinations 0/0/3/6/10; gems %u–%u\n", min_gems, max_gems);
    return 0;
}
