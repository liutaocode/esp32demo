#include "six_arts_museum_state.h"

void six_arts_museum_state_init(six_arts_museum_state_t *state)
{
    if (!state) return;
    state->page = 0;
    state->detail_view = false;
}

void six_arts_museum_state_move(six_arts_museum_state_t *state, int delta)
{
    if (!state) return;

    int next = (int)state->page + delta;
    while (next < 0) next += SIX_ARTS_MUSEUM_PAGE_COUNT;
    state->page = (size_t)(next % SIX_ARTS_MUSEUM_PAGE_COUNT);
    state->detail_view = false;
}

void six_arts_museum_state_toggle_detail(six_arts_museum_state_t *state)
{
    if (state) state->detail_view = !state->detail_view;
}
