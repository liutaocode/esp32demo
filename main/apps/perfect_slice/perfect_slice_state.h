#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { SLICE_WIDTH = 180, SLICE_ROUNDS = 10, SLICE_REVEAL_MS = 400 };
typedef enum { SLICE_HOME, SLICE_PLAY, SLICE_REVEAL, SLICE_PAUSED, SLICE_RESULT } slice_page_t;
typedef struct {
    slice_page_t page, resume;
    unsigned mode, round, target, cut, error, points, score;
    unsigned streak, perfects, best[2], reveal_ms;
    uint32_t phase; /* Millipixels along a reflected path. */
    bool perfect, new_best;
} slice_state_t;

void slice_home(slice_state_t *s);
void slice_start(slice_state_t *s);
void slice_tick(slice_state_t *s, uint32_t ms);
bool slice_cut(slice_state_t *s);
bool slice_next(slice_state_t *s);
void slice_pause(slice_state_t *s);
unsigned slice_position(const slice_state_t *s);
unsigned slice_target_x(const slice_state_t *s);
unsigned slice_speed(const slice_state_t *s);
