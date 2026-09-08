#include "sanxingdui_adpcm.h"

void sanxingdui_adpcm_init(sanxingdui_adpcm_state_t *state, int16_t predictor,
                         uint8_t step_index)
{
    if (!state) return;
    state->predictor = predictor;
    state->step_index = step_index > 88 ? 88 : step_index;
}

int16_t sanxingdui_adpcm_decode(sanxingdui_adpcm_state_t *state, uint8_t code)
{
    static const int16_t steps[89] = {
        7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,
        66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,
        371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,
        1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,
        5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,
        16818,18500,20350,22385,24623,27086,29794,32767,
    };
    static const int8_t index_change[16] = {
        -1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8,
    };
    if (!state) return 0;

    code &= 0x0f;
    const int step = steps[state->step_index];
    int delta = step >> 3;
    if (code & 4) delta += step;
    if (code & 2) delta += step >> 1;
    if (code & 1) delta += step >> 2;

    state->predictor += (code & 8) ? -delta : delta;
    if (state->predictor > 32767) state->predictor = 32767;
    if (state->predictor < -32768) state->predictor = -32768;

    state->step_index += index_change[code];
    if (state->step_index < 0) state->step_index = 0;
    if (state->step_index > 88) state->step_index = 88;
    return (int16_t)state->predictor;
}
