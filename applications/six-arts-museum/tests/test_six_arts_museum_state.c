#include "six_arts_museum_state.h"
#include <assert.h>

int main(void)
{
    six_arts_museum_state_t state;
    six_arts_museum_state_init(&state);
    assert(state.page == 0);
    assert(!state.detail_view);

    six_arts_museum_state_toggle_detail(&state);
    assert(state.detail_view);

    six_arts_museum_state_move(&state, 1);
    assert(state.page == 1);
    assert(!state.detail_view);

    six_arts_museum_state_move(&state, -2);
    assert(state.page == SIX_ARTS_MUSEUM_PAGE_COUNT - 1);

    six_arts_museum_state_move(&state, SIX_ARTS_MUSEUM_PAGE_COUNT + 1);
    assert(state.page == 0);
    return 0;
}
