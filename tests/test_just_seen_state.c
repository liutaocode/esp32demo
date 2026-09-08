#include "just_seen_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    for (unsigned mode = 1; mode <= 3; mode++) for (unsigned seed = 0; seed < 1000; seed++) {
        js_state_t s; js_init(&s); s.back = mode; js_start(&s, seed);
        uint8_t sequence[11]; memcpy(sequence, s.cards, sizeof sequence);
        uint16_t code = s.code;
        unsigned matches = 0;
        for (unsigned i = 0; i < mode + 8; i++) assert(s.cards[i] < JS_ANIMALS);
        for (unsigned i = 0; i < mode; i++) { assert(js_current(&s) == sequence[i]); js_next(&s); }
        for (unsigned r = 0; r < 8; r++) {
            assert(s.page == JS_ASK && s.round == r);
            assert(js_current(&s) == sequence[r + mode]);
            assert(js_expected(&s) == sequence[r]);
            js_state_t saved = s;
            js_pause(&s); js_pause(&s); assert(s.page == JS_PAUSE);
            js_answer(&s, true); assert(s.correct == saved.correct);
            js_resume(&s); assert(s.page == JS_LEARN);
            for (unsigned i = 0; i < mode; i++) {
                assert(js_current(&s) == sequence[r + i]); js_next(&s);
            }
            assert(s.page == JS_ASK && s.round == r && s.correct == saved.correct);
            bool same = sequence[r] == sequence[r + mode]; matches += same;
            js_answer(&s, same); assert(s.last_correct && s.correct == r + 1);
            js_answer(&s, !same); assert(s.correct == r + 1);
            js_pause(&s); js_resume(&s); assert(s.page == JS_FEEDBACK);
            js_next(&s);
        }
        assert(matches == 4 && s.page == JS_RESULT && s.longest == 8 && s.best[mode - 1] == 8);
        js_next(&s); assert(s.page == JS_RESULT);
        js_start(&s, code - 1000); assert(!memcmp(sequence, s.cards, mode + 8));
        for (unsigned i = 0; i < mode; i++) js_next(&s);
        for (unsigned r = 0; r < 8; r++) {
            js_answer(&s, sequence[r] != sequence[r + mode]); js_next(&s);
        }
        assert(!s.correct && !s.longest && s.best[mode - 1] == 8);
        js_home(&s); js_mode(&s, 1); assert(s.back == mode % 3 + 1);
        js_mode(&s, -1); assert(s.back == mode);
    }
    js_state_t s; js_init(&s); js_start(&s, 9); js_next(&s);
    for (unsigned i = 0; i < 8; i++) {
        bool right = (i == 0 || i == 1 || i >= 5);
        js_answer(&s, (js_current(&s) == js_expected(&s)) == right); js_next(&s);
    }
    assert(s.correct == 5 && s.longest == 3);
    puts("Just Seen state: 3000 puzzles, independent answer oracle, replay, pause, scoring PASS");
}
