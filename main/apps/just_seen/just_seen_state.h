#pragma once
#include <stdbool.h>
#include <stdint.h>
#define JS_ROUNDS 8
#define JS_ANIMALS 6
#define JS_MAX_BACK 3

typedef enum { JS_HOME, JS_LEARN, JS_ASK, JS_FEEDBACK, JS_PAUSE, JS_RESULT } js_page_t;
typedef struct {
    js_page_t page, resume;
    uint8_t back, cursor, round, correct, streak, longest;
    uint8_t cards[JS_ROUNDS + JS_MAX_BACK];
    uint8_t best[JS_MAX_BACK];
    uint16_t code;
    bool last_correct;
} js_state_t;
void js_init(js_state_t *s);
void js_home(js_state_t *s);
void js_mode(js_state_t *s, int direction);
void js_start(js_state_t *s, uint16_t code);
void js_next(js_state_t *s);
void js_answer(js_state_t *s, bool same);
void js_pause(js_state_t *s);
void js_resume(js_state_t *s);
uint8_t js_current(const js_state_t *s);
uint8_t js_expected(const js_state_t *s);
