#include "vibe_check_state.h"
#include <assert.h>

static void choose(vibe_check_state_t *state, uint8_t choice, uint32_t entropy)
{
    vibe_check_state_move(state, choice ? 1 : -1);
    vibe_check_state_confirm(state, entropy);
}

static uint8_t expected_result(uint8_t answers)
{
    uint8_t energetic = ((answers & 1U) != 0U) || ((answers & 8U) != 0U);
    uint8_t maker = ((answers & 2U) != 0U) || ((answers & 16U) != 0U);
    uint8_t bold = (answers & 4U) != 0U;
    return (uint8_t)(energetic | (maker << 1) | (bold << 2));
}

int main(void)
{
    vibe_check_state_t state;
    vibe_check_state_init(&state);
    assert(state.page == VIBE_CHECK_WELCOME);
    assert(vibe_check_state_confirm(&state, 0) == VIBE_CHECK_STARTED);

    choose(&state, 1, 0);
    choose(&state, 1, 0);
    choose(&state, 1, 0);
    choose(&state, 1, 0);
    assert(state.page == VIBE_CHECK_QUESTION);
    assert(state.question == 4);
    assert(vibe_check_state_confirm(&state, 0x123) == VIBE_CHECK_FINISHED);
    assert(state.page == VIBE_CHECK_RESULT);
    assert(state.result == 7);
    assert(state.share_code <= 0x0fff);

    assert(vibe_check_state_confirm(&state, 0) == VIBE_CHECK_RESTARTED);
    assert(state.page == VIBE_CHECK_QUESTION);
    assert(state.question == 0);
    assert(state.answers == 0);

    choose(&state, 0, 0);
    choose(&state, 0, 0);
    choose(&state, 0, 0);
    choose(&state, 0, 0);
    choose(&state, 0, 0);
    assert(state.result == 0);

    vibe_check_state_init(&state);
    vibe_check_state_move(&state, 1);
    assert(state.choice == 0);

    uint8_t seen_results = 0;
    for (uint8_t answers = 0; answers < 32; answers++) {
        vibe_check_state_init(&state);
        assert(vibe_check_state_confirm(&state, 0) == VIBE_CHECK_STARTED);
        for (uint8_t question = 0; question < VIBE_CHECK_QUESTION_COUNT; question++) {
            choose(&state, (answers >> question) & 1U, 0x456);
        }
        assert(state.result == expected_result(answers));
        assert(state.share_code <= 0x0fff);
        seen_results |= (uint8_t)(1U << state.result);
    }
    assert(seen_results == 0xff);
    return 0;
}
