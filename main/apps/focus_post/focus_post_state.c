#include "focus_post_state.h"
#include <string.h>
static uint32_t random_step(uint32_t *r) { *r = *r * 1664525u + 1013904223u; return *r; }
void fp_init(fp_state_t *s) { memset(s, 0, sizeof(*s)); }
void fp_home(fp_state_t *s) { s->page = FP_HOME; }
unsigned fp_duration(const fp_state_t *s) { return 4000u - (s->pace > 2 ? 2 : s->pace) * 1000u; }
uint8_t fp_current(const fp_state_t *s)
{
    if (s->tutorial) return s->practice == 0 ? s->target : (s->target + 1) % FP_ANIMALS;
    return s->cards[s->round < FP_ROUNDS ? s->round : FP_ROUNDS - 1];
}
void fp_start(fp_state_t *s, uint32_t seed, bool tutorial)
{
    uint8_t pace = s->pace;
    fp_init(s); s->pace = pace; s->seed = seed; s->tutorial = tutorial;
    uint32_t r = seed; s->target = random_step(&r) % FP_ANIMALS;
    /* Two balanced halves: no trial streak can exceed six. Both skills are
       measured equally, so waiting or pressing on every card cannot win. */
    for (unsigned half = 0; half < 2; half++) {
        uint8_t order[6] = {1, 1, 1, 0, 0, 0};
        for (unsigned i = 5; i; i--) {
            unsigned j = (random_step(&r) >> 8) % (i + 1);
            uint8_t swap = order[i]; order[i] = order[j]; order[j] = swap;
        }
        for (unsigned i = 0; i < 6; i++) s->cards[half * 6 + i] = order[i] ? s->target :
            (s->target + 1 + ((random_step(&r) >> 8) % 2)) % FP_ANIMALS;
    }
    s->page = FP_RULE;
}
static void ready(fp_state_t *s, int64_t now)
{
    s->page = FP_READY; s->opened = now; s->deadline = now + 850;
}
static void finish(fp_state_t *s, bool pressed)
{
    s->pressed = pressed;
    s->correct = pressed == (fp_current(s) == s->target);
    if (!s->tutorial) {
        if (s->correct && pressed) s->delivered++;
        else if (s->correct) s->waited++;
        else if (pressed) s->slips++;
        else s->misses++;
    }
    s->page = FP_FEEDBACK;
}
void fp_tick(fp_state_t *s, int64_t now)
{
    if (s->page == FP_READY && now >= s->deadline) {
        s->page = s->tutorial ? FP_PRACTICE : FP_VISITOR;
        /* Start from actual presentation time, never catch up missed trials. */
        s->opened = now;
        s->deadline = now + (s->resuming ? s->resume_window : fp_duration(s));
        s->resuming = false;
    } else if ((s->page == FP_VISITOR || s->page == FP_PRACTICE) && now >= s->deadline) {
        if (!(s->tutorial && s->practice == 0)) finish(s, false);
    }
}
void fp_ok(fp_state_t *s, int64_t now)
{
    if (now < s->opened) return;
    /* Input timestamps decide boundary results even if the UI is late. */
    if (s->page == FP_VISITOR || s->page == FP_PRACTICE) {
        if (now >= s->deadline && !(s->tutorial && s->practice == 0)) finish(s, false);
        else finish(s, true);
    } else if (s->page == FP_RULE) ready(s, now);
    else if (s->page == FP_FEEDBACK) {
        if (s->tutorial) {
            if (!s->correct) { ready(s, now); return; }
            if (s->practice == 0) s->practice = 1;
            else { s->tutorial = false; s->page = FP_RULE; return; }
        } else if (++s->round == FP_ROUNDS) { s->page = FP_RESULT; return; }
        ready(s, now);
    }
}
void fp_pause(fp_state_t *s, int64_t now)
{
    if (s->page == FP_HOME || s->page == FP_PAUSE || s->page == FP_RESULT) return;
    /* Settle a deadline before freezing; repeated pauses do not extend it. */
    if (s->page == FP_VISITOR || s->page == FP_PRACTICE) fp_tick(s, now);
    s->resume_page = s->page;
    s->remaining = s->deadline > now ? s->deadline - now : 0;
    s->page = FP_PAUSE;
}
void fp_resume(fp_state_t *s, int64_t now)
{
    if (s->page != FP_PAUSE) return;
    if (s->resume_page == FP_VISITOR || s->resume_page == FP_PRACTICE) {
        s->resuming = true; s->resume_window = s->remaining;
        ready(s, now);
    } else {
        s->page = s->resume_page; s->opened = now; s->deadline = now + s->remaining;
    }
}
