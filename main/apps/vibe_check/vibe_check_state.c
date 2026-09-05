#include "vibe_check_state.h"

static uint8_t answer(const vibe_check_state_t *state, uint8_t question)
{
    return (state->answers >> question) & 1U;
}

static uint8_t calculate_result(const vibe_check_state_t *state)
{
    /* Three legible personality axes produce eight distinct shareable results. */
    uint8_t energetic = (answer(state, 0) + answer(state, 3)) >= 1U;
    uint8_t maker = (answer(state, 1) + answer(state, 4)) >= 1U;
    uint8_t bold = answer(state, 2);
    return (uint8_t)(energetic | (maker << 1) | (bold << 2));
}

void vibe_check_state_init(vibe_check_state_t *state)
{
    *state = (vibe_check_state_t){
        .page = VIBE_CHECK_WELCOME,
    };
}

void vibe_check_state_move(vibe_check_state_t *state, int delta)
{
    if (state->page != VIBE_CHECK_QUESTION || delta == 0) return;
    state->choice = delta > 0 ? 1U : 0U;
}

vibe_check_event_t vibe_check_state_confirm(vibe_check_state_t *state,
                                            uint32_t entropy)
{
    if (state->page == VIBE_CHECK_WELCOME) {
        state->page = VIBE_CHECK_QUESTION;
        state->question = 0;
        state->choice = 0;
        state->answers = 0;
        return VIBE_CHECK_STARTED;
    }

    if (state->page == VIBE_CHECK_QUESTION) {
        uint8_t mask = (uint8_t)(1U << state->question);
        state->answers = (uint8_t)((state->answers & ~mask) |
                                   (state->choice ? mask : 0U));
        if (++state->question < VIBE_CHECK_QUESTION_COUNT) {
            state->choice = 0;
            return VIBE_CHECK_ANSWERED;
        }

        state->result = calculate_result(state);
        state->share_code = (uint16_t)(((state->answers * 73U) ^
                                        (entropy & 0x0fffU) ^ 0x5a7U) & 0x0fffU);
        state->page = VIBE_CHECK_RESULT;
        return VIBE_CHECK_FINISHED;
    }

    vibe_check_state_init(state);
    state->page = VIBE_CHECK_QUESTION;
    return VIBE_CHECK_RESTARTED;
}
