#include "social_battery_state.h"

void social_battery_init(social_battery_state_t *state)
{
    *state = (social_battery_state_t){ .page = SOCIAL_BADGE, .preset = 1U };
}

uint8_t social_battery_minutes(const social_battery_state_t *state)
{
    static const uint8_t minutes[SOCIAL_BATTERY_PRESET_COUNT] = {5, 15, 30};
    return minutes[state->preset % SOCIAL_BATTERY_PRESET_COUNT];
}

uint32_t social_battery_seconds(const social_battery_state_t *state)
{
    return (uint32_t)((state->remaining_ms + 999U) / 1000U);
}

uint8_t social_battery_progress(const social_battery_state_t *state)
{
    if (state->total_ms == 0U) return 0;
    return (uint8_t)((state->total_ms - state->remaining_ms) * 100U /
                     state->total_ms);
}

bool social_battery_tick(social_battery_state_t *state, uint64_t now_ms)
{
    if (now_ms < state->last_ms) return false;
    uint64_t elapsed = now_ms - state->last_ms;
    state->last_ms = now_ms;
    if (state->page != SOCIAL_RECHARGING || state->paused || elapsed == 0U) {
        return false;
    }
    if (elapsed >= state->remaining_ms) {
        state->remaining_ms = 0;
        state->page = SOCIAL_READY;
    } else {
        state->remaining_ms -= elapsed;
    }
    return true;
}

bool social_battery_input(social_battery_state_t *state,
                          social_battery_input_t input, uint64_t now_ms)
{
    /* Settle time before pause/cancel. An expiry consumes this event so an
       OK arriving at the deadline cannot dismiss the completion screen. */
    social_battery_page_t before = state->page;
    bool changed = social_battery_tick(state, now_ms);
    if (before != state->page) return true;
    switch (state->page) {
    case SOCIAL_BADGE:
        if (state->locked) {
            if (input != SOCIAL_HOLD) return changed;
            state->locked = false;
        } else if (input == SOCIAL_UP || input == SOCIAL_DOWN) {
            state->mode = (state->mode + (input == SOCIAL_UP
                ? SOCIAL_BATTERY_MODE_COUNT - 1U : 1U)) % SOCIAL_BATTERY_MODE_COUNT;
        } else if (input == SOCIAL_OK) {
            state->locked = true;
        } else if (input == SOCIAL_HOLD) {
            state->page = SOCIAL_SETUP;
        } else return changed;
        return true;
    case SOCIAL_SETUP:
        if (input == SOCIAL_UP || input == SOCIAL_DOWN) {
            state->preset = (state->preset + (input == SOCIAL_UP
                ? SOCIAL_BATTERY_PRESET_COUNT - 1U : 1U)) % SOCIAL_BATTERY_PRESET_COUNT;
        } else if (input == SOCIAL_OK) {
            state->total_ms = (uint64_t)social_battery_minutes(state) * 60000U;
            state->remaining_ms = state->total_ms;
            state->last_ms = now_ms;
            state->paused = false;
            state->page = SOCIAL_RECHARGING;
        } else if (input == SOCIAL_HOLD) {
            state->page = SOCIAL_BADGE;
        } else return changed;
        return true;
    case SOCIAL_RECHARGING:
        if (input == SOCIAL_OK) {
            state->paused = !state->paused;
        } else if (input == SOCIAL_HOLD) {
            state->page = SOCIAL_BADGE;
            state->paused = false;
            state->remaining_ms = 0;
            state->total_ms = 0;
        } else return changed;
        return true;
    case SOCIAL_READY:
        if (input != SOCIAL_OK && input != SOCIAL_HOLD) return changed;
        /* Finishing a timer is not consent to become socially available. */
        state->page = SOCIAL_BADGE;
        return true;
    }
    return changed;
}
