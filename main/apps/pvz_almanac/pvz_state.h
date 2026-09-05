#pragma once
#include "pvz_catalog.h"
#include <stdbool.h>
typedef enum { PVZ_HOME, PVZ_BROWSE, PVZ_QUIZ, PVZ_REVEAL, PVZ_RESULT } pvz_page_t;
typedef struct {
    pvz_page_t page;
    uint8_t menu, index, kind, round, choice, score;
    uint8_t deck[PVZ_ROUNDS], options[PVZ_OPTIONS];
    uint32_t seed, rng, seen;
    bool correct;
} pvz_state_t;
void pvz_init(pvz_state_t *s);
void pvz_move(pvz_state_t *s, int direction);
void pvz_open(pvz_state_t *s, unsigned menu, uint32_t seed);
void pvz_confirm(pvz_state_t *s);
void pvz_home(pvz_state_t *s);
unsigned pvz_seen_count(const pvz_state_t *s);
const char *pvz_rank(unsigned score);
