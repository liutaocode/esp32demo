#include "social_battery_state.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    social_battery_state_t s;
    social_battery_init(&s);
    social_battery_input(&s, SOCIAL_UP, 1000);
    assert(s.mode == 3);
    social_battery_input(&s, SOCIAL_DOWN, 1000);
    assert(s.mode == 0);
    social_battery_input(&s, SOCIAL_DOWN, 1000);
    social_battery_input(&s, SOCIAL_OK, 1000);
    assert(s.locked);
    assert(!social_battery_input(&s, SOCIAL_UP, 1000));
    assert(!social_battery_input(&s, SOCIAL_OK, 1000));
    assert(s.mode == 1 && s.page == SOCIAL_BADGE);
    social_battery_input(&s, SOCIAL_HOLD, 1000);
    assert(!s.locked && s.page == SOCIAL_BADGE);
    social_battery_input(&s, SOCIAL_HOLD, 1000);
    assert(s.page == SOCIAL_SETUP);
    social_battery_input(&s, SOCIAL_HOLD, 1000);
    assert(s.page == SOCIAL_BADGE);
    social_battery_input(&s, SOCIAL_HOLD, 1000);
    assert(social_battery_minutes(&s) == 15);
    social_battery_input(&s, SOCIAL_UP, 1000);
    assert(social_battery_minutes(&s) == 5);
    social_battery_input(&s, SOCIAL_UP, 1000);
    assert(social_battery_minutes(&s) == 30);
    social_battery_input(&s, SOCIAL_DOWN, 1000);
    social_battery_input(&s, SOCIAL_OK, 1000);
    assert(s.page == SOCIAL_RECHARGING && social_battery_seconds(&s) == 300);
    social_battery_tick(&s, 1250);
    assert(social_battery_seconds(&s) == 300);
    social_battery_tick(&s, 1000); /* Ignore a regressing clock. */
    assert(s.remaining_ms == 299750);
    social_battery_input(&s, SOCIAL_OK, 2250); /* Settle before pause. */
    assert(s.paused && s.remaining_ms == 298750);
    social_battery_tick(&s, 1000000);
    assert(s.remaining_ms == 298750);
    social_battery_input(&s, SOCIAL_OK, 1000250);
    assert(!s.paused);
    social_battery_tick(&s, 1001000);
    assert(s.remaining_ms == 298000);
    /* OK at the deadline must not silently dismiss completion. */
    social_battery_input(&s, SOCIAL_OK, 1299000);
    assert(s.page == SOCIAL_READY && s.remaining_ms == 0);
    assert(social_battery_progress(&s) == 100);
    social_battery_tick(&s, UINT64_MAX);
    assert(s.page == SOCIAL_READY && s.mode == 1);
    social_battery_input(&s, SOCIAL_OK, UINT64_MAX);
    assert(s.page == SOCIAL_BADGE && s.mode == 1);

    for (uint8_t preset = 0; preset < SOCIAL_BATTERY_PRESET_COUNT; ++preset) {
        social_battery_init(&s);
        s.preset = preset;
        social_battery_input(&s, SOCIAL_HOLD, 0);
        social_battery_input(&s, SOCIAL_OK, 0);
        uint64_t total = (uint64_t)social_battery_minutes(&s) * 60000;
        social_battery_tick(&s, total / 2);
        assert(social_battery_progress(&s) == 50);
        social_battery_tick(&s, total - 1);
        assert(s.page == SOCIAL_RECHARGING && social_battery_seconds(&s) == 1);
        social_battery_tick(&s, UINT64_MAX); /* Delayed callback, no underflow. */
        assert(s.page == SOCIAL_READY && social_battery_seconds(&s) == 0);
    }
    social_battery_init(&s);
    social_battery_input(&s, SOCIAL_HOLD, 0);
    social_battery_input(&s, SOCIAL_OK, 0);
    social_battery_input(&s, SOCIAL_HOLD, 150);
    assert(s.page == SOCIAL_BADGE && s.remaining_ms == 0 && !s.paused);
    assert(!social_battery_tick(&s, 900000));
    assert(social_battery_progress(&s) == 0);
    puts("Social Battery state tests: PASS");
    return 0;
}
