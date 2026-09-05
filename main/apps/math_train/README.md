<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Math Train

An offline, single-player mental-arithmetic game with a Chinese interface. A locomotive carries ten stars: every correct first attempt lights one. Ten unique equations form a short trip. There is no countdown, time bonus, life loss or automatic advance.

## Controls and modes

- Up/down select a difficulty, one of three answers or a result action. Confirm starts, submits or continues. Hold confirm to return home (abandons the active trip).
- Choose addition/subtraction within 10, within 20, within 100, or multiplication/exact division using factors 1 through 9. These are practice ranges, not grade or curriculum guarantees.
- Each fresh trip has five questions of each operation, shuffled, with no duplicate equations. Subtraction stays nonnegative, division has no remainder, and each question has three distinct choices with exactly one correct answer.
- Feedback always shows the full correct equation. Incorrect feedback also shows the selected value and waits for confirmation.
- At the station, choose another random trip, review mistakes, or change difficulty. Empty review is disabled and skipped by navigation.
- Review only visits missed equations, reshuffles their choices and removes a mistake when corrected. Remaining mistakes can be reviewed again. Review never changes the original score or best streak.
- Best scores are separate for each mode and last only until restart. Starting a new trip clears the previous trip and its mistakes. No persistent progress, network, microphone or TTS is included.
- After one idle minute the display dims; after three it turns off. The first key only wakes it, without answering or leaving the current page. Missing battery readings display a placeholder; unavailable buttons display a Chinese error.

## Implementation and validation

Pure state is in `math_train_state.c`; input callbacks only enqueue timestamped events. The LVGL timer owns UI mutations. Stale and rapid events are discarded across page transitions. Enter/exit require the BSP LVGL lock; exit disables input, deletes both timers and then deletes the page. No worker touches the UI.

The 18 px Chinese subset and 28 px arithmetic font are generated from Source Han Sans (SIL OFL). No emoji or symbol-font icons are required. Shared pixel sky, title plate, outlined paper and grass remain intact; the train uses original geometric artwork.

```bash
python3 tools/generate_math_train_fonts.py --check
cc -std=c11 -Wall -Wextra -Werror -Imain/apps/math_train tests/test_math_train_state.c main/apps/math_train/math_train_state.c -o /tmp/test_math_train_state
/tmp/test_math_train_state
cmake -S tests/math_train_ui -B /tmp/math-train-ui
cmake --build /tmp/math-train-ui -j8
# Run preview in a temporary output directory; it writes PPM snapshots.
# After activating ESP-IDF 5.5.3:
FAP_APP=math_train ./tools/validate.sh
```

The host tests cover 120,000 generated equations, balanced operators, unique choices, deterministic seeds, scoring, review and menu wrapping. The actual LVGL test checks 4,000 additional question/feedback pages, missing glyphs, Latin letters, text widths, bounds, sibling label overlap, idle wake, input bursts, unavailable peripherals and 50 exit/re-entry cycles.

See the showcase and review package. Build validation is separate from physical-device acceptance; installation, boot and USB capture passed, and the submission is pending moderation. Extended physical-device checks remain unverified.
