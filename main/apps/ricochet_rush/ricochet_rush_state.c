#include "ricochet_rush_state.h"
#include <math.h>
#include <string.h>

#define RADIUS 2.0f
#define FLOOR 166.0f
#define SPEED 155.0f
static uint32_t random_next(rr_state_t *s)
{
    uint32_t x = s->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return s->rng = x;
}
static float clamp(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
void rr_rect(unsigned i, float *x, float *y, float *w, float *h)
{
    *x = (i % RR_COLS) * 33 + 3; *y = (i / RR_COLS) * 22 + 4;
    *w = 27; *h = 18;
}
static void row(rr_state_t *s)
{
    unsigned order[RR_COLS] = {0,1,2,3,4,5};
    for (unsigned i = RR_COLS - 1; i; i--) {
        unsigned j = random_next(s) % (i + 1), tmp = order[i];
        order[i] = order[j]; order[j] = tmp;
    }
    unsigned count = 2 + (s->round >= 8) + (s->mode && s->round >= 16);
    for (unsigned i = 0; i < count; i++)
        s->hp[order[i]] = 1 + s->round / 2 + random_next(s) % (2 + s->round / 5);
    /* Every row offers one ball; spare columns always leave a route upward. */
    s->pickup[order[count]] = true;
}
void rr_home(rr_state_t *s) { s->page = RR_HOME; s->mode &= 1; }
void rr_start(rr_state_t *s, uint32_t seed)
{
    unsigned mode = s->mode & 1, a = s->best[0], b = s->best[1];
    memset(s, 0, sizeof(*s));
    s->mode = mode; s->best[0] = a; s->best[1] = b;
    s->seed = s->rng = seed ? seed : 0x91e10da5U;
    s->round = 1; s->balls = mode ? 4 : 6; s->launch_x = 99;
    s->angle = -42; s->direction = 1; s->page = RR_AIM;
    row(s);
    /* First row starts close enough for the first shot to feel immediate. */
    for (unsigned i = 0; i < RR_COLS; i++) {
        s->hp[i + 2 * RR_COLS] = s->hp[i]; s->hp[i] = 0;
        s->pickup[i + 2 * RR_COLS] = s->pickup[i]; s->pickup[i] = false;
    }
}
static void velocity(float angle, float *vx, float *vy)
{
    float radians = angle * 0.01745329252f;
    *vx = sinf(radians) * SPEED; *vy = -cosf(radians) * SPEED;
}
bool rr_fire(rr_state_t *s)
{
    if (s->page != RR_AIM) return false;
    memset(s->ball, 0, sizeof(s->ball));
    s->page = RR_FLIGHT; s->launched = s->returned = s->gained = s->hits = 0;
    s->flight_ms = s->remainder = 0; s->first_return = s->recalled = s->fast = false;
    return true;
}
void rr_pause(rr_state_t *s)
{
    if (s->page == RR_PAUSED) s->page = s->resume;
    else if (s->page == RR_AIM || s->page == RR_FLIGHT || s->page == RR_SETTLE) {
        s->resume = s->page; s->page = RR_PAUSED;
    }
}
void rr_reverse(rr_state_t *s)
{
    if (s->page == RR_AIM) s->direction = -s->direction;
    else if (s->page == RR_FLIGHT) s->fast = !s->fast;
}
static void finish(rr_state_t *s, bool won)
{
    s->page = RR_RESULT; s->won = won;
    s->new_best = s->score > s->best[s->mode];
    if (s->new_best) s->best[s->mode] = s->score;
}
static void next_round(rr_state_t *s)
{
    for (unsigned i = RR_CELLS - RR_COLS; i < RR_CELLS; i++)
        if (s->hp[i]) { finish(s, false); return; }
    if (s->round == RR_ROUNDS) { finish(s, true); return; }
    for (unsigned i = RR_CELLS; i-- > RR_COLS;) {
        s->hp[i] = s->hp[i - RR_COLS]; s->pickup[i] = s->pickup[i - RR_COLS];
    }
    memset(s->hp, 0, RR_COLS * sizeof(*s->hp));
    memset(s->pickup, 0, RR_COLS * sizeof(*s->pickup));
    memset(s->flash, 0, sizeof(s->flash));
    s->round++; row(s); s->page = RR_AIM; s->fast = false;
    s->angle = s->launch_x < 99 ? 20 : -20;
    s->direction = s->launch_x < 99 ? 1 : -1;
}
static bool overlaps(float x, float y, unsigned i)
{
    float bx, by, w, h; rr_rect(i, &bx, &by, &w, &h);
    return x > bx - RADIUS && x < bx + w + RADIUS &&
           y > by - RADIUS && y < by + h + RADIUS;
}
static void move_ball(rr_state_t *s, rr_ball_t *b)
{
    float ox = b->x, oy = b->y;
    b->x += b->vx * .005f; b->y += b->vy * .005f;
    if (b->x < RADIUS) { b->x = 2 * RADIUS - b->x; b->vx = fabsf(b->vx); }
    if (b->x > RR_WIDTH - RADIUS) {
        b->x = 2 * (RR_WIDTH - RADIUS) - b->x; b->vx = -fabsf(b->vx);
    }
    if (b->y < RADIUS) { b->y = 2 * RADIUS - b->y; b->vy = fabsf(b->vy); }
    if (b->y >= FLOOR && b->vy > 0) {
        b->active = false; s->returned++;
        if (!s->first_return) { s->next_x = clamp(b->x, 8, 190); s->first_return = true; }
        return;
    }
    /* At most four nearby grid cells; bounded substeps cannot skip a tile. */
    int c0 = (int)floorf((b->x - 5) / 33), c1 = (int)((b->x + 2) / 33);
    int r0 = (int)floorf((b->y - 6) / 22), r1 = (int)((b->y + 2) / 22);
    for (int r = r0; r <= r1; r++) for (int c = c0; c <= c1; c++) {
        if (r < 0 || r >= RR_ROWS || c < 0 || c >= RR_COLS) continue;
        unsigned i = (unsigned)(r * RR_COLS + c);
        if (s->pickup[i]) {
            float dx = b->x - (c * 33 + 16.5f), dy = b->y - (r * 22 + 13);
            if (dx * dx + dy * dy <= 64) { s->pickup[i] = false; if (s->balls + s->gained < RR_BALLS) s->gained++; }
        }
        if (!s->hp[i] || !overlaps(b->x, b->y, i)) continue;
        float x, y, w, h; rr_rect(i, &x, &y, &w, &h);
        bool side = ox <= x - RADIUS || ox >= x + w + RADIUS;
        bool vertical = oy <= y - RADIUS || oy >= y + h + RADIUS;
        if (side) b->vx = -b->vx;
        if (vertical || !side) b->vy = -b->vy;
        b->x = ox; b->y = oy;
        s->hp[i]--; s->hits++; s->score++; s->flash[i] = 20;
        if (!s->hp[i]) { s->cleared++; s->score += 10; }
        if (s->score > 99999) s->score = 99999;
        return; /* One collision per ball/substep prevents duplicate corner damage. */
    }
}
static void step(rr_state_t *s)
{
    for (unsigned i = 0; i < RR_CELLS; i++) if (s->flash[i]) s->flash[i]--;
    if (s->page == RR_AIM) {
        s->angle += s->direction * (s->mode ? .20f : .14f);
        if (s->angle > 65) { s->angle = 130 - s->angle; s->direction = -1; }
        if (s->angle < -65) { s->angle = -130 - s->angle; s->direction = 1; }
        return;
    }
    if (s->page == RR_SETTLE) {
        s->settle_ms += RR_STEP_MS;
        if (s->settle_ms >= 700) next_round(s);
        return;
    }
    if (s->page != RR_FLIGHT) return;
    if (s->launched < s->balls && s->flight_ms >= s->launched * 70) {
        rr_ball_t *b = &s->ball[s->launched++];
        *b = (rr_ball_t){.x = s->launch_x, .y = FLOOR, .active = true};
        velocity(s->angle, &b->vx, &b->vy);
    }
    for (unsigned i = 0; i < s->launched; i++) if (s->ball[i].active) move_ball(s, &s->ball[i]);
    s->flight_ms += RR_STEP_MS;
    if (s->flight_ms >= 18000) {
        for (unsigned i = 0; i < RR_BALLS; i++) s->ball[i].active = false;
        s->returned = s->balls; s->recalled = true;
    }
    if (s->returned == s->balls) {
        s->balls += s->gained;
        if (s->balls > RR_BALLS) s->balls = RR_BALLS;
        if (s->first_return) s->launch_x = s->next_x;
        if (s->hits > s->best_hits) s->best_hits = s->hits;
        s->page = RR_SETTLE; s->settle_ms = 0; s->fast = false;
    }
}
void rr_tick(rr_state_t *s, unsigned ms)
{
    if (s->page != RR_AIM && s->page != RR_FLIGHT && s->page != RR_SETTLE) return;
    if (ms > 50) ms = 50;
    unsigned count = (ms + s->remainder) / RR_STEP_MS;
    s->remainder = (ms + s->remainder) % RR_STEP_MS;
    if (s->fast && s->page == RR_FLIGHT) count *= 3;
    rr_page_t before = s->page;
    while (count--) { step(s); if (s->page != before) break; }
}
unsigned rr_preview(const rr_state_t *s, float *xy, unsigned capacity)
{
    float x = s->launch_x, y = FLOOR, vx, vy;
    velocity(s->angle, &vx, &vy);
    unsigned count = 0;
    for (unsigned n = 0; n < 200 && count < capacity; n++) {
        x += vx * .005f; y += vy * .005f;
        if (x < RADIUS) { x = 2 * RADIUS - x; vx = fabsf(vx); }
        if (x > RR_WIDTH - RADIUS) { x = 2 * (RR_WIDTH - RADIUS) - x; vx = -fabsf(vx); }
        if (y < RADIUS) break;
        int c0 = (int)floorf((x - 5) / 33), c1 = (int)((x + 2) / 33);
        int r0 = (int)floorf((y - 6) / 22), r1 = (int)((y + 2) / 22);
        for (int r = r0; r <= r1; r++) for (int c = c0; c <= c1; c++) {
            if (r < 0 || r >= RR_ROWS || c < 0 || c >= RR_COLS) continue;
            unsigned i = (unsigned)(r * RR_COLS + c);
            if (s->hp[i] && overlaps(x, y, i)) return count;
        }
        if (n % 12 == 0) { xy[count * 2] = x; xy[count * 2 + 1] = y; count++; }
    }
    return count;
}
