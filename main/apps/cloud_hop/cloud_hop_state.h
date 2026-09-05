#pragma once

#include <stdbool.h>
#include <stdint.h>

enum { CH_GOAL = 25, CH_FIELD = 198, CH_FLIGHT_MS = 440, CH_FEEDBACK_MS = 520,
       CH_CURSOR_MIN = 12, CH_CURSOR_MAX = 185, CH_PERFECT = 4 };
typedef enum { CH_HOME, CH_AIM, CH_FLY, CH_LANDED, CH_PAUSED, CH_RESULT } ch_page_t;
typedef struct {
    ch_page_t page, resume;
    uint8_t mode, level, lives, streak, best_streak, perfects;
    uint16_t score, best[2], challenge;
    uint32_t phase_ms, cursor_phase, round_seed;
    int16_t cursor, current_x, target_x, target_width, landing_x;
    bool hit, perfect, new_best;
} ch_state_t;
typedef struct { int16_t x, y; } ch_point_t;

void ch_home(ch_state_t *s);
void ch_start(ch_state_t *s, uint16_t challenge);
void ch_tick(ch_state_t *s, uint32_t elapsed_ms);
bool ch_jump(ch_state_t *s);
void ch_pause(ch_state_t *s);
ch_point_t ch_pose(const ch_state_t *s);
int ch_miss_distance(const ch_state_t *s);
