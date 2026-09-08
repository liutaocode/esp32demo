#include "lane_leap_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void start(ll_state_t *s, unsigned seed)
{
    ll_start(s, seed);
    for (unsigned i = 0; i < 75; i++) ll_tick(s, 20);
    assert(s->page == LL_RACING && s->hp == 3 && s->lane == 1);
}
static void fixture(ll_state_t *s, ll_kind_t kind, float y)
{
    memset(s->row, 0, sizeof(s->row));
    s->spawn_ms = 10000;
    s->row[0].active = true; s->row[0].y = y; s->row[0].kind[s->lane] = kind;
}
static void advance(ll_state_t *s, unsigned ms) { for (unsigned i = 0; i < ms / 20; i++) ll_tick(s, 20); }
int main(void)
{
    ll_state_t s = {0};
    start(&s, 7);
    ll_move(&s, -1); ll_move(&s, -1); assert(s.lane == 0);
    assert(ll_jump(&s)); assert(!ll_jump(&s)); advance(&s, 200);
    ll_move(&s, 1); assert(s.lane == 1 && ll_height(&s) > 20);
    ll_pause(&s); ll_state_t frozen = s; advance(&s, 2000);
    assert(!memcmp(&s, &frozen, sizeof(s)));
    ll_move(&s, 1); assert(s.lane == 1); assert(!ll_jump(&s));
    ll_pause(&s); assert(s.page == LL_READY); advance(&s, 900);
    assert(s.page == LL_RACING && s.jump_ms == frozen.jump_ms);

    start(&s, 11); fixture(&s, LL_TRUCK, LL_PLAYER_Y);
    s.jump_ms = LL_JUMP_MS / 2; ll_tick(&s, 20); assert(s.hp == 2);
    advance(&s, 300); assert(s.hp == 2);
    start(&s, 11); fixture(&s, LL_BARRIER, LL_PLAYER_Y); ll_tick(&s, 20); assert(s.hp == 2);
    start(&s, 11); fixture(&s, LL_GAP, LL_PLAYER_Y); ll_tick(&s, 20); assert(s.hp == 2);
    /* Correctly timed leap clears the whole contact window at each speed. */
    for (unsigned mode = 0; mode < 2; mode++) for (unsigned level = 1; level <= 6; level++) {
        s.mode = mode; start(&s, 99); s.elapsed_ms = (level - 1) * 18000;
        float speed = (mode ? 74.0f : 58.0f) + (level - 1) * 7.0f;
        fixture(&s, LL_BARRIER, LL_PLAYER_Y - speed * 0.38f);
        assert(ll_jump(&s)); advance(&s, 900);
        assert(s.hp == 3 && s.jumps == 1);
    }
    /* Landing during contact is a collision, even after clearing its front. */
    start(&s, 9); fixture(&s, LL_GAP, LL_PLAYER_Y);
    s.jump_ms = 60; ll_tick(&s, 20); assert(s.hp == 2);
    start(&s, 9); fixture(&s, LL_AIR_COIN, LL_PLAYER_Y);
    ll_tick(&s, 20); assert(s.coins == 0);
    s.jump_ms = 380; ll_tick(&s, 20); assert(s.coins == 1 && s.score == 30);
    advance(&s, 40); assert(s.coins == 1);
    /* Lane change into the side of a truck must still collide. */
    start(&s, 5); fixture(&s, LL_EMPTY, LL_PLAYER_Y + 5);
    s.row[0].kind[0] = LL_TRUCK; ll_move(&s, -1); ll_tick(&s, 20); assert(s.hp == 2);
    s.invincible_ms = 0; s.hp = 1; s.row[0].hurt = false; s.score = 123;
    ll_tick(&s, 20); assert(s.page == LL_RESULT && s.best[s.mode] == 123 && s.new_best);
    unsigned best = s.best[s.mode]; ll_home(&s); assert(s.best[s.mode] == best);

    /* Frame batching remains deterministic; oversized stalls are bounded. */
    ll_state_t a = {0}, b = {0}; start(&a, 117); start(&b, 117);
    for (unsigned i = 0; i < 20; i++) ll_tick(&a, 20);
    for (unsigned i = 0; i < 4; i++) ll_tick(&b, 100);
    assert(!memcmp(&a, &b, sizeof(a)));
    unsigned elapsed = a.elapsed_ms; ll_tick(&a, 8000); assert(a.elapsed_ms == elapsed + 100);

    /* Ground-only solver: every generated route is survivable at max speed,
     * with at most two lane changes between rows. Jumping is always optional. */
    for (unsigned seed = 1; seed <= 200; seed++) {
        ll_state_t run = {0}; run.mode = seed % 2; start(&run, seed);
        run.elapsed_ms = 90000;
        for (unsigned t = 0; t < 6000; t++) {
            ll_row_t *next = NULL;
            for (unsigned r = 0; r < LL_ROWS; r++) {
                ll_row_t *row = &run.row[r];
                if (!row->active) continue;
                unsigned safe = 0;
                for (unsigned l = 0; l < 3; l++) if (row->kind[l] <= LL_AIR_COIN) safe++;
                assert(safe > 0);
                if (row->y < LL_PLAYER_Y + 24 && (!next || row->y > next->y)) next = row;
            }
            if (next) {
                for (unsigned l = 0; l < 3; l++) if (next->kind[l] <= LL_AIR_COIN) {
                    if (run.lane != l) ll_move(&run, run.lane < l ? 1 : -1);
                    break;
                }
            }
            ll_tick(&run, 20);
            assert(run.page == LL_RACING && run.hp == 3);
        }
        assert(run.cleared > 80 && run.best_streak > 80 && ll_multiplier(&run) == 4);
    }
    puts("Lane Leap state: collisions, jumps, pause, deterministic time, records, 200 fair routes PASS");
}
