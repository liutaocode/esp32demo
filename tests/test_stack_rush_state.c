#include "stack_rush_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void land(stack_rush_state_t *s, int delta)
{
    assert(s->page == STACK_MOVING);
    s->moving.x = s->tower[s->count - 1].x + delta;
    assert(stack_rush_drop(s));
}

static void next(stack_rush_state_t *s)
{
    stack_rush_tick(s, STACK_RUSH_SETTLE_MS);
    assert(s->page == STACK_MOVING);
}

static void test_cut_combo_and_miss(void)
{
    stack_rush_state_t s;
    stack_rush_init(&s);
    assert(!stack_rush_drop(&s));
    stack_rush_start(&s);
    land(&s, 10);
    assert(s.tower[1].x == 52 && s.tower[1].width == 98);
    assert(s.debris.x == 150 && s.debris.width == 10);
    assert(!s.perfect && !stack_rush_drop(&s));
    next(&s);
    land(&s, -8);
    assert(s.tower[2].x == 52 && s.tower[2].width == 90);
    assert(s.debris.x == 44 && s.debris.width == 8);
    for (unsigned i = 0; i < 3; i++) {
        next(&s);
        land(&s, i == 1 ? 2 : -2);
        assert(s.perfect && s.streak == i + 1);
    }
    assert(s.restored && s.tower[s.count - 1].width == 98);
    assert(s.tower[s.count - 1].x == 48);
    next(&s);
    land(&s, 3);
    assert(!s.perfect && s.streak == 0 && s.tower[s.count - 1].width == 95);
    next(&s);
    land(&s, 95); /* Edge contact is not an overlap. */
    assert(s.page == STACK_RESULT && s.floors == 6 && s.new_best);
    assert(s.best[0] == 6 && s.best[1] == 0);
    stack_rush_start(&s);
    assert(s.best[0] == 6 && s.floors == 0 && s.streak == 0);
    land(&s, -108);
    assert(s.page == STACK_RESULT && !s.new_best);
}

static void test_timing_pause_and_bounds(void)
{
    stack_rush_state_t a, b;
    stack_rush_init(&a);
    stack_rush_start(&a);
    b = a;
    stack_rush_tick(&a, 1000);
    for (int i = 0; i < 100; i++) stack_rush_tick(&b, 10);
    assert(a.phase == b.phase && a.moving.x == 65);
    stack_rush_pause(&a);
    b = a;
    stack_rush_tick(&a, UINT32_MAX);
    assert(memcmp(&a, &b, sizeof(a)) == 0);
    assert(!stack_rush_drop(&a));
    stack_rush_pause(&a);
    assert(a.page == STACK_MOVING);
    for (unsigned i = 0; i < 1000; i++) {
        stack_rush_tick(&a, UINT32_MAX - i);
        assert(a.moving.x >= 0 && a.moving.x + a.moving.width <= STACK_RUSH_FIELD);
    }
    land(&a, 0);
    stack_rush_tick(&a, 319);
    assert(a.page == STACK_SETTLING);
    stack_rush_pause(&a);
    stack_rush_tick(&a, 5000);
    assert(a.settle_ms == 319);
    stack_rush_pause(&a);
    stack_rush_tick(&a, 1);
    assert(a.page == STACK_MOVING);
    stack_rush_home(&a);
    stack_rush_select(&a);
    assert(a.mode == 1);
    stack_rush_start(&a);
    stack_rush_select(&a);
    assert(a.mode == 1 && stack_rush_speed(&a) == 100);
}

static void test_win_history_and_restore_cap(void)
{
    stack_rush_state_t s;
    stack_rush_init(&s);
    stack_rush_select(&s);
    stack_rush_start(&s);
    land(&s, -3);
    for (int i = 1; i < STACK_RUSH_GOAL; i++) {
        next(&s);
        land(&s, 0);
        assert(s.count <= STACK_RUSH_HISTORY);
        assert(s.tower[s.count - 1].width <= STACK_RUSH_WIDTH);
        if (i == 3) assert(s.restored && s.tower[s.count - 1].width == 108);
    }
    assert(s.won && s.floors == 50 && s.perfects == 49);
    stack_rush_tick(&s, UINT32_MAX);
    assert(s.page == STACK_RESULT && s.best[1] == 50 && s.best[0] == 0);
    assert(stack_rush_speed(&s) == 200);
    assert(!stack_rush_drop(&s));
}

static void test_every_legal_overlap(void)
{
    /* Exhaust the geometry, including one-pixel towers and field boundaries. */
    for (int w = 1; w <= STACK_RUSH_WIDTH; w++) {
        for (int x = 0; x <= STACK_RUSH_FIELD - w; x++) {
            for (int moving = 0; moving <= STACK_RUSH_FIELD - w; moving++) {
                stack_rush_state_t s;
                stack_rush_init(&s);
                stack_rush_start(&s);
                s.tower[0] = (stack_rush_block_t){x, w};
                s.moving = (stack_rush_block_t){moving, w};
                s.streak = 2; /* Exercise recovery at both screen edges. */
                int distance = moving > x ? moving - x : x - moving;
                stack_rush_drop(&s);
                if (distance >= w) assert(s.page == STACK_RESULT);
                else {
                    stack_rush_block_t top = s.tower[s.count - 1];
                    assert(top.x >= 0 && top.x + top.width <= STACK_RUSH_FIELD);
                    assert(top.width > 0 && top.width <= STACK_RUSH_WIDTH);
                    if (distance > 2) {
                        assert(top.width == w - distance);
                        assert(top.width + s.debris.width == w);
                    }
                }
            }
        }
    }
}

int main(void)
{
    test_cut_combo_and_miss();
    test_timing_pause_and_bounds();
    test_win_history_and_restore_cap();
    test_every_legal_overlap();
    puts("Stack Rush: geometry, timing, pause, combos, records and victory PASS");
    return 0;
}
