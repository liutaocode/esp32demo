#include "math_rail_state.h"
#include <string.h>
static uint32_t random_value(mt_state_t *s)
{
    uint32_t x = s->rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return s->rng = x;
}
static unsigned pick(mt_state_t *s, unsigned n) { return random_value(s) % n; }
const char *mt_mode_name(unsigned mode)
{
    static const char *const names[] = {"十以内加减", "二十以内加减", "百以内加减", "九九乘除"};
    return names[mode % MT_MODES];
}
const char *mt_station_name(unsigned station)
{
    static const char *const names[] = {"青草站", "花田站", "森林站", "星光站", "云朵站", "彩虹站"};
    return names[station % MT_STATIONS];
}
const char *mt_operator(mt_op_t op)
{
    static const char *const ops[] = {"+", "−", "×", "÷"};
    return ops[(unsigned)op % 4];
}
void mt_init(mt_state_t *s) { memset(s, 0, sizeof(*s)); }
void mt_home(mt_state_t *s) { s->page = MT_HOME; }
void mt_mode(mt_state_t *s, int direction)
{
    if (s->page == MT_HOME) s->mode = (s->mode + (direction > 0 ? 1 : MT_MODES - 1)) % MT_MODES;
}
static mt_question_t generate(mt_state_t *s, mt_op_t op)
{
    mt_question_t q = {.op = op};
    unsigned limit = s->mode == 0 ? 10 : s->mode == 1 ? 20 : 100;
    if (op == MT_ADD || op == MT_SUB) {
        unsigned total = 2 + pick(s, limit - 1);
        unsigned part = 1 + pick(s, total - 1);
        q.a = op == MT_ADD ? part : total;
        q.b = op == MT_ADD ? total - part : part;
        q.answer = op == MT_ADD ? total : total - part;
    } else {
        unsigned a = 2 + pick(s, 8), b = 2 + pick(s, 8);
        q.a = op == MT_MUL ? a : a * b; q.b = b;
        q.answer = op == MT_MUL ? a * b : a;
    }
    int delta = op == MT_MUL ? q.b : s->mode == 2 && pick(s, 2) ? 10 : 1 + (int)pick(s, 2);
    int wrong = (int)q.answer + (pick(s, 2) ? delta : -delta);
    unsigned max_answer = op == MT_DIV ? 9 : op == MT_MUL ? 81 : limit;
    if (wrong < 0 || wrong > (int)max_answer) wrong = (int)q.answer + ((int)q.answer - delta >= 0 ? -delta : delta);
    q.options[0] = q.answer; q.options[1] = (uint8_t)wrong;
    return q;
}
static bool same(mt_question_t a, mt_question_t b)
{
    if (a.op != b.op) return false;
    if (a.a == b.a && a.b == b.b) return true;
    return (a.op == MT_ADD || a.op == MT_MUL) && a.a == b.b && a.b == b.a;
}
static void show_current(mt_state_t *s)
{
    s->current = s->questions[s->review ? s->misses[s->position] : s->position];
    if (s->review && pick(s, 2)) {
        uint8_t tmp = s->current.options[0]; s->current.options[0] = s->current.options[1]; s->current.options[1] = tmp;
    }
    s->page = MT_ASK;
}
void mt_start(mt_state_t *s, uint32_t seed)
{
    s->seed = seed; s->rng = seed ? seed : 0xA341316CU;
    s->position = s->correct = s->streak = s->best_streak = s->miss_count = s->review_correct = 0;
    s->review = s->earned = s->last_correct = false; s->count = MT_ROUNDS;
    uint8_t slots[MT_ROUNDS];
    for (unsigned i = 0; i < MT_ROUNDS; i++) {
        mt_question_t q;
        bool duplicate;
        do {
            q = generate(s, (mt_op_t)((s->mode == 3 ? MT_MUL : MT_ADD) + i % 2));
            duplicate = false;
            for (unsigned j = 0; j < i; j++) if (same(q, s->questions[j])) duplicate = true;
        } while (duplicate);
        s->questions[i] = q; slots[i] = i % 2;
    }
    for (unsigned i = MT_ROUNDS - 1; i > 0; i--) {
        unsigned j = pick(s, i + 1); mt_question_t q = s->questions[i];
        s->questions[i] = s->questions[j]; s->questions[j] = q;
        j = pick(s, i + 1); uint8_t slot = slots[i]; slots[i] = slots[j]; slots[j] = slot;
    }
    for (unsigned i = 0; i < MT_ROUNDS; i++) if (slots[i]) {
        mt_question_t *q = &s->questions[i]; uint8_t tmp = q->options[0]; q->options[0] = q->options[1]; q->options[1] = tmp;
    }
    show_current(s);
}
bool mt_review(mt_state_t *s)
{
    if (s->page != MT_RESULT || !s->miss_count) return false;
    s->review = true; s->count = s->miss_count; s->position = s->review_correct = 0;
    show_current(s); return true;
}
bool mt_answer(mt_state_t *s, unsigned choice)
{
    if (s->page != MT_ASK || choice > 1) return false;
    s->last_correct = s->current.options[choice] == s->current.answer;
    if (s->review) s->review_correct += s->last_correct;
    else if (s->last_correct) {
        s->correct++; s->streak++;
        if (s->streak > s->best_streak) s->best_streak = s->streak;
    } else { s->misses[s->miss_count++] = s->position; s->streak = 0; }
    s->page = MT_FEEDBACK; return true;
}
bool mt_next(mt_state_t *s)
{
    if (s->page != MT_FEEDBACK) return false;
    if (++s->position < s->count) show_current(s);
    else {
        s->page = MT_RESULT;
        if (!s->review) {
            if (s->correct > s->best[s->mode]) s->best[s->mode] = s->correct;
            s->earned = s->correct >= 8 && s->stamps[s->mode] < MT_STATIONS;
            if (s->earned) s->stamps[s->mode]++;
        }
    }
    return true;
}
void mt_pause(mt_state_t *s)
{
    if (s->page == MT_PAUSED) s->page = s->resume;
    else if (s->page == MT_ASK || s->page == MT_FEEDBACK) { s->resume = s->page; s->page = MT_PAUSED; }
}
