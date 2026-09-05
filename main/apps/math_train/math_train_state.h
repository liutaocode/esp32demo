#pragma once
#include <stdbool.h>
#include <stdint.h>
#define MT_COUNT 10
#define MT_MODES 4
typedef enum { MT_HOME, MT_ASK, MT_FEEDBACK, MT_RESULT } mt_page_t;
typedef struct { uint8_t a, b, op, answer; uint8_t choices[3]; } mt_question_t;
typedef struct {
    mt_page_t page;
    uint32_t rng;
    uint8_t mode, selected, cursor, completed, correct, streak, best_streak;
    uint8_t records[MT_MODES], review_order[MT_COUNT], review_total;
    bool missed[MT_COUNT], reviewing, last_correct, new_record;
    mt_question_t questions[MT_COUNT];
} mt_state_t;
void mt_init(mt_state_t *s);
void mt_home(mt_state_t *s);
void mt_move(mt_state_t *s, int delta);
void mt_start(mt_state_t *s, uint32_t seed);
bool mt_review(mt_state_t *s);
bool mt_answer(mt_state_t *s);
bool mt_next(mt_state_t *s);
unsigned mt_missed(const mt_state_t *s);
const mt_question_t *mt_current(const mt_state_t *s);
const char *mt_mode_name(unsigned mode);
const char *mt_operator(unsigned op);
const char *mt_rank(const mt_state_t *s);
