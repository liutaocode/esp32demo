#include "minecraft_guide_state.h"
#include <assert.h>

int main(void)
{
    minecraft_guide_state_t state;
    minecraft_guide_state_init(&state);
    assert(state.index == 0);

    minecraft_guide_state_move(&state, 1);
    assert(state.index == 1);

    minecraft_guide_state_move(&state, -1);
    assert(state.index == 0);

    minecraft_guide_state_move(&state, -1);
    assert(state.index == MINECRAFT_GUIDE_ITEM_COUNT - 1);

    minecraft_guide_state_move(&state, MINECRAFT_GUIDE_ITEM_COUNT + 1);
    assert(state.index == 0);
    return 0;
}
