#pragma once

#include <stdint.h>

typedef struct {
    int predictor;
    int step_index;
} sanxingdui_adpcm_state_t;

void sanxingdui_adpcm_init(sanxingdui_adpcm_state_t *state, int16_t predictor,
                         uint8_t step_index);
int16_t sanxingdui_adpcm_decode(sanxingdui_adpcm_state_t *state, uint8_t code);
