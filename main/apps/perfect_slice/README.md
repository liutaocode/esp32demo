<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Perfect Slice implementation

Player guide and screenshots.

The standalone application uses a pure C state machine, a production LVGL page, a static four-event input queue, and a dedicated 16 px Chinese font. It does not initialize audio or networking. Enter and exit require the LVGL lock; button callbacks only enqueue press events. LVGL timers own rendering and game state. Exit disables input and deletes timers before the page. An empty placeholder screen is reclaimed on re-entry.

The moving knife uses a reflected fixed-point path. Input is judged against the displayed position before the next movement update; events older than 200 ms are discarded. A frame delay above 200 ms pauses active play. Battery I2C sampling occurs outside active cutting. Slice decorations are clipped mathematically to each piece, so even one-pixel slices have valid dimensions. Font fallback supplies ASCII digits and punctuation.

## Build and checks

From the repository root with ESP-IDF 5.5.3 activated:

```bash
FAP_APP=perfect_slice ./tools/validate.sh
python3 tools/generate_perfect_slice_font.py
cmake -S tests/perfect_slice_ui -B build/perfect-slice-ui
cmake --build build/perfect-slice-ui -j 8
(cd build/perfect-slice-ui && ./preview)
```

The complete gate includes pure-state tests. The separate UI executable uses the resolved LVGL dependency and shared host stubs, produces PPM screenshots, and checks all 179 positions over ten targets and two modes, glyph availability, text widths, queue overflow, stale events, repeated presses, stalled frames, idle wake, battery/button degradation, and fifty exit/re-entry cycles.

The application selection adds no partition changes. Keep board testing distinct from host rendering and firmware compilation. On hardware verify a full round in each mode, visible press-to-cut latency, pause/resume during play and reveal, idle dim/wake, readability, and sustained operation.
