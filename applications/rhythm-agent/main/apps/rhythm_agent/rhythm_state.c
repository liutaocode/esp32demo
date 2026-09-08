#include "rhythm_state.h"
#include <string.h>
static uint32_t random_next(uint32_t *x) { *x ^= *x << 13; *x ^= *x >> 17; *x ^= *x << 5; return *x; }
void ra_pattern(ra_pattern_t *p, uint32_t seed, unsigned door, ra_mode_t mode) {
    memset(p, 0, sizeof(*p));
    if (door >= RA_DOORS) door = RA_DOORS - 1;
    uint32_t rng = (seed % 10000 + 1) * 2654435761u ^ (door + 1) * 2246822519u ^ (uint32_t)mode;
    if (!rng) rng = 1;
    p->count = mode == RA_TRAIN ? 3 + door / 2 : 4 + (door * 4 / 7);
    unsigned beat = mode == RA_TRAIN ? 650 : 500;
    for (unsigned i = 0; i < p->count; i++) {
        /* Opening training locks teach mapping before adding long rests. */
        p->keys[i] = random_next(&rng) % 3;
        if (i && p->keys[i] == p->keys[i-1] && i > 1 && p->keys[i-1] == p->keys[i-2])
            p->keys[i] = (p->keys[i] + 1) % 3;
        if (i) p->at[i] = p->at[i-1] + beat + ((door >= 2 && random_next(&rng) % 3 == 0) ? beat : 0);
    }
}
static void reset_attempt(ra_game_t *g) {
    g->phase = RA_DEMO; g->index = g->points = g->accuracy = 0;
    g->origin = g->last_press = -1; g->passed = false; g->all_keys = true;
    g->error_ms = 0; g->last_mark = RA_EXACT;
    memset(g->marks, 0, sizeof(g->marks));
}
void ra_start(ra_game_t *g, uint32_t seed, ra_mode_t mode) {
    memset(g, 0, sizeof(*g)); g->seed = seed % 10000; g->mode = mode; g->lives = 3;
    ra_pattern(&g->pattern, g->seed, 0, mode); reset_attempt(g);
}
void ra_ready(ra_game_t *g) { if (g->phase == RA_DEMO) g->phase = RA_READY; }
static void finish(ra_game_t *g) {
    g->accuracy = g->points / g->pattern.count;
    /* Training teaches key memory; timing is feedback, not a progression gate. */
    g->passed = g->all_keys && (g->mode == RA_TRAIN || g->accuracy >= 75u);
    g->attempts++; g->phase = RA_FEEDBACK;
    if (g->passed) g->score += g->accuracy;
    else if (g->mode == RA_CHALLENGE && g->lives) g->lives--;
}
bool ra_press(ra_game_t *g, unsigned key, int64_t ms) {
    if (key > 2 || ms < 0 || (g->phase != RA_READY && g->phase != RA_INPUT)) return false;
    if (g->phase == RA_INPUT) { ra_tick(g, ms); if (g->phase != RA_INPUT) return false; }
    /* Reject duplicate/debounce events; use callback timestamps, never paint time. */
    if (g->last_press >= 0 && ms - g->last_press < 100) return false;
    if (g->phase == RA_READY) { g->origin = ms; g->phase = RA_INPUT; }
    /* Grade each gap independently so one early/late note cannot shift every
       later target. Callback timestamps still exclude rendering latency. */
    int64_t delta = g->index ? ms - g->last_press -
        (g->pattern.at[g->index] - g->pattern.at[g->index-1]) : 0;
    g->last_press = ms;
    g->error_ms = delta > 9999 ? 9999 : delta < -9999 ? -9999 : (int)delta;
    int64_t error = delta < 0 ? -delta : delta;
    unsigned perfect = g->mode == RA_TRAIN ? 110 : 80;
    unsigned good = g->mode == RA_TRAIN ? 220 : 160;
    unsigned points = 0;
    if (key != g->pattern.keys[g->index]) { g->last_mark = RA_WRONG; g->all_keys = false; }
    else if (error <= perfect) { g->last_mark = RA_EXACT; points = 100; }
    else if (error <= good) { g->last_mark = RA_GOOD; points = 75; }
    else { g->last_mark = delta < 0 ? RA_EARLY : RA_LATE; points = 25; }
    g->marks[g->index++] = g->last_mark; g->points += points;
    if (g->index == g->pattern.count) finish(g);
    return true;
}
void ra_tick(ra_game_t *g, int64_t ms) {
    /* Training waits for all keys; challenge times out relative to the last
       accepted key, using the same interval as the scoring target. */
    if (g->phase != RA_INPUT || g->mode == RA_TRAIN ||
        ms <= g->last_press + g->pattern.at[g->index] - g->pattern.at[g->index-1] + 1100) return;
    while (g->index < g->pattern.count) g->marks[g->index++] = RA_MISSED;
    g->all_keys = false; g->last_mark = RA_MISSED; finish(g);
}
void ra_replay(ra_game_t *g) { if (g->phase == RA_DEMO || g->phase == RA_READY || g->phase == RA_INPUT) reset_attempt(g); }
void ra_next(ra_game_t *g) {
    if (g->phase != RA_FEEDBACK) return;
    if ((g->passed && g->door + 1 == RA_DOORS) || (!g->passed && g->mode == RA_CHALLENGE && !g->lives)) {
        g->completed = g->passed; g->phase = RA_FINISHED; return;
    }
    if (g->passed) { g->door++; ra_pattern(&g->pattern, g->seed, g->door, g->mode); }
    reset_attempt(g);
}
