#include "tomato_bloom_state.h"

#include <assert.h>
#include <string.h>

static void finish_focus_and_break(tomato_bloom_state_t *state)
{
    assert(state->page == TOMATO_BLOOM_FOCUS);
    assert(tomato_bloom_state_tick(state, state->remaining_seconds) ==
           TOMATO_BLOOM_FOCUS_FINISHED);
    assert(state->page == TOMATO_BLOOM_HARVEST);
    assert(tomato_bloom_state_confirm(state) == TOMATO_BLOOM_BREAK_STARTED);
    assert(tomato_bloom_state_tick(state, state->remaining_seconds) ==
           TOMATO_BLOOM_BREAK_FINISHED);
    assert(state->page == TOMATO_BLOOM_READY);
}

int main(void)
{
    tomato_bloom_state_t state;
    tomato_bloom_state_init(&state);
    assert(state.page == TOMATO_BLOOM_SETUP);
    assert(state.preset == 1U);
    assert(tomato_bloom_focus_minutes(&state) == 25U);
    assert(strcmp(tomato_bloom_rank(&state), "播种新手") == 0);

    assert(tomato_bloom_state_move(&state, -1) ==
           TOMATO_BLOOM_PRESET_CHANGED);
    assert(tomato_bloom_focus_minutes(&state) == 15U);
    tomato_bloom_state_move(&state, -1);
    assert(tomato_bloom_focus_minutes(&state) == 45U);
    tomato_bloom_state_move(&state, 1);
    tomato_bloom_state_move(&state, 1);
    assert(tomato_bloom_focus_minutes(&state) == 25U);

    assert(tomato_bloom_state_confirm(&state) == TOMATO_BLOOM_FOCUS_STARTED);
    assert(state.remaining_seconds == 25U * 60U);
    assert(tomato_bloom_progress_per_mille(&state) == 0U);
    assert(tomato_bloom_growth_stage(&state) == 0U);
    tomato_bloom_state_tick(&state, 5U * 60U);
    assert(tomato_bloom_progress_per_mille(&state) == 200U);
    assert(tomato_bloom_growth_stage(&state) == 1U);

    assert(tomato_bloom_state_confirm(&state) == TOMATO_BLOOM_PAUSED);
    uint32_t paused_at = state.remaining_seconds;
    assert(tomato_bloom_state_tick(&state, 999U) == TOMATO_BLOOM_NO_CHANGE);
    assert(state.remaining_seconds == paused_at);
    assert(tomato_bloom_state_confirm(&state) == TOMATO_BLOOM_RESUMED);
    assert(tomato_bloom_state_tick(&state, paused_at) ==
           TOMATO_BLOOM_FOCUS_FINISHED);
    assert(state.completed_sessions == 1U);
    assert(state.focused_minutes == 25U);
    assert(strcmp(tomato_bloom_rank(&state), "初次发芽") == 0);
    assert(tomato_bloom_next_break_minutes(&state) == 5U);

    assert(tomato_bloom_state_confirm(&state) == TOMATO_BLOOM_BREAK_STARTED);
    assert(state.remaining_seconds == 5U * 60U);
    assert(tomato_bloom_state_confirm(&state) == TOMATO_BLOOM_PAUSED);
    assert(tomato_bloom_state_confirm(&state) == TOMATO_BLOOM_RESUMED);
    assert(tomato_bloom_state_tick(&state, 5U * 60U) ==
           TOMATO_BLOOM_BREAK_FINISHED);
    assert(tomato_bloom_state_confirm(&state) == TOMATO_BLOOM_FOCUS_STARTED);

    finish_focus_and_break(&state);
    tomato_bloom_state_confirm(&state);
    finish_focus_and_break(&state);
    tomato_bloom_state_confirm(&state);
    assert(tomato_bloom_state_tick(&state, state.remaining_seconds) ==
           TOMATO_BLOOM_FOCUS_FINISHED);
    assert(state.completed_sessions == TOMATO_BLOOM_SET_SIZE);
    assert(tomato_bloom_next_break_minutes(&state) == 15U);
    assert(strcmp(tomato_bloom_rank(&state), "专注农夫") == 0);
    tomato_bloom_state_confirm(&state);
    assert(state.remaining_seconds == 15U * 60U);

    assert(tomato_bloom_state_reset_timer(&state) == TOMATO_BLOOM_RESET);
    assert(state.page == TOMATO_BLOOM_SETUP);
    assert(state.completed_sessions == TOMATO_BLOOM_SET_SIZE);
    assert(state.focused_minutes == 100U);
    assert(tomato_bloom_state_reset_timer(&state) == TOMATO_BLOOM_NO_CHANGE);

    tomato_bloom_state_t empty = { 0 };
    assert(tomato_bloom_progress_per_mille(&empty) == 0U);
    assert(tomato_bloom_next_break_minutes(&empty) == 5U);
    return 0;
}
