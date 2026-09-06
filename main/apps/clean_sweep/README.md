<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Clean Sweep implementation

[Player guide and screenshots](../../../projects/clean-sweep/README.md).

The standalone application uses a pure C state machine, a production LVGL page, a static six-event input queue, and dedicated 12/16/24 px Chinese fonts. It does not initialize audio or networking. Enter and exit require the LVGL lock; button callbacks only enqueue press and long-press events, and LVGL timers own state advance and drawing. Exit disables input and deletes timers before the page.

The ten by twenty well is one LVGL object that draws rectangles from `LV_EVENT_DRAW_MAIN` instead of one object per cell: two hundred cell objects would spend tens of KB of internal RAM on object headers and local styles alone, and this board has no PSRAM. The same callback draws the grid, settled blocks, landing ghost, active piece, and the clear animation, so the flash, burst, and collapse phases need no extra objects. The well is invalidated only when the board changes, and during the clear animation on a 40 ms step, so a still board costs no refresh.

The three OK actions map onto the driver's click, double-click and long-press events. The state machine emits exactly one of them per physical press: a long press never emits a trailing single click, and a double click never emits a single click first (`iot_button.c` raises the single click only when `repeat == 1` in `PRESS_REPEAT_DOWN_CHECK`). Rotation, drop and pause are therefore mutually exclusive by construction, with no compensation for an accidental rotation. Direction keys still act on the press edge so lateral movement has no added latency; a rotation waits out the 180 ms short-press window that rules out a double click.

Do not shorten the long-press threshold with `iot_button_set_param()`. Registering the callback stores the then-current long-press duration in `event_args.long_press.press_time`, and `PRESS_UP_CHECK` only invokes the callback when that stored value still equals `long_press_ticks`; changing the threshold silently disables the long-press callback. This application did exactly that for one revision — host tests cannot see it, and on hardware the long press simply does nothing.

## Build and checks

From the repository root with ESP-IDF 5.5.3 activated:

```bash
FAP_APP=clean_sweep ./tools/validate.sh
python3 tools/generate_clean_sweep_fonts.py
python3 tools/test_clean_sweep_ui.py
```

The complete gate includes pure-state tests for the shape table, seven-bag randomizer, gravity, lock delay, the three clear-animation phases, hold-to-drop undo, scoring, combos, perfect clears, sprint completion, and top-out. The UI executable uses the resolved LVGL dependency and shared host stubs, plays a full game with a placement heuristic, checks glyph availability, text widths, panel clipping, and screen bounds on every frame, covers all rotations and columns of the seven pieces, four-line clears, perfect clears, pause, both result pages, idle pause and dim, button degradation, and fifty exit/re-entry cycles, and writes PPM screenshots.

The application selection adds no partition changes. Keep board testing distinct from host rendering and firmware compilation. On hardware verify press-to-move latency, how the click-to-rotate and double-click drop feel, animation smoothness and screen colors, idle dim and wake, and sustained operation.
