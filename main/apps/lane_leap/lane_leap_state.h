#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { LL_LANES = 3, LL_ROWS = 4, LL_WIDTH = 198, LL_HEIGHT = 182,
       LL_PLAYER_Y = 146, LL_STEP_MS = 20, LL_JUMP_MS = 760 };
typedef enum { LL_HOME, LL_READY, LL_RACING, LL_PAUSED, LL_RESULT } ll_page_t;
typedef enum { LL_EMPTY, LL_COIN, LL_AIR_COIN, LL_BARRIER, LL_TRUCK, LL_GAP } ll_kind_t;
typedef struct {
    bool active, passed, hurt, jumped;
    float y;
    ll_kind_t kind[LL_LANES];
    bool taken[LL_LANES];
} ll_row_t;
typedef struct {
    ll_page_t page;
    unsigned mode, lane, hp, score, coins, streak, best_streak, jumps, cleared;
    unsigned best[2], jump_ms, cooldown_ms, invincible_ms, countdown_ms;
    unsigned elapsed_ms, spawn_ms, accumulator_ms, result_ms, notice_ms;
    unsigned notice; /* 1 coin, 2 air coin, 3 leap, 4 impact */
    uint32_t seed, random;
    bool new_best;
    float distance;
    ll_row_t row[LL_ROWS];
} ll_state_t;

void ll_home(ll_state_t *s);
void ll_start(ll_state_t *s, uint32_t seed);
void ll_move(ll_state_t *s, int direction);
bool ll_jump(ll_state_t *s);
void ll_pause(ll_state_t *s);
void ll_tick(ll_state_t *s, unsigned ms);
float ll_height(const ll_state_t *s);
unsigned ll_level(const ll_state_t *s);
unsigned ll_multiplier(const ll_state_t *s);
