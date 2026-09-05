#pragma once
#include <stdbool.h>
#include <stdint.h>

#define BR_ROUNDS 8
#define BR_TIMEOUT 9000
#define BR_FEEDBACK 450

typedef enum { BR_HOME, BR_AIM, BR_CHOICE, BR_PAUSED, BR_RESULT } br_page_t;
typedef struct {
    br_page_t page, resume;
    uint8_t round, clears, perfects;
    uint16_t needle, target, half_width;
    uint32_t phase, elapsed, cooldown, pot, score, best, seed;
    bool perfect, burst, new_best;
} br_state_t;

void br_home(br_state_t *s);
void br_start(br_state_t *s, uint32_t seed);
void br_tick(br_state_t *s, uint32_t ms);
bool br_stop(br_state_t *s);
bool br_continue(br_state_t *s);
bool br_collect(br_state_t *s);
void br_pause(br_state_t *s);
uint32_t br_reward(const br_state_t *s);
uint32_t br_remaining(const br_state_t *s);
