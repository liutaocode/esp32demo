#pragma once

#include <stdbool.h>
#include <stddef.h>

#define SANXINGDUI_MUSEUM_PAGE_COUNT 11

typedef struct {
    size_t page;
    bool detail_view;
} sanxingdui_museum_state_t;

void sanxingdui_museum_state_init(sanxingdui_museum_state_t *state);
void sanxingdui_museum_state_move(sanxingdui_museum_state_t *state, int delta);
void sanxingdui_museum_state_toggle_detail(sanxingdui_museum_state_t *state);
