<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Needle Rush

A Chinese one-button timing game for AI Passport. Shoot a needle into the empty
space on a rotating wheel. Avoid existing needles, clear ten levels, and replay
the same course to improve your score. The interface uses Simplified Chinese;
the screenshots below come from the production LVGL page with simulated hardware.



## Play

| Screen | UP | DOWN | OK |
| --- | --- | --- | --- |
| Home | Change difficulty | Change difficulty | Start |
| Playing | Pause | No action | Fire |
| Paused | Resume | Home | No action |
| Result | Start next challenge number | Home | Replay same challenge |

Hold OK to return home. A shot starts on button press; holding that same press
returns home when the long-press event arrives. The orange dot below the wheel
marks the firing lane, and the center number is the number of needles left.
Dark needles are obstacles; green needles are successful shots. Colliding needles
flash red. Each level reverses direction and increases the speed.

- Casual mode allows three misses across the entire run; expert mode allows one.
- Each successful shot earns 10 points; every third consecutive success adds 5.
  A miss resets the streak. Ten levels contain 62 shots; a flawless run earns 720.
- The challenge number reproduces starting positions and level speeds. Difficulty
  and challenge number must both match when comparing attempts.
- Separate best scores for each difficulty survive page changes, but reset when
  the device restarts. No account, network, saved personal data, or TTS is used.
- At one minute idle, play pauses and the backlight dims. At three minutes it
  turns off. The first press after screen-off only wakes it; press UP to resume.

## Build and validation

Activate ESP-IDF 5.5.3, then run from the repository root:

```bash
FAP_APP=needle_rush ./tools/validate.sh
```

The verified merged image is `build/FoloToy-AI-Passport-full.bin`. This selects
only this application and retains the 3 MB application limit, protected identity,
permanent Recovery, and five-second UP bootloader hook. Install via the existing
mini-program Recovery flow. See the [compatibility guide](../../../docs/development/engineering/ble-recovery-compatibility.md).

The static gate includes the pure-C state test: angular wrap and exact collision
boundaries, pause during flight, input rejection, stall timing, score retention,
same-course replay, and 200 full games. A separate native LVGL test exercises the
production page, checks glyph coverage, rejects English words, checks text bounds
and 3,600 wheel layouts, and tests wake-up, input bursts, fallbacks, and 50 exits:

```bash
cmake -S tests/needle_rush_ui -B build/needle-rush-ui -G Ninja
cmake --build build/needle-rush-ui
(cd build/needle-rush-ui && ./preview)
python3 tools/generate_needle_rush_font.py
```

The preview test writes PPM captures in its working directory. It requires the
resolved LVGL component in `managed_components/`. The font generator uses the
bundled Source Han Sans SC source and pinned `lv_font_conv@1.5.3`, generating an
uncompressed 16 px Chinese subset with Montserrat fallback for numbers.

## Runtime and device acceptance

The pure state module owns collision and scoring. Button callbacks only enqueue
timestamped events. LVGL timers handle movement and rendering; enter/exit require
the BSP LVGL lock. Timers stop before screen deletion. No application worker
tasks, network clients, image decoders, or audio buffers are allocated.

The wheel freezes for the 120 ms flight and 420 ms feedback, so collision is
judged against the position visible at the accepted press. At most one queued
event is used per frame; events older than 200 ms are discarded. A stalled frame
advances the simulation by at most 60 ms. Idle timing uses actual elapsed time.
Battery reads occur only on home, pause, and result pages.

Compilation and native rendering do not validate the physical display or input
latency. On a device, check both directions at the highest speed, near-edge
collisions, repeated presses and long-press return, Chinese text, backlight wake,
and Recovery access. Frame rate, power draw, and actual key feel remain device
measurements.
