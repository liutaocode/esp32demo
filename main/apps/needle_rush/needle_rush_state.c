#include "needle_rush_state.h"
#include <string.h>

static int32_t wrap(int32_t a) { a %= NR_TURN; return a < 0 ? a + NR_TURN : a; }
unsigned nr_distance(int32_t a, int32_t b)
{
    unsigned d = (unsigned)wrap(a - b);
    return d > NR_TURN / 2 ? NR_TURN - d : d;
}
int32_t nr_angle(const nr_state_t *s, unsigned pin)
{
    return pin < s->count ? wrap(s->pins[pin] + s->phase) : 0;
}
int nr_speed(const nr_state_t *s)
{
    int speed = 34 + (int)s->level * 5 + (s->mode ? 20 : 0);
    return s->level % 2 ? -speed : speed; /* Degrees/second = millidegrees/ms. */
}
static void level_begin(nr_state_t *s)
{
    s->initial_count = s->count = 2 + s->level / 3;
    s->remaining = 5 + s->level / 3;
    s->phase = (int32_t)((s->challenge * 7919U + s->level * 4733U) % NR_TURN);
    for (unsigned i = 0; i < s->count; i++)
        s->pins[i] = (int32_t)(i * NR_TURN / s->count);
    s->page = NR_SPIN;
    s->elapsed = 0;
}
void nr_home(nr_state_t *s) { s->page = NR_HOME; s->elapsed = 0; }
void nr_start(nr_state_t *s, unsigned challenge)
{
    unsigned mode = s->mode == 1, a = s->best[0], b = s->best[1];
    memset(s, 0, sizeof(*s));
    s->mode = mode; s->best[0] = a; s->best[1] = b;
    s->challenge = challenge % 10000;
    s->lives = mode ? 1 : 3;
    level_begin(s);
}
bool nr_fire(nr_state_t *s)
{
    if (s->page != NR_SPIN) return false;
    s->hit = false;
    for (unsigned i = 0; i < s->count; i++)
        if (nr_distance(nr_angle(s, i), NR_IMPACT) <= NR_CLEARANCE) s->hit = true;
    /* Freeze the displayed wheel throughout the short flight. Input/render
       latency cannot move a safe gap out from under the player's shot. */
    s->page = NR_FLIGHT;
    s->elapsed = 0;
    return true;
}
static void finish(nr_state_t *s)
{
    s->page = NR_RESULT;
    s->new_best = s->score > s->best[s->mode];
    if (s->new_best) s->best[s->mode] = s->score;
}
void nr_tick(nr_state_t *s, uint32_t ms)
{
    /* A stalled display must never fast-forward unseen movement or shots. */
    if (ms > 60) ms = 60;
    if (s->page == NR_SPIN) {
        s->phase = wrap(s->phase + nr_speed(s) * (int32_t)ms);
    } else if (s->page == NR_FLIGHT) {
        s->elapsed += ms;
        if (s->elapsed < NR_FLIGHT_MS) return;
        if (s->hit) {
            s->lives--; s->streak = 0;
        } else {
            s->pins[s->count++] = wrap(NR_IMPACT - s->phase);
            s->remaining--; s->streak++;
            if (s->streak > s->best_streak) s->best_streak = s->streak;
            s->score += 10 + (s->streak % 3 == 0 ? 5 : 0);
        }
        s->page = NR_FEEDBACK; s->elapsed = 0;
    } else if (s->page == NR_FEEDBACK) {
        s->elapsed += ms;
        if (s->elapsed < NR_FEEDBACK_MS) return;
        if (!s->lives) finish(s);
        else if (!s->remaining) {
            if (s->level + 1 == NR_LEVELS) { s->won = true; finish(s); }
            else { s->level++; level_begin(s); }
        } else { s->page = NR_SPIN; s->elapsed = 0; }
    }
}
void nr_pause(nr_state_t *s)
{
    if (s->page == NR_PAUSED) s->page = s->resume;
    else if (s->page == NR_SPIN || s->page == NR_FLIGHT || s->page == NR_FEEDBACK) {
        s->resume = s->page; s->page = NR_PAUSED;
    }
}
