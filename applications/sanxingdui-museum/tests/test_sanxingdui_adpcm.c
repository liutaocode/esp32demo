#include "sanxingdui_adpcm.h"
#include <assert.h>

int main(void)
{
    sanxingdui_adpcm_state_t state;
    sanxingdui_adpcm_init(&state, 0, 0);
    assert(sanxingdui_adpcm_decode(&state, 7) == 11);
    assert(state.step_index == 8);
    assert(sanxingdui_adpcm_decode(&state, 15) == -19);
    assert(state.step_index == 16);

    sanxingdui_adpcm_init(&state, 32760, 88);
    assert(sanxingdui_adpcm_decode(&state, 7) == 32767);
    assert(state.step_index == 88);
    return 0;
}
