#pragma once

#include <stdbool.h>
#include <stddef.h>

#define HAIHUNHOU_MUSEUM_PAGE_COUNT 11

typedef struct {
    size_t page;
    bool detail_view;
} haihunhou_museum_state_t;

void haihunhou_museum_state_init(haihunhou_museum_state_t *state);
void haihunhou_museum_state_move(haihunhou_museum_state_t *state, int delta);
void haihunhou_museum_state_toggle_detail(haihunhou_museum_state_t *state);
