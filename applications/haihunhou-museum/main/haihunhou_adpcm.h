#pragma once

#include <stdint.h>

typedef struct {
    int predictor;
    int step_index;
} haihunhou_adpcm_state_t;

void haihunhou_adpcm_init(haihunhou_adpcm_state_t *state, int16_t predictor,
                         uint8_t step_index);
int16_t haihunhou_adpcm_decode(haihunhou_adpcm_state_t *state, uint8_t code);
