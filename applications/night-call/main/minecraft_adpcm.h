#pragma once

#include <stdint.h>

typedef struct {
    int predictor;
    int step_index;
} minecraft_adpcm_state_t;

void minecraft_adpcm_init(minecraft_adpcm_state_t *state, int16_t predictor,
                           uint8_t step_index);
int16_t minecraft_adpcm_decode(minecraft_adpcm_state_t *state, uint8_t code);
