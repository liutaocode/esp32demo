#include "pvz_state.h"
#include <string.h>
static uint32_t random_next(pvz_state_t *s) {
    uint32_t x = s->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return s->rng = x;
}
static void question(pvz_state_t *s) {
    uint8_t answer = s->deck[s->round];
    unsigned first = answer < PVZ_PLANTS ? 0 : PVZ_PLANTS;
    unsigned count = answer < PVZ_PLANTS ? PVZ_PLANTS : PVZ_COUNT - PVZ_PLANTS;
    s->options[0] = answer;
    for (unsigned i = 1; i < PVZ_OPTIONS; ++i) {
        uint8_t v;
        bool duplicate;
        do {
            v = first + random_next(s) % count;
            duplicate = false;
            for (unsigned j = 0; j < i; ++j) if (s->options[j] == v) duplicate = true;
        } while (duplicate);
        s->options[i] = v;
    }
    for (unsigned i = PVZ_OPTIONS - 1; i > 0; --i) {
        unsigned j = random_next(s) % (i + 1);
        uint8_t t = s->options[i]; s->options[i] = s->options[j]; s->options[j] = t;
    }
    s->choice = 0;
    s->page = PVZ_QUIZ;
}
void pvz_init(pvz_state_t *s) { memset(s, 0, sizeof(*s)); }
void pvz_home(pvz_state_t *s) { s->page = PVZ_HOME; }
void pvz_open(pvz_state_t *s, unsigned menu, uint32_t seed) {
    if (menu > 2) return;
    s->menu = menu;
    if (menu < 2) {
        s->page = PVZ_BROWSE;
        s->kind = menu;
        s->index = menu ? PVZ_PLANTS : 0;
        s->seen |= UINT32_C(1) << s->index;
        return;
    }
    s->seed = seed ? seed : 1;
    s->rng = s->seed;
    s->score = s->round = 0;
    uint8_t pool[PVZ_COUNT];
    for (unsigned i = 0; i < PVZ_COUNT; ++i) pool[i] = i;
    for (unsigned i = 0; i < PVZ_ROUNDS; ++i) {
        unsigned j = i + random_next(s) % (PVZ_COUNT - i);
        uint8_t t = pool[i]; pool[i] = pool[j]; pool[j] = t;
        s->deck[i] = pool[i];
    }
    question(s);
}
void pvz_move(pvz_state_t *s, int direction) {
    if (!direction) return;
    int delta = direction > 0 ? 1 : -1;
    if (s->page == PVZ_HOME) s->menu = (s->menu + 3 + delta) % 3;
    else if (s->page == PVZ_QUIZ) s->choice = (s->choice + PVZ_OPTIONS + delta) % PVZ_OPTIONS;
    else if (s->page == PVZ_BROWSE) {
        int first = s->kind ? PVZ_PLANTS : 0;
        int count = s->kind ? PVZ_COUNT - PVZ_PLANTS : PVZ_PLANTS;
        s->index = first + (s->index - first + count + delta) % count;
        s->seen |= UINT32_C(1) << s->index;
    }
}
void pvz_confirm(pvz_state_t *s) {
    if (s->page == PVZ_QUIZ) {
        s->correct = s->options[s->choice] == s->deck[s->round];
        if (s->correct) ++s->score;
        s->index = s->deck[s->round];
        s->seen |= UINT32_C(1) << s->index;
        s->page = PVZ_REVEAL;
    } else if (s->page == PVZ_REVEAL) {
        if (++s->round == PVZ_ROUNDS) s->page = PVZ_RESULT;
        else question(s);
    } else if (s->page == PVZ_RESULT) pvz_open(s, 2, s->seed);
}
unsigned pvz_seen_count(const pvz_state_t *s) {
    unsigned n = 0;
    for (unsigned i = 0; i < PVZ_COUNT; ++i) n += (s->seen >> i) & 1U;
    return n;
}
const char *pvz_rank(unsigned score) {
    if (score >= 5) return "草坪守护大师";
    if (score >= 3) return "花园研究员";
    return "新晋小园丁";
}
