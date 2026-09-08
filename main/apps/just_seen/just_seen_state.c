#include "just_seen_state.h"
#include <string.h>
static uint32_t random_next(uint32_t *seed)
{
    *seed ^= *seed << 13; *seed ^= *seed >> 17; *seed ^= *seed << 5;
    return *seed;
}
void js_init(js_state_t *s) { memset(s, 0, sizeof(*s)); s->back = 1; }
void js_home(js_state_t *s) { s->page = JS_HOME; }
void js_mode(js_state_t *s, int direction)
{
    if (s->page == JS_HOME)
        s->back = (uint8_t)((s->back - 1 + (direction > 0 ? 1 : 2)) % 3 + 1);
}
void js_start(js_state_t *s, uint16_t code)
{
    s->code = (uint16_t)(1000 + code % 9000);
    uint32_t seed = s->code * 37U + s->back;
    bool same[JS_ROUNDS] = {true, true, true, true, false, false, false, false};
    for (unsigned i = JS_ROUNDS - 1; i > 0; i--) {
        unsigned j = random_next(&seed) % (i + 1);
        bool tmp = same[i]; same[i] = same[j]; same[j] = tmp;
    }
    for (unsigned i = 0; i < s->back; i++) s->cards[i] = random_next(&seed) % JS_ANIMALS;
    for (unsigned r = 0; r < JS_ROUNDS; r++) {
        uint8_t old = s->cards[r];
        s->cards[r + s->back] = same[r] ? old :
            (uint8_t)((old + 1 + random_next(&seed) % (JS_ANIMALS - 1)) % JS_ANIMALS);
    }
    s->round = s->cursor = s->correct = s->streak = s->longest = 0;
    s->last_correct = false;
    s->page = JS_LEARN;
}
uint8_t js_current(const js_state_t *s)
{
    unsigned index = s->page == JS_LEARN ? s->round + s->cursor : s->round + s->back;
    return s->cards[index];
}
uint8_t js_expected(const js_state_t *s) { return s->cards[s->round]; }
void js_next(js_state_t *s)
{
    if (s->page == JS_LEARN) {
        if (++s->cursor == s->back) s->page = JS_ASK;
    } else if (s->page == JS_FEEDBACK) {
        if (s->round == JS_ROUNDS - 1) {
            s->page = JS_RESULT;
            if (s->correct > s->best[s->back - 1]) s->best[s->back - 1] = s->correct;
        } else { ++s->round; s->page = JS_ASK; }
    }
}
void js_answer(js_state_t *s, bool same)
{
    if (s->page != JS_ASK) return;
    s->last_correct = (same == (js_current(s) == js_expected(s)));
    if (s->last_correct) {
        ++s->correct; ++s->streak;
        if (s->streak > s->longest) s->longest = s->streak;
    } else s->streak = 0;
    s->page = JS_FEEDBACK;
}
void js_pause(js_state_t *s)
{
    if (s->page == JS_ASK || s->page == JS_LEARN || s->page == JS_FEEDBACK) {
        s->resume = s->page; s->page = JS_PAUSE;
    }
}
void js_resume(js_state_t *s)
{
    if (s->page != JS_PAUSE) return;
    s->page = s->resume;
    if (s->page != JS_FEEDBACK) { s->cursor = 0; s->page = JS_LEARN; }
}
