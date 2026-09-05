#include "fruit_merge_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void settle(fm_state_t *s)
{
    unsigned steps = 0;
    while (s->busy) { assert(fm_step(s)); assert(fm_valid(s)); assert(++steps <= 21); }
}
static unsigned mass(const fm_state_t *s)
{
    unsigned total = s->board.melons * 512;
    for (unsigned r = 0; r < FM_ROWS; r++) for (unsigned c = 0; c < FM_COLS; c++)
        if (s->board.cells[r][c]) total += 1u << s->board.cells[r][c];
    return total;
}
int main(void)
{
    fm_state_t s, other;
    fm_init(&s); assert(s.page == FM_HOME && fm_valid(&s));
    fm_start(&s, 0); assert(s.seed == 1 && s.board.current == 1 && s.board.next == 1);
    assert(fm_landing(&s, 4) == -1);
    fm_select(&s, -1); assert(s.board.selected == 3);
    fm_select(&s, 1); assert(s.board.selected == 0);
    assert(fm_drop(&s)); assert(!fm_drop(&s)); settle(&s);
    assert(s.board.cells[0][0] == 1 && s.board.turns == 1);
    fm_board_t before = s.board;
    assert(fm_drop(&s)); settle(&s);
    assert(s.board.cells[0][0] == 2 && s.board.score == 4 && s.chain == 1);
    fm_pause(&s); assert(fm_undo(&s)); assert(!memcmp(&s.board, &before, sizeof(before)));
    fm_pause(&s); assert(!fm_undo(&s)); fm_resume(&s);
    /* A vertical merge falls into a horizontal pair and chains again. */
    fm_start(&s, 4); s.board.cells[0][0] = 1; s.board.cells[0][1] = 2;
    assert(fm_drop(&s)); settle(&s);
    assert(s.chain == 2 && s.board.cells[0][0] == 3 && !s.board.cells[0][1]);
    assert(s.board.score == 20 && s.board.peak == 3);
    /* Multiple candidates choose below, then left, then right, then above. */
    fm_start(&s, 4); s.board.cells[0][0] = 1; s.board.cells[0][2] = 1; s.board.selected = 1;
    assert(fm_drop(&s)); assert(fm_step(&s));
    assert(!s.board.cells[0][0] && s.board.cells[0][1] == 2 && s.board.cells[0][2] == 1);
    settle(&s);
    /* Top-tier pairs disappear; score is bounded and mass is accounted for. */
    fm_start(&s, 4); s.board.cells[0][0] = 8; s.board.cells[0][1] = 8;
    s.busy = true; s.board.score = FM_SCORE_MAX - 1; s.board.peak = 8;
    settle(&s); assert(!s.board.cells[0][0] && s.board.melons == 1 && s.board.score == FM_SCORE_MAX);
    /* A full column is rejected without advancing RNG, score, turn, or undo. */
    fm_start(&s, 42);
    for (unsigned r = 0; r < FM_ROWS; r++) s.board.cells[r][0] = 1 + r % 2;
    before = s.board; assert(!fm_drop(&s)); assert(s.full_notice);
    assert(!memcmp(&before, &s.board, sizeof(before)) && !s.has_previous);
    fm_select(&s, 1); assert(!s.full_notice && fm_drop(&s));
    fm_pause(&s); assert(!fm_step(&s)); assert(!fm_undo(&s));
    fm_resume(&s); settle(&s);
    /* Filling the last cell ends only after all possible rescue merges. */
    fm_start(&s, 42);
    for (unsigned r = 0; r < FM_ROWS; r++) for (unsigned c = 0; c < FM_COLS; c++)
        s.board.cells[r][c] = (r + c) % 2 + 2;
    s.board.cells[4][0] = 0; s.board.current = 1;
    assert(fm_drop(&s)); settle(&s); assert(s.page == FM_RESULT);
    assert(fm_undo(&s) && s.page == FM_PLAY && fm_landing(&s, 0) == 4);
    /* Identical inputs replay every draw and board, including after undo. */
    unsigned turns = 0, highest = 0;
    for (uint32_t seed = 1; seed <= 1000; seed++) {
        fm_init(&s); fm_init(&other); fm_start(&s, seed); fm_start(&other, seed);
        uint32_t chooser = seed;
        for (unsigned i = 0; i < 1000 && s.page == FM_PLAY; i++) {
            chooser = chooser * 1664525u + 1013904223u;
            unsigned col = (chooser >> 16) % FM_COLS;
            while (fm_landing(&s, col) < 0) col = (col + 1) % FM_COLS;
            s.board.selected = other.board.selected = col;
            unsigned old_mass = mass(&s), incoming = 1u << s.board.current;
            assert(fm_drop(&s) && fm_drop(&other)); settle(&s); settle(&other);
            assert(mass(&s) == old_mass + incoming);
            assert(!memcmp(&s.board, &other.board, sizeof(s.board)));
            turns++; if (s.board.peak > highest) highest = s.board.peak;
        }
    }
    printf("Fruit Merge: rules, chains, gravity, undo, replay, bounds and %u randomized turns PASS (peak %u)\n", turns, highest);
}
