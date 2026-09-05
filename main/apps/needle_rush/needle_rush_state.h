#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { NR_TURN = 360000, NR_IMPACT = 90000, NR_CLEARANCE = 14000,
       NR_LEVELS = 10, NR_MAX_PINS = 14, NR_FLIGHT_MS = 120,
       NR_FEEDBACK_MS = 420 };
typedef enum { NR_HOME, NR_SPIN, NR_FLIGHT, NR_FEEDBACK, NR_PAUSED, NR_RESULT } nr_page_t;
typedef struct {
    nr_page_t page, resume;
    unsigned mode, level, lives, remaining, count, initial_count;
    unsigned score, streak, best_streak, best[2], challenge, elapsed;
    int32_t phase, pins[NR_MAX_PINS];
    bool hit, won, new_best;
} nr_state_t;

void nr_home(nr_state_t *s);
void nr_start(nr_state_t *s, unsigned challenge);
void nr_tick(nr_state_t *s, uint32_t ms);
bool nr_fire(nr_state_t *s);
void nr_pause(nr_state_t *s);
int32_t nr_angle(const nr_state_t *s, unsigned pin);
unsigned nr_distance(int32_t a, int32_t b);
int nr_speed(const nr_state_t *s);
