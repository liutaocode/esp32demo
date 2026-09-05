#include "down_100_state.h"
#include <string.h>

static uint32_t mix(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    return x ^ (x >> 16);
}
int d100_lane_x(int lane) { return 35 + lane * 64; }
unsigned d100_stage(unsigned depth) { return depth ? (depth > 100 ? 99 : depth - 1) / 20 : 0; }
unsigned d100_crack_delay(unsigned depth)
{
    static const unsigned delays[] = {1000, 900, 750, 650, 550};
    return delays[d100_stage(depth)];
}
int d100_speed(const d100_state_t *s)
{
    unsigned floor = s->floor > D100_GOAL ? D100_GOAL : s->floor;
    static const unsigned speeds[] = {16, 24, 34, 46, 58, 70};
    unsigned stage = floor == 100 ? 4 : floor / 20;
    return speeds[stage] + (speeds[stage + 1] - speeds[stage]) * (floor - stage * 20) / 20 + (s->mode ? 4 : 0);
}
/* Evenly spread spikes instead of clustering them in a random corner of the
   stage. Seeded rotation/direction varies order while keeping exact quotas.
   The first four floors stay clear. */
static unsigned obstacle_rank(uint32_t seed, unsigned stage, unsigned depth)
{
    static const unsigned strides[] = {3, 6, 9, 12, 14};
    uint32_t pattern = mix(seed + stage * 3571U);
    unsigned count = stage ? 19 : 15;
    unsigned step = pattern & 1 ? strides[stage] : count - strides[stage];
    unsigned slot = stage ? (depth - 1) % 20 : depth - 5;
    return (slot * step + (pattern >> 8) % count) % count;
}
static void notice(d100_state_t *s, d100_notice_t n)
{
    s->notice = n; s->notice_ms = 1100;
}
static void finish(d100_state_t *s, bool clear)
{
    s->page = D100_RESULT;
    if (clear) { s->score += 300 + s->health * 50; notice(s, D100_CLEAR); }
    s->new_best = s->score > s->best[s->mode];
    if (s->new_best) s->best[s->mode] = s->score;
}
static void generate(d100_state_t *s, d100_row_t *r, unsigned depth, int32_t y)
{
    memset(r, 0, sizeof(*r));
    r->depth = depth; r->y = y;
    uint32_t random = mix(s->seed + depth * 7919U);
    /* Center landings never have spikes, but late stages may make them
       brittle. Every landing retains a one-press escape to an edge exit. */
    unsigned stage = d100_stage(depth);
    unsigned hole = 2 - s->previous_hole;
    if (depth > 20 && random % 100 < 10 + stage * 8) hole = s->previous_hole;
    unsigned extra = 2 - hole;
    r->tile[1] = D100_SOLID;
    r->tile[hole] = D100_HOLE;
    r->tile[extra] = D100_SOLID;
    static const unsigned spike_count[] = {3, 6, 9, 12, 14};
    static const unsigned crack_count[] = {0, 3, 3, 4, 4};
    static const unsigned combined_count[] = {0, 0, 3, 6, 10};
    if (depth > 4 && depth % 20 != 0) {
        unsigned rank = obstacle_rank(s->seed, stage, depth);
        if (rank < spike_count[stage]) {
            r->tile[extra] = D100_SPIKE;
            if (rank < combined_count[stage]) r->tile[1] = D100_CRACK;
        } else if (rank < spike_count[stage] + crack_count[stage]) r->tile[extra] = D100_CRACK;
    }
    /* Rewards stay sparse and never replace an obstacle or require damage. */
    if (depth > 4 && depth % 3 == 2) {
        unsigned lane = (random >> 5) % 2 ? 1 : extra;
        if (r->tile[lane] != D100_SOLID) lane = lane == 1 ? extra : 1;
        if (r->tile[lane] == D100_SOLID) r->tile[lane] = D100_GEM;
        if (depth % 15 == 5 && random % 2 == 0)
            for (unsigned i = 0; i < 3; i++) if (r->tile[i] == D100_SOLID) r->tile[i] = D100_GEM;
    }
    if (depth % 20 == 0) { r->tile[1] = D100_HEAL; r->tile[extra] = D100_SOLID; }
    if (depth == D100_GOAL) r->tile[hole] = D100_SOLID;
    s->previous_hole = hole;
}
void d100_start(d100_state_t *s, uint16_t challenge)
{
    uint16_t best0 = s->best[0], best1 = s->best[1];
    uint8_t mode = s->mode == 1;
    memset(s, 0, sizeof(*s));
    s->best[0] = best0; s->best[1] = best1; s->mode = mode;
    s->challenge = challenge % 10000; s->seed = mix(s->challenge + 1U);
    s->page = D100_PLAY; s->health = mode ? 1 : 3;
    s->lane = 1; s->support = 0; s->feet = 66000;
    s->rows[0] = (d100_row_t){.y = s->feet, .depth = 0, .tile = {D100_SOLID, D100_SOLID, D100_HOLE}};
    s->previous_hole = 2;
    for (unsigned i = 1; i < D100_ROWS; i++) generate(s, &s->rows[i], i, s->feet + i * D100_GAP * 1000);
    s->next_depth = D100_ROWS;
    notice(s, D100_READY);
}
static void touch(d100_state_t *s)
{
    if (s->support < 0 || s->page != D100_PLAY) return;
    d100_row_t *r = &s->rows[s->support];
    unsigned tile = r->tile[s->lane];
    if (tile == D100_HOLE) { s->support = -1; s->vy = 0; return; }
    if (r->depth > s->floor) {
        unsigned delta = r->depth - s->floor;
        s->combo = s->floor && s->active_ms - s->last_land_ms <= 2000 ? s->combo + 1 : 1;
        if (s->combo > s->max_combo) s->max_combo = s->combo;
        s->score += delta * 10 + (s->combo > 5 ? 5 : s->combo) * 2;
        s->floor = r->depth; s->last_land_ms = s->active_ms;
        notice(s, s->combo >= 3 ? D100_COMBO : D100_LAND);
    }
    if (!(r->used & (1U << s->lane))) {
        r->used |= 1U << s->lane;
        if (tile == D100_GEM) { s->gems++; s->score += 30; notice(s, D100_TREASURE); }
        else if (tile == D100_HEAL) {
            s->supplies++;
            if (!s->mode && s->health < 3) s->health++;
            notice(s, D100_SUPPLY);
        } else if (tile == D100_SPIKE && !s->immune_ms) {
            s->health--; s->combo = 0; s->immune_ms = 1000; notice(s, D100_HURT);
            if (!s->health) { finish(s, false); return; }
        } else if (tile == D100_CRACK) { r->crack_ms[s->lane] = 1; notice(s, D100_BREAKING); }
    }
    if (s->floor >= D100_GOAL) finish(s, true);
}
bool d100_move(d100_state_t *s, int direction)
{
    if (s->page != D100_PLAY || (direction != -1 && direction != 1)) return false;
    int lane = s->lane + direction;
    if (lane < 0 || lane > 2) return false;
    s->lane = lane;
    touch(s);
    return true;
}
void d100_pause(d100_state_t *s)
{
    if (s->page == D100_PLAY) s->page = D100_PAUSE;
    else if (s->page == D100_PAUSE) s->page = D100_PLAY;
}
static void step(d100_state_t *s)
{
    s->active_ms += D100_STEP_MS;
    if (s->notice_ms > D100_STEP_MS) s->notice_ms -= D100_STEP_MS;
    else s->notice_ms = 0;
    if (s->immune_ms > D100_STEP_MS) s->immune_ms -= D100_STEP_MS;
    else s->immune_ms = 0;
    for (int i = 0; i < D100_ROWS; i++) {
        d100_row_t *r = &s->rows[i];
        for (int lane = 0; lane < 3; lane++) if (r->crack_ms[lane]) {
            r->crack_ms[lane] += D100_STEP_MS;
            if (r->crack_ms[lane] >= d100_crack_delay(r->depth)) {
                r->crack_ms[lane] = 0;
                r->tile[lane] = D100_HOLE;
                if (s->support == i) touch(s);
            }
        }
    }
    if (s->support >= 0) s->feet = s->rows[s->support].y;
    else {
        int32_t old = s->feet;
        s->vy += 6500;
        if (s->vy > 210000) s->vy = 210000;
        s->feet += s->vy / 100;
        int hit = -1;
        for (int i = 0; i < D100_ROWS; i++) {
            d100_row_t *r = &s->rows[i];
            if (r->tile[s->lane] != D100_HOLE && old <= r->y && s->feet >= r->y &&
                (hit < 0 || r->y < s->rows[hit].y)) hit = i;
        }
        if (hit >= 0) { s->support = hit; s->feet = s->rows[hit].y; s->vy = 0; touch(s); }
    }
    if (s->page != D100_PLAY) return;
    int32_t scroll = d100_speed(s) * D100_STEP_MS;
    /* Follow quick descents; always reveal the next landing before reaching it. */
    if (s->feet - scroll > 104000) scroll = s->feet - 104000;
    s->feet -= scroll;
    for (int i = 0; i < D100_ROWS; i++) s->rows[i].y -= scroll;
    if (s->feet <= 25000) { notice(s, D100_CEILING); finish(s, false); return; }
    for (int i = 0; i < D100_ROWS; i++) {
        if (s->rows[i].y >= -12000 || s->next_depth > D100_GOAL) continue;
        int32_t bottom = 0;
        for (int j = 0; j < D100_ROWS; j++) if (s->rows[j].y > bottom) bottom = s->rows[j].y;
        generate(s, &s->rows[i], s->next_depth++, bottom + D100_GAP * 1000);
    }
}
void d100_tick(d100_state_t *s, uint32_t elapsed_ms)
{
    if (s->page != D100_PLAY) return;
    /* A stalled renderer must not simulate unseen seconds of danger. */
    if (elapsed_ms > 100) elapsed_ms = 100;
    s->remainder_ms += elapsed_ms;
    while (s->remainder_ms >= D100_STEP_MS && s->page == D100_PLAY) {
        s->remainder_ms -= D100_STEP_MS; step(s);
    }
}
