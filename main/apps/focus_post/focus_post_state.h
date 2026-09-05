#pragma once
#include <stdbool.h>
#include <stdint.h>
#define FP_ROUNDS 12
#define FP_ANIMALS 3
typedef enum { FP_HOME, FP_RULE, FP_PRACTICE, FP_READY, FP_VISITOR, FP_FEEDBACK, FP_PAUSE, FP_RESULT } fp_page_t;
typedef struct {
    fp_page_t page, resume_page;
    uint8_t pace, target, cards[FP_ROUNDS], round, practice;
    uint8_t delivered, waited, misses, slips;
    bool tutorial, correct, pressed, resuming;
    uint32_t seed;
    int64_t opened, deadline, remaining, resume_window;
} fp_state_t;
void fp_init(fp_state_t *s);
void fp_home(fp_state_t *s);
void fp_start(fp_state_t *s, uint32_t seed, bool tutorial);
void fp_ok(fp_state_t *s, int64_t now);
void fp_tick(fp_state_t *s, int64_t now);
void fp_pause(fp_state_t *s, int64_t now);
void fp_resume(fp_state_t *s, int64_t now);
uint8_t fp_current(const fp_state_t *s);
unsigned fp_duration(const fp_state_t *s);
