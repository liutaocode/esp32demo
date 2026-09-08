#pragma once

#include <stdint.h>

typedef struct {
    int predictor;
    int step_index;
} suzhou_adpcm_state_t;

void suzhou_adpcm_init(suzhou_adpcm_state_t *state, int16_t predictor,
                       uint8_t step_index);
int16_t suzhou_adpcm_decode(suzhou_adpcm_state_t *state, uint8_t code);
