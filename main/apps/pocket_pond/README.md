[简体中文](README.zh_CN.md) | English

# Pocket Pond

An offline, turn-based fishing and collecting game with a Chinese interface.
See the illustrated project and review materials.

## Play

Each trip shuffles nine distinct fish and three waves without replacement. Click
OK to draw and DOWN to bank the basket. One wave is harmless; the second ends
the trip and loses only the unbanked fish. Catching all nine fish automatically
banks the maximum 29 points. Remaining fish, waves and the exact next-draw odds
are visible. There is no clock, reaction test, energy system, purchase or network.

UP opens the album; UP/DOWN browse and OK returns. Holding OK returns home
without abandoning the current trip; OK resumes it. A double-click is one action,
not two. Rapid input is coalesced to prevent accidental extra draws.

The album records each species up to 999 catches. One, five and twenty banked
catches award bronze, silver and gold status respectively. The best trip and up
to 9999 completed trips are stored locally. Wait for the saved message before
powering off. Unbanked trips survive idle/home navigation but **not power loss**.
Storage failures show a Chinese warning; they do not erase existing NVS data.

After 30 seconds idle the backlight dims; after 90 seconds it turns off. The
first click/long press wakes only. Play only while safely parked or as a passenger;
the game is not intended for a driver at a red light. No TTS or microphone is used.

## Implementation and verification

The pure state model owns the finite deck, bank/loss rules, collectible counts
and versioned 24-byte save encoding. Button callbacks enqueue at zero timeout;
the LVGL timer consumes input. An application-lifetime worker owns NVS and never
accesses UI objects. Exit disables dispatch and removes timers before the screen.
No new BSP logic, dependencies, pins, partitions or Recovery changes are needed.
The existing `FAP_SCREENSHOT_V1` service is retained for later device capture.

```sh
# Activate ESP-IDF 5.5.3 first.
FAP_APP=pocket_pond ./tools/validate.sh
cmake -S tests/pocket_pond_ui -B build/pocket-pond-ui
cmake --build build/pocket-pond-ui -j8
(cd projects/pocket-pond/assets && ../../../build/pocket-pond-ui/preview)
```

The host suite checks 20000 shuffled decks, scoring, bank/loss, odds, save
validation and caps. Production LVGL tests check glyph availability, Chinese-only
labels, width, screen/panel bounds, non-overlapping text rows, album variants,
input bursts, home/resume, idle/wake, unavailable battery/buttons/storage and exit.
A production build is not device acceptance. Physical keys, display colors,
long-idle behavior, power-cycle persistence and USB capture remain on-device checks.
