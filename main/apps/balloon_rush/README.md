<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Balloon Rush

A pocket timing game about deciding when to stop. Inflate a cheerful balloon,
collect your points, or risk everything for a larger reward. Eight increasingly
narrow target zones make a short round easy to share with a friend. All on-device
text is Simplified Chinese. No network, account, microphone, or TTS is needed.



## Play

- Press confirm to start. Stop the moving needle inside the green zone within
  nine seconds. The yellow center gives a 50% precision bonus.
- After a success, confirm risks another inflation; DOWN collects your points.
  Missing the zone or running out of time loses all uncollected points.
- Round rewards are `100 × round²`. Clearing all eight rounds collects
  automatically. The maximum score is 30,600 with eight precision bonuses.
- UP pauses or resumes. DOWN on the pause screen abandons the round. On the
  result screen, confirm retries and DOWN returns home. Holding confirm returns
  home; the initial press still performs the normal press action.
- The record lasts for this boot and survives leaving/re-entering the app.
  Idle screens dim after one minute and turn off after three. The first press
  wakes only. Controls remain disabled if button initialization fails.

## Build and validate

Activate ESP-IDF 5.5.3, then run from the repository root:

```bash
FAP_APP=balloon_rush ./tools/validate.sh
cmake -S tests/balloon_rush_ui -B /tmp/balloon-rush-ui -G Ninja
cmake --build /tmp/balloon-rush-ui
mkdir -p /tmp/balloon-rush-previews
cd /tmp/balloon-rush-previews
/tmp/balloon-rush-ui/preview
```

The complete gate includes the independent state tests. The separate real-LVGL
preview checks glyph availability, Chinese-only labels, label widths, content
bounds, every round, input queues, wake behavior and 50 exit/re-entry cycles.
Snapshots simulate the display, clock, buttons and battery; they are not device
captures. Regenerate the font with `python3 tools/generate_balloon_rush_font.py`.

The application has no audio/storage worker. Button callbacks enqueue events;
the LVGL timer owns state and widgets, reuses moving objects, drops stale inputs,
and stops before screen deletion. Battery reads are skipped during timing play.
Timing decisions use the last displayed needle rather than a future frame.
The existing 3 MB application limit, identity partition, permanent Recovery,
and five-second UP boot hook remain unchanged.

Physical input latency, animation smoothness, screen readability, battery sleep
behavior, and mini-program installation still require device testing.
