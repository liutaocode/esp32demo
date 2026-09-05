#include "fruit_merge_state.h"
#include <string.h>

static uint8_t draw(fm_board_t *b)
{
    uint32_t x = b->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    b->rng = x;
    /* Fixed distribution keeps same-seed replay independent of player score. */
    unsigned n = x % 10;
    return n < 5 ? 1 : n < 8 ? 2 : 3;
}
void fm_init(fm_state_t *s) { memset(s, 0, sizeof(*s)); s->page = FM_HOME; }
void fm_start(fm_state_t *s, uint32_t seed)
{
    uint32_t best = s->best;
    uint8_t collection = s->collection;
    fm_init(s); s->best = best; s->collection = collection;
    s->seed = seed ? seed : 1; s->board.rng = s->seed;
    s->board.current = 1; s->board.next = 1;
    s->page = FM_PLAY; s->has_game = true;
}
int fm_landing(const fm_state_t *s, unsigned col)
{
    if (col >= FM_COLS) return -1;
    for (int r = 0; r < FM_ROWS; r++) if (!s->board.cells[r][col]) return r;
    return -1;
}
void fm_select(fm_state_t *s, int direction)
{
    if (s->page != FM_PLAY || s->busy) return;
    s->board.selected = (s->board.selected + (direction < 0 ? FM_COLS - 1 : 1)) % FM_COLS;
    s->full_notice = false;
}
bool fm_drop(fm_state_t *s)
{
    if (s->page != FM_PLAY || s->busy) return false;
    int row = fm_landing(s, s->board.selected);
    if (row < 0) { s->full_notice = true; return false; }
    s->previous = s->board; s->has_previous = true;
    s->focus_row = (uint8_t)row; s->focus_col = s->board.selected;
    s->board.cells[row][s->focus_col] = s->board.current;
    if (s->board.current > s->board.peak) s->board.peak = s->board.current;
    s->collection |= (uint8_t)(1u << (s->board.current - 1));
    if (s->board.turns < UINT16_MAX) s->board.turns++;
    s->chain = 0; s->busy = true; s->full_notice = false;
    return true;
}
static void gravity(fm_state_t *s)
{
    for (unsigned c = 0; c < FM_COLS; c++) {
        unsigned write = 0;
        for (unsigned r = 0; r < FM_ROWS; r++) {
            uint8_t v = s->board.cells[r][c];
            if (!v) continue;
            s->board.cells[r][c] = 0; s->board.cells[write][c] = v;
            if (r == s->focus_row && c == s->focus_col) s->focus_row = write;
            write++;
        }
    }
}
static bool neighbor(const fm_state_t *s, int r, int c, int *rr, int *cc)
{
    static const int dr[] = {-1, 0, 0, 1}, dc[] = {0, -1, 1, 0};
    uint8_t v = s->board.cells[r][c];
    if (!v) return false;
    for (unsigned i = 0; i < 4; i++) {
        int y = r + dr[i], x = c + dc[i];
        if (y >= 0 && y < FM_ROWS && x >= 0 && x < FM_COLS && s->board.cells[y][x] == v) {
            *rr = y; *cc = x; return true;
        }
    }
    return false;
}
bool fm_step(fm_state_t *s)
{
    if (!s->busy || s->page != FM_PLAY) return false;
    int r = s->focus_row, c = s->focus_col, rr = 0, cc = 0;
    bool found = neighbor(s, r, c, &rr, &cc);
    /* First extend the active chain, then resolve other contacts made by gravity. */
    if (!found) {
        for (r = 0; r < FM_ROWS && !found; r++)
            for (c = 0; c < FM_COLS; c++)
                if (neighbor(s, r, c, &rr, &cc)) { found = true; break; }
        r--;
    }
    if (found) {
        uint8_t v = s->board.cells[r][c];
        s->board.cells[rr][cc] = 0;
        s->board.cells[r][c] = v == FM_TOP ? 0 : v + 1;
        s->focus_row = r; s->focus_col = c;
        if (v == FM_TOP && s->board.melons < UINT16_MAX) s->board.melons++;
        if (v < FM_TOP) {
            s->collection |= (uint8_t)(1u << v);
            if (v + 1 > s->board.peak) s->board.peak = v + 1;
        }
        s->chain++;
        if (s->chain > s->max_chain) s->max_chain = s->chain;
        uint32_t points = (1u << (v + 1)) * s->chain;
        s->board.score = s->board.score > FM_SCORE_MAX - points ? FM_SCORE_MAX : s->board.score + points;
        if (s->board.score > s->best) s->best = s->board.score;
        gravity(s);
        return true;
    }
    s->busy = false;
    s->board.current = s->board.next; s->board.next = draw(&s->board);
    bool space = false;
    for (unsigned col = 0; col < FM_COLS; col++) if (fm_landing(s, col) >= 0) space = true;
    if (!space) s->page = FM_RESULT;
    return true;
}
bool fm_undo(fm_state_t *s)
{
    if (s->busy || s->undo_used || !s->has_previous ||
        (s->page != FM_PAUSE && s->page != FM_RESULT)) return false;
    s->board = s->previous; s->undo_used = true; s->chain = 0;
    s->full_notice = false; s->page = FM_PLAY;
    return true;
}
void fm_pause(fm_state_t *s)
{
    if (s->page == FM_PLAY) s->page = FM_PAUSE;
}
void fm_resume(fm_state_t *s)
{
    if (s->page == FM_PAUSE || (s->page == FM_HOME && s->has_game)) s->page = FM_PLAY;
}
bool fm_valid(const fm_state_t *s)
{
    if (!s->has_game) return true;
    const fm_board_t *b = &s->board;
    if (b->selected >= FM_COLS || b->current < 1 || b->current > 3 || b->next < 1 || b->next > 3 ||
        b->peak > FM_TOP || b->score > FM_SCORE_MAX || s->focus_row >= FM_ROWS || s->focus_col >= FM_COLS) return false;
    for (unsigned c = 0; c < FM_COLS; c++) {
        bool empty = false;
        for (unsigned r = 0; r < FM_ROWS; r++) {
            if (b->cells[r][c] > FM_TOP || (empty && b->cells[r][c])) return false;
            if (!b->cells[r][c]) empty = true;
        }
    }
    return true;
}
