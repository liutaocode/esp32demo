#pragma once

#include <stdbool.h>
#include <stddef.h>

#define SIX_ARTS_MUSEUM_PAGE_COUNT 11

typedef struct {
    size_t page;
    bool detail_view;
} six_arts_museum_state_t;

void six_arts_museum_state_init(six_arts_museum_state_t *state);
void six_arts_museum_state_move(six_arts_museum_state_t *state, int delta);
void six_arts_museum_state_toggle_detail(six_arts_museum_state_t *state);
