#pragma once

#include <stdbool.h>
#include <stddef.h>

#define SUZHOU_TRAVEL_ATTRACTION_COUNT 10

typedef struct {
    size_t index;
    bool detail_visible;
} suzhou_travel_state_t;

void suzhou_travel_state_init(suzhou_travel_state_t *state);
void suzhou_travel_state_move(suzhou_travel_state_t *state, int delta);
void suzhou_travel_state_toggle_detail(suzhou_travel_state_t *state);
