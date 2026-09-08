#include "sanxingdui_museum_state.h"
#include <assert.h>

int main(void)
{
    sanxingdui_museum_state_t state;
    sanxingdui_museum_state_init(&state);
    assert(state.page == 0);
    assert(!state.detail_view);

    sanxingdui_museum_state_toggle_detail(&state);
    assert(state.detail_view);

    sanxingdui_museum_state_move(&state, 1);
    assert(state.page == 1);
    assert(!state.detail_view);

    sanxingdui_museum_state_move(&state, -2);
    assert(state.page == SANXINGDUI_MUSEUM_PAGE_COUNT - 1);

    sanxingdui_museum_state_move(&state, SANXINGDUI_MUSEUM_PAGE_COUNT + 1);
    assert(state.page == 0);
    return 0;
}
