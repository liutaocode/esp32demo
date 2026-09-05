#pragma once
#include <stdbool.h>
#include <stdint.h>
#define MT_ROUNDS 10
#define MT_MODES 4
#define MT_STATIONS 6
typedef enum { MT_HOME, MT_ASK, MT_FEEDBACK, MT_RESULT, MT_PAUSED } mt_page_t;
typedef enum { MT_ADD, MT_SUB, MT_MUL, MT_DIV } mt_op_t;
typedef struct { uint8_t a, b, answer, options[2]; mt_op_t op; } mt_question_t;
typedef struct {
    mt_page_t page, resume;
    uint8_t mode, position, count, correct, streak, best_streak;
    uint8_t misses[MT_ROUNDS], miss_count, review_correct;
    uint8_t best[MT_MODES], stamps[MT_MODES];
    bool review, last_correct, earned;
    uint32_t rng, seed;
    mt_question_t questions[MT_ROUNDS], current;
} mt_state_t;
void mt_init(mt_state_t *s);
void mt_home(mt_state_t *s);
void mt_mode(mt_state_t *s, int direction);
void mt_start(mt_state_t *s, uint32_t seed);
bool mt_review(mt_state_t *s);
bool mt_answer(mt_state_t *s, unsigned choice);
bool mt_next(mt_state_t *s);
void mt_pause(mt_state_t *s);
const char *mt_mode_name(unsigned mode);
const char *mt_station_name(unsigned station);
const char *mt_operator(mt_op_t op);
