#pragma once

#include <stdint.h>

#define TOMATO_BLOOM_PRESET_COUNT 3U
#define TOMATO_BLOOM_SET_SIZE 4U

typedef enum {
    TOMATO_BLOOM_SETUP = 0,
    TOMATO_BLOOM_FOCUS,
    TOMATO_BLOOM_FOCUS_PAUSED,
    TOMATO_BLOOM_HARVEST,
    TOMATO_BLOOM_BREAK,
    TOMATO_BLOOM_BREAK_PAUSED,
    TOMATO_BLOOM_READY,
} tomato_bloom_page_t;

typedef enum {
    TOMATO_BLOOM_NO_CHANGE = 0,
    TOMATO_BLOOM_PRESET_CHANGED,
    TOMATO_BLOOM_FOCUS_STARTED,
    TOMATO_BLOOM_PAUSED,
    TOMATO_BLOOM_RESUMED,
    TOMATO_BLOOM_FOCUS_FINISHED,
    TOMATO_BLOOM_BREAK_STARTED,
    TOMATO_BLOOM_BREAK_FINISHED,
    TOMATO_BLOOM_RESET,
} tomato_bloom_event_t;

typedef struct {
    tomato_bloom_page_t page;
    uint8_t preset;
    uint8_t completed_sessions;
    uint32_t phase_total_seconds;
    uint32_t remaining_seconds;
    uint32_t focused_minutes;
} tomato_bloom_state_t;

void tomato_bloom_state_init(tomato_bloom_state_t *state);
tomato_bloom_event_t tomato_bloom_state_move(tomato_bloom_state_t *state,
                                               int direction);
tomato_bloom_event_t tomato_bloom_state_confirm(tomato_bloom_state_t *state);
tomato_bloom_event_t tomato_bloom_state_tick(tomato_bloom_state_t *state,
                                              uint32_t elapsed_seconds);
tomato_bloom_event_t tomato_bloom_state_reset_timer(tomato_bloom_state_t *state);
uint8_t tomato_bloom_focus_minutes(const tomato_bloom_state_t *state);
uint8_t tomato_bloom_next_break_minutes(const tomato_bloom_state_t *state);
uint16_t tomato_bloom_progress_per_mille(const tomato_bloom_state_t *state);
uint8_t tomato_bloom_growth_stage(const tomato_bloom_state_t *state);
const char *tomato_bloom_rank(const tomato_bloom_state_t *state);
