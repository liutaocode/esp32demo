#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { D100_ROWS = 7, D100_GAP = 44, D100_FIELD_W = 198,
       D100_FIELD_H = 166, D100_STEP_MS = 10, D100_GOAL = 100 };
typedef enum { D100_HOME, D100_PLAY, D100_PAUSE, D100_RESULT, D100_HELP } d100_page_t;
typedef enum { D100_HOLE, D100_SOLID, D100_GEM, D100_CRACK, D100_SPIKE, D100_HEAL } d100_tile_t;
typedef enum { D100_READY, D100_LAND, D100_COMBO, D100_TREASURE, D100_BREAKING,
               D100_HURT, D100_SUPPLY, D100_CEILING, D100_CLEAR } d100_notice_t;
typedef struct {
    int32_t y;
    uint16_t depth, crack_ms[3];
    uint8_t tile[3], used;
} d100_row_t;
typedef struct {
    d100_page_t page;
    d100_notice_t notice;
    d100_row_t rows[D100_ROWS];
    uint32_t seed, active_ms, last_land_ms;
    uint16_t challenge, floor, score, best[2], notice_ms, immune_ms, remainder_ms;
    uint16_t next_depth, combo, max_combo, gems, supplies;
    int32_t feet, vy;
    int8_t lane, support;
    uint8_t mode, health, previous_hole;
    bool new_best;
} d100_state_t;
void d100_start(d100_state_t *s, uint16_t challenge);
void d100_tick(d100_state_t *s, uint32_t elapsed_ms);
bool d100_move(d100_state_t *s, int direction);
void d100_pause(d100_state_t *s);
int d100_speed(const d100_state_t *s);
unsigned d100_stage(unsigned depth);
unsigned d100_crack_delay(unsigned depth);
int d100_lane_x(int lane);
