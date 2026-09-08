#include "haihunhou_adpcm.h"
#include <assert.h>

int main(void)
{
    haihunhou_adpcm_state_t state;
    haihunhou_adpcm_init(&state, 0, 0);
    assert(haihunhou_adpcm_decode(&state, 7) == 11);
    assert(state.step_index == 8);
    assert(haihunhou_adpcm_decode(&state, 15) == -19);
    assert(state.step_index == 16);

    haihunhou_adpcm_init(&state, 32760, 88);
    assert(haihunhou_adpcm_decode(&state, 7) == 32767);
    assert(state.step_index == 88);
    return 0;
}
