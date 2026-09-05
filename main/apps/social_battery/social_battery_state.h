#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SOCIAL_BATTERY_MODE_COUNT 4U
#define SOCIAL_BATTERY_PRESET_COUNT 3U

typedef enum {
    SOCIAL_BADGE = 0,
    SOCIAL_SETUP,
    SOCIAL_RECHARGING,
    SOCIAL_READY,
} social_battery_page_t;

typedef enum { SOCIAL_UP, SOCIAL_DOWN, SOCIAL_OK, SOCIAL_HOLD } social_battery_input_t;

typedef struct {
    social_battery_page_t page;
    uint8_t mode;
    uint8_t preset;
    bool locked;
    bool paused;
    uint64_t last_ms;
    uint64_t remaining_ms;
    uint64_t total_ms;
} social_battery_state_t;

void social_battery_init(social_battery_state_t *state);
/* Call with a monotonic timestamp; delayed callbacks never stretch a session. */
bool social_battery_tick(social_battery_state_t *state, uint64_t now_ms);
bool social_battery_input(social_battery_state_t *state,
                          social_battery_input_t input, uint64_t now_ms);
uint8_t social_battery_minutes(const social_battery_state_t *state);
uint32_t social_battery_seconds(const social_battery_state_t *state);
uint8_t social_battery_progress(const social_battery_state_t *state);
