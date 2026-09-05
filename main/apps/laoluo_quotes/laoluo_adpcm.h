#pragma once

#include <stdint.h>

typedef struct {
    int predictor;
    int step_index;
} laoluo_adpcm_state_t;

void laoluo_adpcm_init(laoluo_adpcm_state_t *state, int16_t predictor,
                       uint8_t step_index);
int16_t laoluo_adpcm_decode(laoluo_adpcm_state_t *state, uint8_t code);
