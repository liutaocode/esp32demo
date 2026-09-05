#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { FM_COLS = 4, FM_ROWS = 5, FM_TOP = 8, FM_SCORE_MAX = 999999 };
typedef enum { FM_HOME, FM_RULES, FM_PLAY, FM_PAUSE, FM_RESULT, FM_BOOK } fm_page_t;
typedef struct {
    uint8_t cells[FM_ROWS][FM_COLS]; /* row zero is the floor */
    uint8_t selected, current, next, peak;
    uint16_t turns, melons;
    uint32_t rng, score;
} fm_board_t;
typedef struct {
    fm_board_t board, previous;
    fm_page_t page, return_page;
    uint32_t seed, best;
    uint8_t collection, chain, max_chain, focus_row, focus_col;
    bool busy, undo_used, has_previous, has_game, full_notice;
} fm_state_t;

void fm_init(fm_state_t *s);
void fm_start(fm_state_t *s, uint32_t seed);
void fm_select(fm_state_t *s, int direction);
int fm_landing(const fm_state_t *s, unsigned col);
bool fm_drop(fm_state_t *s);
/* One deterministic pair per step. Gravity runs after each pair. */
bool fm_step(fm_state_t *s);
bool fm_undo(fm_state_t *s);
void fm_pause(fm_state_t *s);
void fm_resume(fm_state_t *s);
bool fm_valid(const fm_state_t *s);
