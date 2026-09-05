#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t index;
    uint16_t play_count;
} laoluo_quotes_state_t;

void laoluo_quotes_state_init(laoluo_quotes_state_t *state, uint32_t seed,
                              size_t quote_count);
void laoluo_quotes_state_move(laoluo_quotes_state_t *state, int delta,
                              size_t quote_count);
void laoluo_quotes_state_mark_played(laoluo_quotes_state_t *state);
