#pragma once
#include <stdbool.h>
#include <stdint.h>

#define WS_WORDS 36
#define WS_WORLDS 3
#define WS_ROUND 6
#define WS_VALID_MASK ((UINT64_C(1) << WS_WORDS) - 1)

typedef enum { WS_HOME, WS_QUESTION, WS_FEEDBACK, WS_RESULT } ws_page_t;
typedef struct { uint64_t learned, review; } ws_progress_t;
typedef struct {
    ws_page_t page;
    ws_progress_t progress;
    uint32_t random;
    uint8_t world, deck[WS_ROUND], count, index, options[3], selected;
    uint8_t correct, recovered;
    bool reviewing, last_correct;
} ws_state_t;

void ws_init(ws_state_t *s, uint32_t seed, ws_progress_t progress);
bool ws_start(ws_state_t *s, bool review);
void ws_move(ws_state_t *s, int delta);
bool ws_answer(ws_state_t *s);
void ws_next(ws_state_t *s);
void ws_home(ws_state_t *s);
unsigned ws_count(uint64_t mask);
unsigned ws_stage(const ws_state_t *s);
uint8_t ws_word(const ws_state_t *s);
