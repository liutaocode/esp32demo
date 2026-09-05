[简体中文](README.zh_CN.md) · **English**

# Focus Post implementation

An offline Chinese button game about noticing a target and withholding a response to other visitors. Public introduction and review preparation live in the project folder.

## Interaction

Up/Down choose a four-, three-, or two-second visitor window. OK starts two unscored practice steps: delivery has no deadline, then waiting uses the selected window. A wrong practice response repeats that step. A rule card precedes scored play. The target remains visible and fixed throughout the course. Each page shows one short main cue and a large animal. Twelve small marks replace the written visitor counter; practice explicitly demonstrates pressing and waiting, while scored play uses a conditional comparison cue. All formal courses contain six target and six non-target visitors, shuffled within balanced halves. Each visitor appears after an 850 ms release interval. Press OK for the target; otherwise wait for the bar to finish. Feedback waits for OK, so there is no automatic next trial. Twelve visitors end the course, with delivered and correctly waited counts shown separately out of six.

Down pauses; OK shows an 850 ms release cue, then resumes with the exact remaining response time. Down from pause returns home. Long OK returns home. A press is emitted before the long event, so holding OK may briefly answer the current visitor before returning home and discarding that course. On results, OK returns home to rest, Up repeats the same course, and Down starts a new course with a rule reminder. Home always offers practice again. There is no best-score ladder, persistent profile, network, microphone, TTS, or reward currency. Replaying preserves the seed and target. Restarting clears the session.

## Runtime and resource decisions

Application code and pure state live in this directory; no BSP behavior or partition changed. The UI uses the shared sky, title plate, grass and panels, with three distinct pixel animals reused from Just Seen, enlarged using integer-scaled rectangles. A small portrait and name identify the target; the visitor is a large picture. Chinese primary cues use a 24 px Source Han Sans subset, with a 16 px subset for secondary hints. Both include ASCII digits and punctuation. Battery sits below the upper-right cloud; unavailable readings show `--%`.

Button callbacks only enqueue timestamped input into a four-entry static queue. A 20 ms LVGL timer owns state and UI mutations. Events older than 250 ms and page-transition duplicates are discarded. Deadline decisions use event time; delayed rendering starts a full response window. There is no repeated-press event handling. Timers stop before screen deletion, and the queue remains statically allocated across lifecycle changes. Entry/exit run under the caller's LVGL lock. After 30 seconds without input, an active course pauses; at 60 seconds the backlight dims, and at 180 seconds it turns off. The first waking press is consumed. Nothing writes to NVS or accesses audio. No image framebuffer or audio buffer is added.

## Build and checks

```bash
python3 tools/generate_focus_post_font.py
cc -std=c11 -Wall -Wextra -Werror -Imain/apps/focus_post \
  tests/test_focus_post_state.c main/apps/focus_post/focus_post_state.c \
  -o /tmp/focus-post-state
/tmp/focus-post-state
cmake -S tests/focus_post_ui -B /tmp/focus-post-ui-build
cmake --build /tmp/focus-post-ui-build -j8
mkdir -p /tmp/focus-post-previews
(cd /tmp/focus-post-previews && /tmp/focus-post-ui-build/preview)
python3 tools/render_focus_post_preview.py
# Activate ESP-IDF 5.5.3 before this build-only gate:
FAP_APP=focus_post ./tools/validate.sh
```

The state tests exercise 3,000 balanced courses, all paces, replay, tutorial correction, exact deadlines, pause timing and always-press/always-wait strategies. The production LVGL host harness checks every page, label glyph coverage, absence of Latin UI letters, text overflow, sibling label overlap, panel bounds, stale input, duplicates, sleep/wake, unavailable buttons and 50 entry/exit cycles. Host previews simulate peripherals; physical display and input remain unverified until authorized installation. The shared screenshot protocol and permanent Recovery contract remain enabled.
