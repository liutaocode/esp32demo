#include "tomato_bloom_state.h"

#include <stddef.h>

static const uint8_t FOCUS_MINUTES[TOMATO_BLOOM_PRESET_COUNT] = { 15, 25, 45 };

static void start_focus(tomato_bloom_state_t *state)
{
    state->page = TOMATO_BLOOM_FOCUS;
    state->phase_total_seconds =
        (uint32_t)tomato_bloom_focus_minutes(state) * 60U;
    state->remaining_seconds = state->phase_total_seconds;
}

void tomato_bloom_state_init(tomato_bloom_state_t *state)
{
    if (!state) return;
    *state = (tomato_bloom_state_t){
        .page = TOMATO_BLOOM_SETUP,
        .preset = 1U,
    };
}

uint8_t tomato_bloom_focus_minutes(const tomato_bloom_state_t *state)
{
    if (!state || state->preset >= TOMATO_BLOOM_PRESET_COUNT) return 25U;
    return FOCUS_MINUTES[state->preset];
}

uint8_t tomato_bloom_next_break_minutes(const tomato_bloom_state_t *state)
{
    if (!state || state->completed_sessions == 0U) return 5U;
    return state->completed_sessions % TOMATO_BLOOM_SET_SIZE == 0U ? 15U : 5U;
}

tomato_bloom_event_t tomato_bloom_state_move(tomato_bloom_state_t *state,
                                               int direction)
{
    if (!state || state->page != TOMATO_BLOOM_SETUP || direction == 0) {
        return TOMATO_BLOOM_NO_CHANGE;
    }

    if (direction > 0) {
        state->preset = (uint8_t)((state->preset + 1U) %
                                  TOMATO_BLOOM_PRESET_COUNT);
    } else {
        state->preset = state->preset == 0U
            ? TOMATO_BLOOM_PRESET_COUNT - 1U
            : (uint8_t)(state->preset - 1U);
    }
    return TOMATO_BLOOM_PRESET_CHANGED;
}

tomato_bloom_event_t tomato_bloom_state_confirm(tomato_bloom_state_t *state)
{
    if (!state) return TOMATO_BLOOM_NO_CHANGE;

    switch (state->page) {
    case TOMATO_BLOOM_SETUP:
    case TOMATO_BLOOM_READY:
        start_focus(state);
        return TOMATO_BLOOM_FOCUS_STARTED;
    case TOMATO_BLOOM_FOCUS:
        state->page = TOMATO_BLOOM_FOCUS_PAUSED;
        return TOMATO_BLOOM_PAUSED;
    case TOMATO_BLOOM_FOCUS_PAUSED:
        state->page = TOMATO_BLOOM_FOCUS;
        return TOMATO_BLOOM_RESUMED;
    case TOMATO_BLOOM_HARVEST:
        state->page = TOMATO_BLOOM_BREAK;
        state->phase_total_seconds =
            (uint32_t)tomato_bloom_next_break_minutes(state) * 60U;
        state->remaining_seconds = state->phase_total_seconds;
        return TOMATO_BLOOM_BREAK_STARTED;
    case TOMATO_BLOOM_BREAK:
        state->page = TOMATO_BLOOM_BREAK_PAUSED;
        return TOMATO_BLOOM_PAUSED;
    case TOMATO_BLOOM_BREAK_PAUSED:
        state->page = TOMATO_BLOOM_BREAK;
        return TOMATO_BLOOM_RESUMED;
    default:
        return TOMATO_BLOOM_NO_CHANGE;
    }
}

tomato_bloom_event_t tomato_bloom_state_tick(tomato_bloom_state_t *state,
                                              uint32_t elapsed_seconds)
{
    if (!state || elapsed_seconds == 0U ||
        (state->page != TOMATO_BLOOM_FOCUS &&
         state->page != TOMATO_BLOOM_BREAK)) {
        return TOMATO_BLOOM_NO_CHANGE;
    }

    if (elapsed_seconds < state->remaining_seconds) {
        state->remaining_seconds -= elapsed_seconds;
        return TOMATO_BLOOM_NO_CHANGE;
    }

    state->remaining_seconds = 0U;
    if (state->page == TOMATO_BLOOM_FOCUS) {
        if (state->completed_sessions < UINT8_MAX) state->completed_sessions++;
        uint32_t total = state->focused_minutes + tomato_bloom_focus_minutes(state);
        state->focused_minutes = total < state->focused_minutes ? UINT32_MAX : total;
        state->page = TOMATO_BLOOM_HARVEST;
        return TOMATO_BLOOM_FOCUS_FINISHED;
    }

    state->page = TOMATO_BLOOM_READY;
    return TOMATO_BLOOM_BREAK_FINISHED;
}

tomato_bloom_event_t tomato_bloom_state_reset_timer(tomato_bloom_state_t *state)
{
    if (!state || state->page == TOMATO_BLOOM_SETUP) {
        return TOMATO_BLOOM_NO_CHANGE;
    }
    state->page = TOMATO_BLOOM_SETUP;
    state->phase_total_seconds = 0U;
    state->remaining_seconds = 0U;
    return TOMATO_BLOOM_RESET;
}

uint16_t tomato_bloom_progress_per_mille(const tomato_bloom_state_t *state)
{
    if (!state || state->phase_total_seconds == 0U) return 0U;
    uint32_t remaining = state->remaining_seconds > state->phase_total_seconds
        ? state->phase_total_seconds : state->remaining_seconds;
    uint32_t elapsed = state->phase_total_seconds - remaining;
    return (uint16_t)((elapsed * 1000U) / state->phase_total_seconds);
}

uint8_t tomato_bloom_growth_stage(const tomato_bloom_state_t *state)
{
    uint16_t progress = tomato_bloom_progress_per_mille(state);
    uint8_t stage = (uint8_t)(progress / 200U);
    return stage > 4U ? 4U : stage;
}

const char *tomato_bloom_rank(const tomato_bloom_state_t *state)
{
    if (!state || state->completed_sessions == 0U) return "播种新手";
    if (state->completed_sessions < 4U) return "初次发芽";
    if (state->completed_sessions < 8U) return "专注农夫";
    if (state->completed_sessions < 16U) return "花园守护者";
    return "深深扎根";
}
