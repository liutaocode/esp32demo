#include "memory_garden_state.h"
#include <assert.h>
#include <string.h>

static void begin_recall(memory_garden_state_t *state)
{
    assert(memory_garden_state_confirm(state, 0) == MEMORY_GARDEN_SHOWING);
    while (state->page == MEMORY_GARDEN_MEMORIZE) {
        memory_garden_state_advance(state);
    }
    assert(state->page == MEMORY_GARDEN_RECALL);
}

static void answer_round(memory_garden_state_t *state, bool correctly)
{
    uint8_t length = state->sequence_length;
    begin_recall(state);
    for (uint8_t index = 0; index < length; index++) {
        uint8_t choice = correctly
            ? state->sequence[index]
            : (uint8_t)((state->sequence[index] + 1U) %
                        MEMORY_GARDEN_SYMBOL_COUNT);
        while (state->selection != choice) memory_garden_state_move(state, 1);
        memory_garden_event_t result = memory_garden_state_confirm(state, 0);
        assert(result == (correctly ? MEMORY_GARDEN_REMEMBERED
                                    : MEMORY_GARDEN_COACHED));
        memory_garden_state_advance(state);
    }
    assert(state->page == MEMORY_GARDEN_ROUND_COMPLETE);
}

int main(void)
{
    memory_garden_state_t state;
    memory_garden_state_init(&state);
    assert(state.page == MEMORY_GARDEN_WELCOME);
    assert(memory_garden_state_confirm(&state, 0x12345678U) ==
           MEMORY_GARDEN_STARTED);
    assert(state.page == MEMORY_GARDEN_ROUND_READY);
    assert(state.sequence_length == 3U);
    for (uint8_t index = 1; index < state.sequence_length; index++) {
        assert(state.sequence[index] != state.sequence[index - 1U]);
    }

    answer_round(&state, true);
    assert(state.round_correct == 3U);
    assert(memory_garden_state_confirm(&state, 0) == MEMORY_GARDEN_NEXT_ROUND);
    assert(state.sequence_length == 4U);
    answer_round(&state, true);
    assert(memory_garden_state_confirm(&state, 0) == MEMORY_GARDEN_NEXT_ROUND);
    assert(state.sequence_length == 5U);
    answer_round(&state, true);
    assert(memory_garden_state_confirm(&state, 0) == MEMORY_GARDEN_FINISHED);
    assert(state.page == MEMORY_GARDEN_RESULT);
    assert(state.total_correct == MEMORY_GARDEN_TOTAL_ITEMS);
    assert(strcmp(memory_garden_result_title(&state), "繁花盛开") == 0);
    assert(state.share_code != 0U);

    assert(memory_garden_state_confirm(&state, 0x87654321U) ==
           MEMORY_GARDEN_RESTARTED);
    for (uint8_t round = 0; round < MEMORY_GARDEN_ROUND_COUNT; round++) {
        answer_round(&state, false);
        memory_garden_state_confirm(&state, 0);
    }
    assert(state.page == MEMORY_GARDEN_RESULT);
    assert(state.total_correct == 0U);
    assert(strcmp(memory_garden_result_title(&state), "茁壮生长") == 0);

    memory_garden_state_init(&state);
    memory_garden_state_move(&state, 1);
    assert(state.selection == 0U);
    return 0;
}
