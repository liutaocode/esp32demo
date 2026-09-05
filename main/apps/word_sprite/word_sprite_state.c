#include "word_sprite_state.h"
#include <string.h>

static uint32_t random_next(ws_state_t *s)
{
    uint32_t x = s->random;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return s->random = x;
}

unsigned ws_count(uint64_t mask)
{
    unsigned count = 0;
    for (mask &= WS_VALID_MASK; mask; mask &= mask - 1) count++;
    return count;
}

unsigned ws_stage(const ws_state_t *s)
{
    unsigned n = ws_count(s->progress.learned);
    return n >= 30 ? 3 : n >= 18 ? 2 : n >= 6 ? 1 : 0;
}

void ws_init(ws_state_t *s, uint32_t seed, ws_progress_t progress)
{
    memset(s, 0, sizeof(*s));
    s->random = seed ? seed : 0x13579bdfU;
    s->progress.review = progress.review & WS_VALID_MASK;
    s->progress.learned = progress.learned & WS_VALID_MASK & ~s->progress.review;
}

uint8_t ws_word(const ws_state_t *s)
{
    return s->index < s->count ? s->deck[s->index] : 0;
}

static void question(ws_state_t *s)
{
    uint8_t target = ws_word(s);
    unsigned start = target / 12 * 12;
    uint8_t pool[11];
    unsigned n = 0;
    for (unsigned i = start; i < start + 12; i++)
        if (i != target) pool[n++] = (uint8_t)i;
    unsigned first = random_next(s) % 11;
    uint8_t a = pool[first];
    pool[first] = pool[10];
    uint8_t b = pool[random_next(s) % 10];
    unsigned slot = random_next(s) % 3;
    s->options[slot] = target;
    s->options[(slot + 1) % 3] = a;
    s->options[(slot + 2) % 3] = b;
    s->selected = 0;
    s->page = WS_QUESTION;
}

bool ws_start(ws_state_t *s, bool review)
{
    if (s->page != WS_HOME && s->page != WS_RESULT) return false;
    uint8_t pool[WS_WORDS];
    unsigned n = 0;
    for (unsigned i = 0; i < WS_WORDS; i++) {
        if (review ? !!(s->progress.review & (UINT64_C(1) << i))
                   : i / 12 == s->world) pool[n++] = (uint8_t)i;
    }
    if (!n) return false;
    /* Shuffle without replacement. Prefer unlit words in normal rounds. */
    for (unsigned i = n - 1; i > 0; i--) {
        unsigned j = random_next(s) % (i + 1);
        uint8_t t = pool[i]; pool[i] = pool[j]; pool[j] = t;
    }
    if (!review) {
        unsigned next = 0;
        for (unsigned i = 0; i < n; i++) {
            if (!(s->progress.learned & (UINT64_C(1) << pool[i]))) {
                uint8_t t = pool[next]; pool[next++] = pool[i]; pool[i] = t;
            }
        }
    }
    s->count = (uint8_t)(n < WS_ROUND ? n : WS_ROUND);
    memcpy(s->deck, pool, s->count);
    s->index = s->correct = s->recovered = 0;
    s->reviewing = review;
    question(s);
    return true;
}

void ws_move(ws_state_t *s, int delta)
{
    int step = delta < 0 ? -1 : delta > 0 ? 1 : 0;
    if (s->page == WS_HOME) s->world = (uint8_t)((s->world + WS_WORLDS + step) % WS_WORLDS);
    if (s->page == WS_QUESTION) s->selected = (uint8_t)((s->selected + 3 + step) % 3);
}

bool ws_answer(ws_state_t *s)
{
    if (s->page != WS_QUESTION) return false;
    uint64_t bit = UINT64_C(1) << ws_word(s);
    s->last_correct = s->options[s->selected] == ws_word(s);
    if (s->last_correct) {
        s->correct++;
        if (s->progress.review & bit) s->recovered++;
        s->progress.learned |= bit;
        s->progress.review &= ~bit;
    } else {
        s->progress.review |= bit;
        s->progress.learned &= ~bit;
    }
    s->page = WS_FEEDBACK;
    return true;
}

void ws_next(ws_state_t *s)
{
    if (s->page != WS_FEEDBACK) return;
    if (++s->index == s->count) s->page = WS_RESULT;
    else question(s);
}

void ws_home(ws_state_t *s) { s->page = WS_HOME; }
