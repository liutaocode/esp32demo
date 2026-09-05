#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MEMORY_GARDEN_ROUND_COUNT 3U
#define MEMORY_GARDEN_MAX_SEQUENCE 5U
#define MEMORY_GARDEN_SYMBOL_COUNT 6U
#define MEMORY_GARDEN_TOTAL_ITEMS 12U

typedef enum {
    MEMORY_GARDEN_WELCOME = 0,
    MEMORY_GARDEN_ROUND_READY,
    MEMORY_GARDEN_MEMORIZE,
    MEMORY_GARDEN_RECALL,
    MEMORY_GARDEN_ITEM_FEEDBACK,
    MEMORY_GARDEN_ROUND_COMPLETE,
    MEMORY_GARDEN_RESULT,
} memory_garden_page_t;

typedef enum {
    MEMORY_GARDEN_NO_CHANGE = 0,
    MEMORY_GARDEN_STARTED,
    MEMORY_GARDEN_SHOWING,
    MEMORY_GARDEN_RECALL_READY,
    MEMORY_GARDEN_REMEMBERED,
    MEMORY_GARDEN_COACHED,
    MEMORY_GARDEN_NEXT_ITEM,
    MEMORY_GARDEN_ROUND_FINISHED,
    MEMORY_GARDEN_NEXT_ROUND,
    MEMORY_GARDEN_FINISHED,
    MEMORY_GARDEN_RESTARTED,
} memory_garden_event_t;

typedef struct {
    memory_garden_page_t page;
    uint32_t seed;
    uint8_t round;
    uint8_t sequence_length;
    uint8_t position;
    uint8_t selection;
    uint8_t sequence[MEMORY_GARDEN_MAX_SEQUENCE];
    uint8_t round_correct;
    uint8_t total_correct;
    uint8_t last_expected;
    uint8_t last_choice;
    bool last_correct;
    uint16_t share_code;
} memory_garden_state_t;

void memory_garden_state_init(memory_garden_state_t *state);
memory_garden_event_t memory_garden_state_confirm(memory_garden_state_t *state,
                                                   uint32_t seed);
memory_garden_event_t memory_garden_state_advance(memory_garden_state_t *state);
void memory_garden_state_move(memory_garden_state_t *state, int direction);
const char *memory_garden_result_title(const memory_garden_state_t *state);
