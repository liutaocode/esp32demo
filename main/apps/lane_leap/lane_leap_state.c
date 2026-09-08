#include "lane_leap_state.h"
#include <string.h>

static uint32_t random_next(ll_state_t *s)
{
    uint32_t x = s->random;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return s->random = x;
}
static unsigned dec(unsigned n, unsigned amount) { return n > amount ? n - amount : 0; }
static void points(ll_state_t *s, unsigned n)
{
    s->score = s->score > 999999 - n ? 999999 : s->score + n;
}
unsigned ll_level(const ll_state_t *s)
{
    unsigned level = 1 + s->elapsed_ms / 18000;
    return level > 6 ? 6 : level;
}
unsigned ll_multiplier(const ll_state_t *s)
{
    unsigned value = 1 + s->streak / 5;
    return value > 4 ? 4 : value;
}
float ll_height(const ll_state_t *s)
{
    if (!s->jump_ms) return 0;
    float t = (float)s->jump_ms / LL_JUMP_MS;
    return 4.0f * 30.0f * t * (1.0f - t);
}
void ll_home(ll_state_t *s)
{
    unsigned a = s->best[0], b = s->best[1], mode = s->mode & 1;
    memset(s, 0, sizeof(*s));
    s->best[0] = a; s->best[1] = b; s->mode = mode; s->page = LL_HOME;
}
void ll_start(ll_state_t *s, uint32_t seed)
{
    ll_home(s);
    s->seed = seed ? seed : 1; s->random = s->seed;
    s->page = LL_READY; s->countdown_ms = 1500; s->lane = 1; s->hp = 3;
    s->spawn_ms = 300;
}
void ll_move(ll_state_t *s, int direction)
{
    if (s->page != LL_RACING) return;
    if (direction < 0 && s->lane > 0) s->lane--;
    else if (direction > 0 && s->lane < 2) s->lane++;
}
bool ll_jump(ll_state_t *s)
{
    if (s->page != LL_RACING || s->jump_ms || s->cooldown_ms) return false;
    s->jump_ms = LL_JUMP_MS;
    return true;
}
void ll_pause(ll_state_t *s)
{
    if (s->page == LL_RACING || s->page == LL_READY) s->page = LL_PAUSED;
    else if (s->page == LL_PAUSED) { s->page = LL_READY; s->countdown_ms = 900; }
    s->accumulator_ms = 0;
}
static void spawn(ll_state_t *s)
{
    ll_row_t *r = NULL;
    for (unsigned i = 0; i < LL_ROWS; i++) if (!s->row[i].active) { r = &s->row[i]; break; }
    if (!r) return;
    memset(r, 0, sizeof(*r)); r->active = true; r->y = -20;
    unsigned safe = random_next(s) % 3, pattern = random_next(s) % 6;
    /* Every row has a ground-safe lane. Rows are >= 1050ms apart, wider
     * than a complete jump plus recovery; no forced consecutive leaps. */
    for (unsigned lane = 0; lane < LL_LANES; lane++) {
        if (lane == safe) r->kind[lane] = LL_COIN;
        else if (pattern == 0) r->kind[lane] = LL_AIR_COIN;
        else if (pattern == 1) r->kind[lane] = LL_BARRIER;
        else if (pattern == 2) r->kind[lane] = lane == (safe + 1) % 3 ? LL_TRUCK : LL_AIR_COIN;
        else if (pattern == 3 && s->elapsed_ms >= 18000) r->kind[lane] = LL_GAP;
        else r->kind[lane] = lane == (safe + 1) % 3 ? LL_TRUCK : LL_BARRIER;
    }
    /* The opening row teaches a visible, optional jump without a truck. */
    if (s->elapsed_ms < 1000) { r->kind[0] = LL_BARRIER; r->kind[1] = LL_COIN; r->kind[2] = LL_AIR_COIN; }
}
static void step(ll_state_t *s)
{
    s->elapsed_ms += LL_STEP_MS;
    s->notice_ms = dec(s->notice_ms, LL_STEP_MS);
    s->invincible_ms = dec(s->invincible_ms, LL_STEP_MS);
    s->cooldown_ms = dec(s->cooldown_ms, LL_STEP_MS);
    if (s->jump_ms) {
        s->jump_ms = dec(s->jump_ms, LL_STEP_MS);
        if (!s->jump_ms) s->cooldown_ms = 100;
    }
    float speed = (s->mode ? 74.0f : 58.0f) + (ll_level(s) - 1) * 7.0f;
    float dy = speed * LL_STEP_MS / 1000.0f;
    s->distance += dy / 4.0f;
    s->spawn_ms = dec(s->spawn_ms, LL_STEP_MS);
    if (!s->spawn_ms) {
        spawn(s);
        s->spawn_ms = (s->mode ? 1300 : 1500) - (ll_level(s) - 1) * 50;
    }
    float height = ll_height(s);
    for (unsigned i = 0; i < LL_ROWS; i++) {
        ll_row_t *r = &s->row[i];
        if (!r->active) continue;
        r->y += dy;
        ll_kind_t kind = r->kind[s->lane];
        /* Collision uses ground position, never the lifted sprite's y.
         * Test the entire overlap, including midair lane changes/landing. */
        float half = kind == LL_TRUCK ? 23.0f : 14.0f;
        if (r->y >= LL_PLAYER_Y - half && r->y <= LL_PLAYER_Y + half) {
            if ((kind == LL_COIN || (kind == LL_AIR_COIN && height >= 10)) && !r->taken[s->lane]) {
                r->taken[s->lane] = true; s->coins++;
                points(s, (kind == LL_AIR_COIN ? 30 : 10) * ll_multiplier(s));
                s->notice = kind == LL_AIR_COIN ? 2 : 1; s->notice_ms = 500;
            }
            bool low = kind == LL_BARRIER || kind == LL_GAP;
            if (low && height >= 9) r->jumped = true;
            if ((kind == LL_TRUCK || (low && height < 9)) && !r->hurt && !s->invincible_ms) {
                r->hurt = true; s->hp--; s->streak = 0; s->invincible_ms = 1200;
                s->notice = 4; s->notice_ms = 900;
                if (!s->hp) {
                    s->page = LL_RESULT; s->result_ms = 0;
                    s->new_best = s->score > s->best[s->mode];
                    if (s->new_best) s->best[s->mode] = s->score;
                    return;
                }
            }
        }
        if (!r->passed && r->y > LL_PLAYER_Y + 24) {
            r->passed = true; s->cleared++;
            if (!r->hurt) {
                s->streak++;
                if (s->streak > s->best_streak) s->best_streak = s->streak;
                points(s, 5 * ll_multiplier(s));
                if (r->jumped) { s->jumps++; points(s, 20 * ll_multiplier(s)); s->notice = 3; s->notice_ms = 600; }
            }
        }
        if (r->y > LL_HEIGHT + 26) r->active = false;
    }
}
void ll_tick(ll_state_t *s, unsigned ms)
{
    if (s->page == LL_RESULT) { if (s->result_ms < 1000) s->result_ms += ms > 100 ? 100 : ms; return; }
    /* Bounded simulation catch-up protects against LCD/USB stalls. */
    if (ms > 100) ms = 100;
    if (s->page == LL_READY) {
        s->countdown_ms = dec(s->countdown_ms, ms);
        if (!s->countdown_ms) s->page = LL_RACING;
        return;
    }
    if (s->page != LL_RACING) return;
    s->accumulator_ms += ms;
    while (s->accumulator_ms >= LL_STEP_MS && s->page == LL_RACING) {
        s->accumulator_ms -= LL_STEP_MS; step(s);
    }
}
