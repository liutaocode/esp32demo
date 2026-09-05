<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Cloud Hop

A one-button timing game for AI Passport. Guide a tiny cloud across 25 floating
islands: watch the red landing marker, press confirm to jump, and aim for the
gold stripe to build a perfect streak. Islands shrink and the marker speeds up.
The interface is entirely Simplified Chinese.

## Device capture and submission



Captured from the running device over USB on September 5, 2026, showing six
islands crossed and 90 points. This is an unmodified framebuffer capture, not
a photograph. The verified firmware was flashed without writing NVS, device
identity or permanent Recovery. Community submission 155, revision 245,
`community-0f3de1f0`, is pending review as of that date. Submission details and
the server-verified firmware hash are in publication.json.



These are actual application screens rendered with desktop LVGL and simulated
peripherals, not photographs or physical-device validation.

## Play

| Screen | Up | Down | Confirm |
| --- | --- | --- | --- |
| Home | Switch mode | Switch mode | Start a new course |
| Playing | Pause | No action | Jump |
| Paused | Resume | Home | No action |
| Results | Next course | Home | Replay the same course |

The relaxed mode gives three chances; sprint gives one. A miss consumes one
chance and retries the same island. A normal landing earns 10 points. Gold
landings earn 20 plus 5 per consecutive perfect landing, capped at 50 per jump.
A normal landing or miss breaks the streak. Clear all 25 islands or use the last
chance to finish. The maximum possible score is 1,175.

The results show score, islands crossed, perfect landings, longest streak and a
four-digit course number. Pass the device to a friend and choose same-course
replay: island positions, widths, cursor speed and initial phase repeat in the
same mode. The number is an identifier, not an online leaderboard or an entry
code. Best scores for both modes survive application re-entry but reset at power
off. No network, accounts, audio or Flash writes are required.

The short loop, visible near misses and same-course rematch are product design
hypotheses for replay and sharing, not claims of proven popularity. Speech is
omitted because timing play benefits from immediate visual feedback.

## Build and checks

After activating ESP-IDF 5.5.3, run from the repository root:

```bash
FAP_APP=cloud_hop ./tools/validate.sh
```

This produces the verified `build/FoloToy-AI-Passport-full.bin`. Preserve a copy
before building another application. Use the mini-program for installation on
a provisioned device; the protected identity and permanent Recovery partitions
and the five-second UP boot hook remain unchanged.

The state machine has host tests for boundaries, scoring, pause, timing overflow,
course reproducibility and flight coordinates. The static gate also checks the
font against UI text. To run the actual LVGL UI checks and generate PPM captures:

```bash
cmake -S tests/cloud_hop_ui -B build/cloud-hop-ui -DCMAKE_BUILD_TYPE=Debug
cmake --build build/cloud-hop-ui -j8
(cd build/cloud-hop-ui && ./preview)
```

The UI harness checks glyph resolution, single-line text widths, label bounds,
input queuing, replay, idle wake-up, unavailable peripherals and 50 exit/re-entry
cycles. It uses the existing Word Sprite test stubs and the resolved LVGL source.
Regenerate the 16 px, 2 bpp Source Han Sans subset with
`python3 tools/generate_cloud_hop_font.py`. Source font license: SIL OFL 1.1.
All cloud and island art is original code-drawn pixel art; no bitmap or audio
assets are embedded in firmware.

## Runtime boundaries

Button callbacks only enqueue presses; LVGL timers own the game and UI. A jump
is judged against the last displayed marker. Extra presses during flight cannot
trigger another jump. Pause preserves flight position and feedback timers.
After 60 seconds idle, play pauses and the display dims; at 180 seconds it turns
off. The first press while dimmed or off only wakes the display. Battery reads
run between rounds or while paused, so its displayed value is cached during play;
unavailable readings show `--%`. All UI timers stop before screen deletion.

Build and host previews cannot verify LCD color, physical key latency, power
consumption, battery accuracy or the mini-program's complete installation flow.
Those checks still require the board.
