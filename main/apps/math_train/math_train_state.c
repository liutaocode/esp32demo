#include "math_train_state.h"
#include <string.h>

static uint32_t random32(mt_state_t *s)
{
    uint32_t x = s->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return s->rng = x;
}
static unsigned pick(mt_state_t *s, unsigned n) { return random32(s) % n; }
static void shuffle(mt_state_t *s, mt_question_t *q)
{
    for (unsigned i = 2; i > 0; i--) {
        unsigned j = pick(s, i + 1);
        uint8_t v = q->choices[i]; q->choices[i] = q->choices[j]; q->choices[j] = v;
    }
}
static mt_question_t question(mt_state_t *s, unsigned op)
{
    unsigned limit = s->mode == 0 ? 10 : s->mode == 1 ? 20 : 100;
    mt_question_t q = {.op = op};
    if (s->mode == 3) {
        unsigned a = 1 + pick(s, 9), b = 1 + pick(s, 9);
        q.a = op == 2 ? a : a * b; q.b = b;
        q.answer = op == 2 ? a * b : a;
        limit = 81;
    } else if (op == 0) {
        q.a = pick(s, limit + 1); q.b = pick(s, limit - q.a + 1);
        q.answer = q.a + q.b;
    } else {
        q.a = pick(s, limit + 1); q.b = pick(s, q.a + 1);
        q.answer = q.a - q.b;
    }
    q.choices[0] = q.answer;
    /* Plausible neighbouring answers, all distinct and within the mode. */
    for (unsigned i = 1; i < 3; i++) {
        unsigned candidate;
        do {
            int d = (int)(1 + pick(s, s->mode >= 2 ? 10 : 3));
            int value = q.answer + (pick(s, 2) ? d : -d);
            candidate = value < 0 || value > (int)limit ? pick(s, limit + 1) : (unsigned)value;
        } while (candidate == q.choices[0] || (i == 2 && candidate == q.choices[1]));
        q.choices[i] = candidate;
    }
    shuffle(s, &q);
    return q;
}
void mt_init(mt_state_t *s) { memset(s, 0, sizeof(*s)); s->rng = 1; }
void mt_home(mt_state_t *s) { s->page = MT_HOME; s->selected = s->mode; }
void mt_move(mt_state_t *s, int delta)
{
    unsigned count = s->page == MT_HOME ? MT_MODES : 3;
    if (s->page == MT_FEEDBACK) return;
    s->selected = (s->selected + count + (delta > 0 ? 1 : -1)) % count;
    if (s->page == MT_HOME) s->mode = s->selected;
    if (s->page == MT_RESULT && s->selected == 1 && !mt_missed(s))
        s->selected = delta > 0 ? 2 : 0;
}
void mt_start(mt_state_t *s, uint32_t seed)
{
    s->rng = seed ? seed : 0x9e3779b9u;
    s->correct = s->streak = s->best_streak = s->cursor = s->completed = s->selected = 0;
    s->review_total = 0; s->reviewing = s->new_record = false;
    memset(s->missed, 0, sizeof(s->missed));
    /* Five of each operation, shuffled, without duplicate equations. */
    for (unsigned i = 0; i < MT_COUNT; i++) {
        mt_question_t q;
        bool duplicate;
        do {
            q = question(s, (s->mode == 3 ? 2 : 0) + (i % 2));
            duplicate = false;
            for (unsigned j = 0; j < i; j++)
                if (q.a == s->questions[j].a && q.b == s->questions[j].b && q.op == s->questions[j].op)
                    duplicate = true;
        } while (duplicate);
        s->questions[i] = q;
    }
    for (unsigned i = MT_COUNT - 1; i > 0; i--) {
        unsigned j = pick(s, i + 1);
        mt_question_t q = s->questions[i]; s->questions[i] = s->questions[j]; s->questions[j] = q;
    }
    s->page = MT_ASK;
}
unsigned mt_missed(const mt_state_t *s)
{
    unsigned n = 0;
    for (unsigned i = 0; i < MT_COUNT; i++) n += s->missed[i];
    return n;
}
static unsigned current_index(const mt_state_t *s)
{
    return s->reviewing ? s->review_order[s->cursor] : s->cursor;
}
const mt_question_t *mt_current(const mt_state_t *s) { return &s->questions[current_index(s)]; }
bool mt_review(mt_state_t *s)
{
    if (s->page != MT_RESULT || !mt_missed(s)) return false;
    s->review_total = 0;
    for (unsigned i = 0; i < MT_COUNT; i++) if (s->missed[i]) {
        s->review_order[s->review_total++] = i;
        shuffle(s, &s->questions[i]);
    }
    s->reviewing = true; s->cursor = s->completed = s->selected = 0; s->page = MT_ASK;
    return true;
}
bool mt_answer(mt_state_t *s)
{
    if (s->page != MT_ASK || s->selected >= 3) return false;
    const mt_question_t *q = mt_current(s);
    s->last_correct = q->choices[s->selected] == q->answer;
    s->missed[current_index(s)] = !s->last_correct;
    s->completed++;
    if (!s->reviewing) {
        if (s->last_correct) { s->correct++; s->streak++; }
        else s->streak = 0;
        if (s->streak > s->best_streak) s->best_streak = s->streak;
    }
    s->page = MT_FEEDBACK;
    return true;
}
bool mt_next(mt_state_t *s)
{
    if (s->page != MT_FEEDBACK) return false;
    unsigned total = s->reviewing ? s->review_total : MT_COUNT;
    s->selected = 0;
    if (s->completed >= total) {
        s->page = MT_RESULT;
        if (!s->reviewing && s->correct > s->records[s->mode]) {
            s->records[s->mode] = s->correct; s->new_record = true;
        }
    } else { s->cursor++; s->page = MT_ASK; }
    return true;
}
const char *mt_mode_name(unsigned mode)
{
    static const char *names[] = {"十内加减", "二十内加减", "百内加减", "乘除口诀"};
    return names[mode % MT_MODES];
}
const char *mt_operator(unsigned op)
{
    static const char *ops[] = {"+", "-", "×", "÷"}; return ops[op % 4];
}
const char *mt_rank(const mt_state_t *s)
{
    if (s->reviewing) return mt_missed(s) ? "再练一遍就更熟" : "错题全部订正啦";
    return s->correct == 10 ? "满星小司机" : s->correct >= 7 ? "闪亮小司机" : "勇敢小司机";
}
