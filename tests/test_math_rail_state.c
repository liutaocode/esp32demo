#include "math_rail_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned correct_choice(const mt_state_t *s) { return s->current.options[0] == s->current.answer ? 0 : 1; }
static void finish(mt_state_t *s, unsigned score)
{
    unsigned n = s->count;
    for (unsigned i = 0; i < n; i++) {
        assert(s->page == MT_ASK);
        unsigned choice = correct_choice(s);
        assert(mt_answer(s, i < score ? choice : 1 - choice));
        assert(!mt_answer(s, choice));
        assert(mt_next(s));
    }
    assert(s->page == MT_RESULT);
}
int main(void)
{
    for (unsigned mode = 0; mode < MT_MODES; mode++) for (uint32_t seed = 0; seed < 25000; seed++) {
        mt_state_t s, t; mt_init(&s); mt_init(&t); s.mode = t.mode = mode;
        mt_start(&s, seed); mt_start(&t, seed);
        assert(!memcmp(s.questions, t.questions, sizeof(s.questions)));
        unsigned top = 0, opcounts[4] = {0};
        for (unsigned i = 0; i < MT_ROUNDS; i++) {
            mt_question_t q = s.questions[i]; opcounts[q.op]++;
            unsigned result = q.op == MT_ADD ? q.a + q.b : q.op == MT_SUB ? q.a - q.b : q.op == MT_MUL ? q.a * q.b : q.a / q.b;
            assert(result == q.answer && q.options[0] != q.options[1]);
            assert(q.options[0] == q.answer || q.options[1] == q.answer);
            top += q.options[0] == q.answer;
            if (mode < 3) {
                unsigned limit = mode == 0 ? 10 : mode == 1 ? 20 : 100;
                assert(q.a <= limit && q.b <= limit && q.answer <= limit);
                assert(q.options[0] <= limit && q.options[1] <= limit);
            } else {
                assert(q.b >= 2 && q.b <= 9);
                if (q.op == MT_DIV) assert(q.a % q.b == 0 && q.answer >= 2 && q.answer <= 9);
                else assert(q.a >= 2 && q.a <= 9);
                assert(q.options[0] <= 81 && q.options[1] <= 81);
            }
            for (unsigned j = 0; j < i; j++) {
                mt_question_t p = s.questions[j];
                if (q.op != p.op) continue;
                assert(q.a != p.a || q.b != p.b);
                if (q.op == MT_ADD || q.op == MT_MUL) assert(q.a != p.b || q.b != p.a);
            }
        }
        assert(top == 5);
        assert(opcounts[mode == 3 ? MT_MUL : MT_ADD] == 5);
        assert(opcounts[mode == 3 ? MT_DIV : MT_SUB] == 5);
    }
    mt_state_t s; mt_init(&s); mt_mode(&s, -1); assert(s.mode == 3); mt_mode(&s, 1); assert(s.mode == 0);
    assert(!mt_review(&s) && !mt_answer(&s, 0) && !mt_next(&s));
    mt_start(&s, 7); assert(!mt_answer(&s, 2));
    mt_pause(&s); assert(s.page == MT_PAUSED); assert(!mt_answer(&s, 0)); mt_pause(&s); assert(s.page == MT_ASK);
    finish(&s, 8); assert(s.correct == 8 && s.best_streak == 8 && s.miss_count == 2 && s.stamps[0] == 1);
    assert(!mt_next(&s)); assert(s.stamps[0] == 1);
    for (unsigned i = 0; i < 20; i++) {
        assert(mt_review(&s));
        assert(s.current.a == s.questions[8].a && s.current.b == s.questions[8].b);
        finish(&s, i % 3); assert(s.correct == 8 && s.best[0] == 8 && s.stamps[0] == 1 && s.miss_count == 2);
    }
    for (unsigned i = 0; i < 10; i++) { mt_start(&s, i); finish(&s, 10); }
    assert(s.stamps[0] == 6 && s.best[0] == 10 && !s.miss_count && !mt_review(&s));
    mt_start(&s, 42); finish(&s, 0); assert(s.miss_count == 10); assert(mt_review(&s)); finish(&s, 10);
    assert(s.review_correct == 10 && s.correct == 0 && s.best[0] == 10);
    mt_home(&s); mt_mode(&s, 1); mt_start(&s, 99); finish(&s, 7); assert(s.stamps[1] == 0 && s.best[1] == 7);
    assert(s.stamps[0] == 6); mt_init(&s); assert(!s.stamps[0]);
    puts("Math Rail: 100000 courses / 1000000 questions, arithmetic, bounds, balance, uniqueness, review, pause and stamps PASS");
}
