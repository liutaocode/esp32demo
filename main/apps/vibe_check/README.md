<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Vibe Check



Vibe Check is a fast, offline personality-card game for FoloToy AI Passport.
Five binary choices reveal one of eight original pixel personas, three readable
traits, and a short card code made for side-by-side comparison or sharing.
The complete device interface is in Simplified Chinese and uses a compact,
application-specific generated font subset. Offline Mandarin narration reads
the welcome, every question with both button choices, and the final persona.

## Play

1. Press OK on the welcome screen.
2. Listen to each prompt and press UP or DOWN once for each of the five choices.
3. Watch the short reveal and compare the result with friends.
4. Press OK on the result card to play again.

Hold OK on the welcome, question, or result screen to replay its narration.

The battery remains visible. The backlight dims after one minute without input
and turns off after three minutes; the first button press wakes it without
changing the game. The app is fully offline and stores no answers or personal
data. If audio initialization fails, all content remains playable in text.

## Implementation

- `vibe_check_state.c` contains the hardware-independent quiz state machine.
- `vibe_check.c` owns the LVGL page, pixel personas, reveal animation, battery
  display, and idle backlight behavior.
- `vibe_check_voice.c` owns cancellable worker-task playback; button and LVGL
  callbacks only replace the pending narration request.
- `vibe_check_audio.c` validates the 14-clip offline Mandarin ADPCM pack, which
  is decoded in 512-sample chunks without loading whole clips into RAM.
- `assets/fonts/vibe_check_zh_16.c` contains only the Simplified Chinese glyphs
  needed by the interface and falls back to Montserrat for digits.
- Button callbacks do not perform storage, audio decoding, or network work.

Regenerate and verify the narration assets with:

```bash
python3 tools/generate_vibe_check_audio.py --synthesize
python3 tools/generate_vibe_check_audio.py --check
```
