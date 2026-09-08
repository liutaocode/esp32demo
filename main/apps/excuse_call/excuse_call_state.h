#pragma once
#include <stdbool.h>
#include <stdint.h>
#define EC_DURATION_MS 60000U
#define EC_RINGS 5U
#define EC_CALLERS 3U
typedef struct {
    bool ringing, timed_out;
    unsigned ring, caller, calls;
    uint64_t deadline;
} ec_state_t;
void ec_init(ec_state_t *s);
void ec_confirm(ec_state_t *s, uint64_t now);
void ec_tick(ec_state_t *s, uint64_t now);
void ec_select(ec_state_t *s, int direction);
unsigned ec_seconds(const ec_state_t *s, uint64_t now);
const char *ec_ring_name(unsigned ring);
const char *ec_caller_name(unsigned caller);
