#include "laoluo_quotes_state.h"
#include <limits.h>

void laoluo_quotes_state_init(laoluo_quotes_state_t *state, uint32_t seed,
                              size_t quote_count)
{
    if (!state) return;
    *state = (laoluo_quotes_state_t){
        .index = quote_count == 0 ? 0 : seed % quote_count,
    };
}

void laoluo_quotes_state_move(laoluo_quotes_state_t *state, int delta,
                              size_t quote_count)
{
    if (!state || quote_count == 0 || delta == 0) return;
    int next = (int)state->index + delta;
    while (next < 0) next += (int)quote_count;
    state->index = (size_t)(next % (int)quote_count);
}

void laoluo_quotes_state_mark_played(laoluo_quotes_state_t *state)
{
    if (state && state->play_count < UINT16_MAX) state->play_count++;
}
