#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { RR_COLS = 6, RR_ROWS = 7, RR_CELLS = 42, RR_BALLS = 24,
       RR_WIDTH = 198, RR_HEIGHT = 172, RR_ROUNDS = 30, RR_STEP_MS = 5 };
typedef enum { RR_HOME, RR_AIM, RR_FLIGHT, RR_SETTLE, RR_PAUSED, RR_RESULT } rr_page_t;
typedef struct { float x, y, vx, vy; bool active; } rr_ball_t;
typedef struct {
    rr_page_t page, resume;
    unsigned mode, round, balls, launched, returned, gained, score, hits, best_hits;
    unsigned best[2], cleared, flight_ms, settle_ms, remainder, aim_phase;
    uint32_t seed, rng;
    uint16_t hp[RR_CELLS];
    uint8_t flash[RR_CELLS];
    bool pickup[RR_CELLS], fast, won, new_best, first_return, recalled;
    float launch_x, next_x, angle;
    int direction;
    rr_ball_t ball[RR_BALLS];
} rr_state_t;
void rr_home(rr_state_t *s);
void rr_start(rr_state_t *s, uint32_t seed);
void rr_tick(rr_state_t *s, unsigned ms);
bool rr_fire(rr_state_t *s);
void rr_pause(rr_state_t *s);
void rr_reverse(rr_state_t *s);
/* Shared by production rendering and host collision tests. */
void rr_rect(unsigned cell, float *x, float *y, float *w, float *h);
unsigned rr_preview(const rr_state_t *s, float *xy, unsigned capacity);
