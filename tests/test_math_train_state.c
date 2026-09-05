#include "math_train_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void answer(mt_state_t *s, bool right)
{
    const mt_question_t *q = mt_current(s);
    unsigned i = 0;
    while ((q->choices[i] == q->answer) != right) i++;
    s->selected = i;
    assert(mt_answer(s)); assert(!mt_answer(s));
    assert(mt_next(s)); assert(!mt_next(s));
}
int main(void)
{
    unsigned positions[3] = {0};
    for (unsigned mode = 0; mode < 4; mode++) for (unsigned seed = 0; seed < 3000; seed++) {
        mt_state_t s, t; mt_init(&s); s.mode = mode; mt_start(&s, seed);
        mt_init(&t); t.mode = mode; mt_start(&t, seed);
        assert(memcmp(s.questions, t.questions, sizeof(s.questions)) == 0);
        unsigned ops[4] = {0};
        unsigned limit = mode == 0 ? 10 : mode == 1 ? 20 : mode == 2 ? 100 : 81;
        for (unsigned i = 0; i < 10; i++) {
            const mt_question_t *q = &s.questions[i]; ops[q->op]++;
            assert(q->a <= limit && q->b <= limit && q->answer <= limit);
            switch(q->op) {
            case 0: assert(q->a + q->b == q->answer); break;
            case 1: assert(q->a >= q->b && q->a - q->b == q->answer); break;
            case 2: assert(q->a >= 1 && q->a <= 9 && q->b >= 1 && q->b <= 9 && q->a * q->b == q->answer); break;
            case 3: assert(q->b >= 1 && q->b <= 9 && q->answer >= 1 && q->answer <= 9 && q->a % q->b == 0 && q->a / q->b == q->answer); break;
            default: assert(0);
            }
            unsigned matches = 0;
            for (unsigned k = 0; k < 3; k++) {
                assert(q->choices[k] <= limit);
                if (q->choices[k] == q->answer) { matches++; positions[k]++; }
                for (unsigned j = k + 1; j < 3; j++) assert(q->choices[k] != q->choices[j]);
            }
            assert(matches == 1);
            for (unsigned j = 0; j < i; j++) {
                const mt_question_t *p = &s.questions[j];
                assert(p->a != q->a || p->b != q->b || p->op != q->op);
            }
            answer(&s, i % 3 != 0);
        }
        assert(ops[mode == 3 ? 2 : 0] == 5 && ops[mode == 3 ? 3 : 1] == 5);
        assert(s.page == MT_RESULT && s.correct == 6 && s.best_streak == 2 && mt_missed(&s) == 4);
        assert(s.records[mode] == 6 && s.new_record);
        assert(mt_review(&s)); assert(s.review_total == 4);
        for (unsigned i = 0; i < 4; i++) answer(&s, i != 1);
        assert(s.page == MT_RESULT && mt_missed(&s) == 1 && s.correct == 6 && s.records[mode] == 6);
        assert(mt_review(&s)); answer(&s, true);
        assert(s.page == MT_RESULT && mt_missed(&s) == 0 && !mt_review(&s));
        mt_move(&s, 1); assert(s.selected == 2); mt_move(&s, -1); assert(s.selected == 0);
        mt_home(&s); assert(s.selected == mode && s.page == MT_HOME);
        mt_start(&s, seed + 1); assert(!s.reviewing && !mt_missed(&s) && !s.correct);
        for (unsigned i = 0; i < 10; i++) answer(&s, true);
        assert(s.correct == 10 && s.best_streak == 10 && s.records[mode] == 10);
    }
    for (unsigned i = 0; i < 3; i++) assert(positions[i] > 35000 && positions[i] < 45000);
    mt_state_t s; mt_init(&s); assert(!mt_review(&s) && !mt_answer(&s) && !mt_next(&s));
    mt_move(&s, -1); assert(s.mode == 3); mt_move(&s, 1); assert(s.mode == 0);
    puts("Math Train: 120,000 random questions, scoring, review and navigation PASS");
}
