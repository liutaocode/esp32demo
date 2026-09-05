#include "minecraft_guide_state.h"

void minecraft_guide_state_init(minecraft_guide_state_t *state)
{
    if (state) state->index = 0;
}
void minecraft_guide_state_move(minecraft_guide_state_t *state, int delta)
{
    if (!state) return;

    int next = (int)state->index + delta;
    while (next < 0) next += MINECRAFT_GUIDE_ITEM_COUNT;
    state->index = (size_t)(next % MINECRAFT_GUIDE_ITEM_COUNT);
}
