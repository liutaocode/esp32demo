#pragma once
#include <stdbool.h>
#include <stdint.h>
#define RA_MAX_NOTES 8
#define RA_DOORS 8
typedef enum { RA_TRAIN, RA_CHALLENGE } ra_mode_t;
typedef enum { RA_DEMO, RA_READY, RA_INPUT, RA_FEEDBACK, RA_FINISHED } ra_phase_t;
typedef enum { RA_EXACT, RA_GOOD, RA_EARLY, RA_LATE, RA_WRONG, RA_MISSED } ra_mark_t;
typedef struct { uint8_t count, keys[RA_MAX_NOTES]; uint16_t at[RA_MAX_NOTES]; } ra_pattern_t;
typedef struct {
    ra_mode_t mode; ra_phase_t phase; ra_pattern_t pattern;
    uint32_t seed; unsigned door, lives, score, index, accuracy, attempts;
    int64_t origin, last_press; int error_ms;
    unsigned points; bool passed, all_keys, completed;
    ra_mark_t marks[RA_MAX_NOTES]; ra_mark_t last_mark;
} ra_game_t;
void ra_pattern(ra_pattern_t *p, uint32_t seed, unsigned door, ra_mode_t mode);
void ra_start(ra_game_t *g, uint32_t seed, ra_mode_t mode);
void ra_ready(ra_game_t *g);
bool ra_press(ra_game_t *g, unsigned key, int64_t ms);
void ra_tick(ra_game_t *g, int64_t ms);
void ra_next(ra_game_t *g);
void ra_replay(ra_game_t *g);
