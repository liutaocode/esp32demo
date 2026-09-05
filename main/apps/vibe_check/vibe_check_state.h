#pragma once

#include <stdbool.h>
#include <stdint.h>

#define VIBE_CHECK_QUESTION_COUNT 5
#define VIBE_CHECK_RESULT_COUNT 8

typedef enum {
    VIBE_CHECK_WELCOME = 0,
    VIBE_CHECK_QUESTION,
    VIBE_CHECK_RESULT,
} vibe_check_page_t;

typedef struct {
    vibe_check_page_t page;
    uint8_t question;
    uint8_t choice;
    uint8_t answers;
    uint8_t result;
    uint16_t share_code;
} vibe_check_state_t;

typedef enum {
    VIBE_CHECK_NO_CHANGE = 0,
    VIBE_CHECK_STARTED,
    VIBE_CHECK_ANSWERED,
    VIBE_CHECK_FINISHED,
    VIBE_CHECK_RESTARTED,
} vibe_check_event_t;

void vibe_check_state_init(vibe_check_state_t *state);
void vibe_check_state_move(vibe_check_state_t *state, int delta);
vibe_check_event_t vibe_check_state_confirm(vibe_check_state_t *state,
                                            uint32_t entropy);
