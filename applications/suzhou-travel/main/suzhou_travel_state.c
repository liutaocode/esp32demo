#include "suzhou_travel_state.h"

void suzhou_travel_state_init(suzhou_travel_state_t *state)
{
    if (!state) {
        return;
    }
    state->index = 0;
    state->detail_visible = false;
}

void suzhou_travel_state_move(suzhou_travel_state_t *state, int delta)
{
    if (!state) {
        return;
    }

    int next = (int)state->index + delta;
    while (next < 0) {
        next += SUZHOU_TRAVEL_ATTRACTION_COUNT;
    }
    state->index = (size_t)(next % SUZHOU_TRAVEL_ATTRACTION_COUNT);
    state->detail_visible = false;
}

void suzhou_travel_state_toggle_detail(suzhou_travel_state_t *state)
{
    if (state) {
        state->detail_visible = !state->detail_visible;
    }
}
