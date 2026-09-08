#pragma once

#include <stddef.h>

#define MINECRAFT_GUIDE_ITEM_COUNT 20

typedef struct {
    size_t index;
} minecraft_guide_state_t;

void minecraft_guide_state_init(minecraft_guide_state_t *state);
void minecraft_guide_state_move(minecraft_guide_state_t *state, int delta);
