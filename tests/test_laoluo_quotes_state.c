#include "laoluo_adpcm.h"
#include "laoluo_quotes_state.h"
#include <assert.h>
#include <stdint.h>

int main(void)
{
    laoluo_quotes_state_t state;
    laoluo_quotes_state_init(&state, 14U, 12U);
    assert(state.index == 2U);
    assert(state.play_count == 0U);

    laoluo_quotes_state_move(&state, -3, 12U);
    assert(state.index == 11U);
    laoluo_quotes_state_move(&state, 2, 12U);
    assert(state.index == 1U);
    laoluo_quotes_state_mark_played(&state);
    assert(state.play_count == 1U);

    laoluo_quotes_state_init(&state, 99U, 0U);
    laoluo_quotes_state_move(&state, 1, 0U);
    assert(state.index == 0U);

    laoluo_adpcm_state_t decoder;
    laoluo_adpcm_init(&decoder, 0, 0);
    assert(laoluo_adpcm_decode(&decoder, 7) == 11);
    assert(laoluo_adpcm_decode(&decoder, 15) == -19);
    return 0;
}
