<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# One More Fruit implementation

The independent application is selected with `FAP_APP=fruit_merge`. Other applications and the default selection remain available.

## Design

Input: up/down on press; confirm on single or double click, coalesced to one action; confirm long-press pauses without a preceding drop. Button callbacks only enqueue bounded timestamped events. LVGL timers own all UI access. Stale input and transition guards prevent queued drops or accidental wake actions.

State: `fruit_merge_state.c` is independent of LVGL and ESP-IDF. Rows are stored from floor to ceiling. Each drop saves the complete board and random generator for one undo. Resolve the focused fruit's neighbors in below/left/right/above order, then scan floor-to-ceiling and left-to-right for contacts formed by gravity. One pair is resolved per 180 ms UI tick interval, followed by compaction. No diagonal matches. Top-rank pairs disappear. One move cannot exceed 20 pair resolutions; each reduces occupied cells. Points are `2^(old_rank+1) * combo_index`, bounded to 999999.

Replay: the first two fruits are cherries. Later draws are 50% cherries, 30% grapes and 20% plums; the sequence depends only on its seed and turn count. Undo restores the exact sequence. The fruit book and best score describe things actually achieved in this power session, so undo does not remove them.

Output: Chinese 12/16/24 px Source Han Sans subsets, code-drawn fruit badges, a persistent current/next preview, highlighted column and landing cell. Rank names supplement color for recognition. The pixel sky, grass and title plate are retained. Battery status is below the top-right cloud; unavailable battery reads show `--%`.

Resources and lifecycle: no audio, networking, storage writes, bitmap frame buffers or worker tasks. One four-element static input queue and two LVGL timers. Exit disables input and deletes both timers before screen deletion. Font data lives in flash. Record and collection data live in RAM only. Dimming and backlight-off do not suspend the MCU.

## Validation

```bash
cc -std=c11 -Wall -Wextra -Werror -Imain/apps/fruit_merge \
  tests/test_fruit_merge_state.c main/apps/fruit_merge/fruit_merge_state.c \
  -o /tmp/test_fruit_merge_state
/tmp/test_fruit_merge_state
python3 tools/generate_fruit_merge_font.py --check
cmake -S tests/fruit_merge_ui -B /tmp/fruit-merge-ui-build
cmake --build /tmp/fruit-merge-ui-build -j8
mkdir -p /tmp/fruit-merge-previews
(cd /tmp/fruit-merge-previews && /tmp/fruit-merge-ui-build/preview)
python3 tools/render_fruit_merge_preview.py
FAP_APP=fruit_merge ./tools/validate.sh
```

Activate ESP-IDF 5.5.3 before the full gate. The preview conversion needs Pillow. The UI harness executes the production screen code with real LVGL, checks every rendered glyph, English-letter exclusion, label overflow, sibling label overlap, panel bounds, queue behavior, wake behavior and repeated exit/entry. Stress snapshots use synthetic states and are not gameplay evidence. See review preparation for the remaining physical checks.
