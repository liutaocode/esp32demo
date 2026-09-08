#include "suzhou_travel_state.h"
#include <assert.h>

int main(void)
{
    suzhou_travel_state_t state;
    suzhou_travel_state_init(&state);
    assert(state.index == 0);
    assert(!state.detail_visible);

    suzhou_travel_state_move(&state, -1);
    assert(state.index == SUZHOU_TRAVEL_ATTRACTION_COUNT - 1);

    suzhou_travel_state_toggle_detail(&state);
    assert(state.detail_visible);

    suzhou_travel_state_move(&state, 1);
    assert(state.index == 0);
    assert(!state.detail_visible);

    suzhou_travel_state_move(&state, 21);
    assert(state.index == 1);
    return 0;
}
