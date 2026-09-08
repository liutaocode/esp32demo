#pragma once

#include <stdint.h>

typedef struct {
    int predictor;
    int step_index;
} six_arts_adpcm_state_t;

void six_arts_adpcm_init(six_arts_adpcm_state_t *state, int16_t predictor,
                         uint8_t step_index);
int16_t six_arts_adpcm_decode(six_arts_adpcm_state_t *state, uint8_t code);
