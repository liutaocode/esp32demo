#include "memory_garden_state.h"

static uint32_t mix(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    return value ^ (value >> 16);
}

static void generate_round(memory_garden_state_t *state)
{
    uint32_t value = mix(state->seed ^ (0x9e3779b9U * (state->round + 1U)));
    state->sequence_length = (uint8_t)(3U + state->round);
    for (uint8_t index = 0; index < state->sequence_length; index++) {
        value = value * 1664525U + 1013904223U;
        uint8_t symbol = (uint8_t)((value >> 16) % MEMORY_GARDEN_SYMBOL_COUNT);
        if (index > 0U && symbol == state->sequence[index - 1U]) {
            symbol = (uint8_t)((symbol + 1U + (value & 1U)) %
                               MEMORY_GARDEN_SYMBOL_COUNT);
        }
        state->sequence[index] = symbol;
    }
    state->position = 0;
    state->selection = 0;
    state->round_correct = 0;
}

static uint16_t make_share_code(const memory_garden_state_t *state)
{
    uint32_t value = mix(state->seed ^ ((uint32_t)state->total_correct << 24));
    return (uint16_t)(value & 0xffffU);
}

void memory_garden_state_init(memory_garden_state_t *state)
{
    *state = (memory_garden_state_t){
        .page = MEMORY_GARDEN_WELCOME,
    };
}

void memory_garden_state_move(memory_garden_state_t *state, int direction)
{
    if (state->page != MEMORY_GARDEN_RECALL || direction == 0) return;
    if (direction > 0) {
        state->selection = (uint8_t)((state->selection + 1U) %
                                     MEMORY_GARDEN_SYMBOL_COUNT);
    } else {
        state->selection = state->selection == 0U
            ? MEMORY_GARDEN_SYMBOL_COUNT - 1U
            : (uint8_t)(state->selection - 1U);
    }
}

memory_garden_event_t memory_garden_state_confirm(memory_garden_state_t *state,
                                                   uint32_t seed)
{
    bool restarting = state->page == MEMORY_GARDEN_RESULT;
    if (state->page == MEMORY_GARDEN_WELCOME ||
        state->page == MEMORY_GARDEN_RESULT) {
        memory_garden_state_init(state);
        state->seed = seed == 0U ? 0x6d2b79f5U : seed;
        state->page = MEMORY_GARDEN_ROUND_READY;
        generate_round(state);
        return restarting ? MEMORY_GARDEN_RESTARTED : MEMORY_GARDEN_STARTED;
    }

    if (state->page == MEMORY_GARDEN_ROUND_READY) {
        state->page = MEMORY_GARDEN_MEMORIZE;
        state->position = 0;
        return MEMORY_GARDEN_SHOWING;
    }

    if (state->page == MEMORY_GARDEN_RECALL) {
        state->last_expected = state->sequence[state->position];
        state->last_choice = state->selection;
        state->last_correct = state->last_choice == state->last_expected;
        if (state->last_correct) {
            state->round_correct++;
            state->total_correct++;
        }
        state->position++;
        state->page = MEMORY_GARDEN_ITEM_FEEDBACK;
        return state->last_correct ? MEMORY_GARDEN_REMEMBERED
                                   : MEMORY_GARDEN_COACHED;
    }

    if (state->page == MEMORY_GARDEN_ROUND_COMPLETE) {
        if (++state->round < MEMORY_GARDEN_ROUND_COUNT) {
            state->page = MEMORY_GARDEN_ROUND_READY;
            generate_round(state);
            return MEMORY_GARDEN_NEXT_ROUND;
        }
        state->share_code = make_share_code(state);
        state->page = MEMORY_GARDEN_RESULT;
        return MEMORY_GARDEN_FINISHED;
    }

    return MEMORY_GARDEN_NO_CHANGE;
}

memory_garden_event_t memory_garden_state_advance(memory_garden_state_t *state)
{
    if (state->page == MEMORY_GARDEN_MEMORIZE) {
        state->position++;
        if (state->position < state->sequence_length) {
            return MEMORY_GARDEN_SHOWING;
        }
        state->page = MEMORY_GARDEN_RECALL;
        state->position = 0;
        state->selection = 0;
        return MEMORY_GARDEN_RECALL_READY;
    }

    if (state->page == MEMORY_GARDEN_ITEM_FEEDBACK) {
        if (state->position < state->sequence_length) {
            state->page = MEMORY_GARDEN_RECALL;
            state->selection = 0;
            return MEMORY_GARDEN_NEXT_ITEM;
        }
        state->page = MEMORY_GARDEN_ROUND_COMPLETE;
        return MEMORY_GARDEN_ROUND_FINISHED;
    }

    return MEMORY_GARDEN_NO_CHANGE;
}

const char *memory_garden_result_title(const memory_garden_state_t *state)
{
    if (state->total_correct >= 10U) return "繁花盛开";
    if (state->total_correct >= 7U) return "花园明亮";
    return "茁壮生长";
}
