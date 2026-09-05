#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Logical playfield pixels, independent of the physical display driver. */
#define STACK_RUSH_FIELD 192
#define STACK_RUSH_WIDTH 108
#define STACK_RUSH_HISTORY 8
#define STACK_RUSH_GOAL 50
#define STACK_RUSH_SETTLE_MS 320

typedef enum {
    STACK_READY, STACK_MOVING, STACK_SETTLING, STACK_PAUSED, STACK_RESULT
} stack_rush_page_t;

typedef struct { int16_t x, width; } stack_rush_block_t;

typedef struct {
    stack_rush_page_t page;
    stack_rush_page_t resume_page;
    uint8_t mode; /* 0: relaxed; 1: fast. Separate session records. */
    uint8_t floors, streak, perfects;
    uint8_t best[2];
    uint8_t count;
    bool perfect, restored, won, new_best;
    uint32_t phase; /* Triangular motion phase, in thousandths of a pixel. */
    uint32_t settle_ms;
    stack_rush_block_t tower[STACK_RUSH_HISTORY];
    stack_rush_block_t moving, debris;
} stack_rush_state_t;

void stack_rush_init(stack_rush_state_t *state);
void stack_rush_select(stack_rush_state_t *state);
void stack_rush_start(stack_rush_state_t *state);
void stack_rush_tick(stack_rush_state_t *state, uint32_t elapsed_ms);
bool stack_rush_drop(stack_rush_state_t *state);
void stack_rush_pause(stack_rush_state_t *state);
void stack_rush_home(stack_rush_state_t *state);
uint16_t stack_rush_speed(const stack_rush_state_t *state);
