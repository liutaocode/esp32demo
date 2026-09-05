<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Down 100

An original three-lane descent game with a Chinese interface. Platforms rise continuously: step into a gap, fall to the next landing and escape the ceiling spikes. Reach floor 100 to win. Seven short original synthesized cues mark starting, gems, supplies, cracking boards, damage, defeat and victory. No network, microphone or speech is used.

Active play uses a full-width 240 by 294 pixel field below a 26 pixel HUD for depth, hearts, score and battery. The title, frame, footer controls and running commentary are hidden; controls and rules are shown on pause. Miner and platform art scale with the field while physics runs in a fixed logical coordinate space.

In-game hazards and items are silver spikes, visibly cracked planks, cyan gems and heart supply packs, without letter or word markers. Collected gems disappear, claimed supplies leave their platform, and plank fissures widen during the latter half of their collapse timer. Art is drawn directly by LVGL without text glyphs or bitmap assets.

## Controls

| Screen | UP | DOWN | OK |
| --- | --- | --- | --- |
| Home | Switch mode | Instructions | New random course |
| Instructions | Home | Home | Start |
| Playing | One lane left | One lane right | Pause |
| Paused | Restart same course | Home | Resume |
| Result | Next course | Home | Retry same course |

Only the immediate PRESS event is consumed; click, double and long events do not duplicate actions. One movement per press, no held or simultaneous keys. The three lanes have centers at 35, 99 and 163 local pixels. Logical movement is instantaneous so the shown lane always matches collision detection.

## Rules and replay

- Adventure starts with three hearts; Extreme starts with one and faster scrolling. Ceiling contact ends either mode immediately.
- Edge exits leave connected pads. Center landings never contain spikes, but may become brittle from floor 41 onward. Every landing retains a one-press escape. The first four floors are clear. All 10,000 maps in both modes allow every gem to be collected without damage with presses 200 ms apart.
- Rewards remain sparse: 21–31 gems per course, each worth 30 points once. Pink spikes cost one heart on first contact, with one second of invulnerability after damage. Each plank starts its own collapse timer on contact, even if the player leaves.
- Five stages guarantee increasing obstacle counts instead of independent random chances. Seeded rotation and direction keep spikes evenly distributed: between spike rows, the five stages have at most 4, 3, 2, 1 or 1 clear rows (excluding supply and stage boundaries). Supply floors are obstacle-free. Stage counts and timings are:

| Floors | Spike rows | Spike + center-plank rows | Plank time | Adventure speed |
| --- | ---: | ---: | ---: | ---: |
| 1–20 | 3 | 0 | 1000 ms | 16–24 |
| 21–40 | 6 | 0 | 900 ms | 24–34 |
| 41–60 | 9 | 3 | 750 ms | 34–46 |
| 61–80 | 12 | 6 | 650 ms | 46–58 |
| 81–100 | 14 | 10 | 550 ms | 58–70 |

Speeds are logical pixels per second, interpolated continuously between stage boundaries; Extreme adds four. Each stage also has 0, 3, 3, 4 or 4 standalone brittle rows, respectively. The HUD color becomes warmer with depth; pause shows the stage number, with no extra instructions during play.

- Every twentieth floor has one green supply pack. Each pack independently restores one heart in Adventure, capped at three. A pickup is consumed once and never clears items on other pads. Extreme does not gain hearts. Floor 100 is a complete final platform.
- A newly reached floor grants 10 points per depth advanced, plus twice the consecutive landing count capped at five. Reach the next floor within two seconds for a combo; damage breaks it. Clearing floor 100 grants 300 plus 50 per remaining heart.
- Course layouts depend only on the four-digit course number, not press timing. Retry or hand the device to a friend to play the same layout. Each mode has a separate best score, kept only until reboot; no save, online ranking or cross-device course-entry feature is promised.

## Implementation and resource budget

`down_100_state.c` is pure C with a fixed 10 ms simulation step, seven recycled platform rows, swept vertical collision checks and a following camera. Speed, obstacle quotas and collapse timing follow the five-stage table, capped at floor 100. Every pad has its own collection bit and collapse timer. The UI maintains a bounded set of platform/art objects; it does not recreate the screen per frame. Original miner/platform art uses geometric LVGL objects and a 16 px Source Han Sans subset.

`down_100_key()` only sends a timestamped event to a static four-entry queue. The LVGL timer drains it, drops events older than 200 ms and accepts at most one action per displayed frame. Slow frames and USB captures do not pause play. The state engine clamps catch-up to 100 ms instead of simulating unseen seconds. HUD values update only when changed, and tile art redraws when its appearance or position changes.

Battery reads occur on non-playing pages only. After 60 seconds without a press the screen dims and play pauses; after three minutes it goes dark. The first waking press is consumed. Exit rejects input and deletes both timers before deleting the screen. All page lifecycle calls run under the caller's LVGL lock. One app-lifetime sound worker has a 4 KB stack, a one-slot latest-cue queue and a 320-byte PCM chunk. It performs all codec setup and blocking output outside LVGL, never accesses UI/game state, cancels on pause/exit and acknowledges idle after muting. Its permanent blocked wait avoids allocating another task on re-entry. Initialization/output failure falls back to silent gameplay. No radios, NVS writes or large audio assets are added. The existing read-only USB screenshot service remains available.

## Build and verification

```sh
python3 tools/generate_down_100_font.py --check
cc -std=c11 -O2 -Wall -Wextra -Werror -Imain/apps/down_100 \
  tests/test_down_100_state.c main/apps/down_100/down_100_state.c -o /tmp/test_down_100
/tmp/test_down_100
cmake -S tests/down_100_ui -B /tmp/down-100-ui-build
cmake --build /tmp/down-100-ui-build -j8
(cd /tmp/down-100-ui-build && ./preview)
python3 tools/render_down_100_preview.py /tmp/down-100-ui-build
# With ESP-IDF 5.5.3 activated; builds only, never installs:
FAP_APP=down_100 ./tools/validate.sh
```

UI tests compile the production UI and LVGL with simulated peripherals, run a complete course, verify all rendered glyphs and bounds, and exercise timing, queue overflow, pauses, idle wake, unavailable controls/battery and 60 lifecycle cycles. Host previews are not device screenshots. Showcase and release handoff.
