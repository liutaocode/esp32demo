<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Ricochet Rush implementation

Product, controls and local review package.

`ricochet_rush_state.c` owns deterministic course generation, aiming, bounded fixed-step collision, ball growth, scoring and round transitions. `ricochet_rush.c` owns the Chinese LVGL page, static input queue, partial updates, inactivity handling and lifecycle. All non-LVGL callers must hold the BSP LVGL lock for enter/exit. Main selects this app only when `FAP_APP=ricochet_rush`; other app selections stay intact.

## Reproduce

With ESP-IDF 5.5.3 activated:

```sh
FAP_APP=ricochet_rush ./tools/validate.sh
```

This builds and verifies a merged image; it does not install firmware. Preserve the verified artifact in the application package before another app build replaces the generic output.

Host logic and production UI:

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror -Imain/apps/ricochet_rush \
  tests/test_ricochet_rush_state.c main/apps/ricochet_rush/ricochet_rush_state.c \
  -lm -o /tmp/test_ricochet_rush_state
/tmp/test_ricochet_rush_state
cmake -S tests/ricochet_rush_ui -B /tmp/ricochet-rush-ui
cmake --build /tmp/ricochet-rush-ui -j 8
mkdir -p /tmp/ricochet-rush-previews
(cd /tmp/ricochet-rush-previews && /tmp/ricochet-rush-ui/preview)
python3 tools/render_ricochet_rush_preview.py --input /tmp/ricochet-rush-previews
python3 tools/generate_ricochet_rush_font.py --check
```

The preview conversion needs Pillow. Font regeneration needs Node/npm and pinned `lv_font_conv@1.5.3`; the font source is the repository's Source Han Sans SC. The static gate checks all application CJK glyphs. UI tests validate rendered glyphs and text widths, extreme aiming positions, maximum counters, all pages, queue behavior, pause/wake, unavailable peripherals and fifty exit/entry cycles.

The gameplay test covers face/corner collisions, no duplicate damage on separation, pickups, side walls, first-return placement, forced recall, ball caps, deterministic replay for eighty courses, fixed-step timing, loss and victory. Physical button latency, frame rate, sustained memory use and player difficulty remain device checks.
