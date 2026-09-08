<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Tally Click 1.2

Turn it on and count immediately. The full-screen counter keeps the latest ten results without dates or clock setup. A restart directly resumes the saved current count.

| Screen | UP | DOWN | OK | Hold OK 1.5 s |
| --- | --- | --- | --- | --- |
| Counting | Subtract one | Add one | Pause | Save result, then reset |
| Paused | History | History | Resume | Save result, then reset |
| History | Newer result | Older result | Return to pause | Return to pause |

Directions count a physical press once; holds do not repeat. Range: 0–9999. Every successful reset saves its result, including zero. Only the newest ten results remain. Failed saves preserve the original count. Version-1.1 snapshots migrate automatically, preserving numbers and removing dates.

Current-count autosave normally runs about 800 ms after the last change. Rapid input prioritizes audio; automatic saves wait for a quiet gap. Wait for the saved indicator beside the battery before powering off. Sudden power loss can discard uncommitted increments. Archive is committed before the zero is displayed.

## Sound and display

Tall original digital numerals, mint/cyan bounce effects, amber pause and archive sweep/particles remain. Distinct original PCM sounds are generated offline, eliminating per-sample floating-point synthesis during playback. A priority-6 audio worker continuously supplies 15 ms blocks, including silence. New sounds replace queued feedback with a 5 ms crossfade. Automatic saves wait until playback and its DMA tail are quiet. Audio failure leaves counting usable. Actual sound quality requires listening on the device.

## Validation

Activate ESP-IDF 5.5.3 and run `./tools/validate.sh`. It covers state and old-record migration, eight counter-worker cases, three audio-worker cases, waveform/crossfade checks, real-LVGL layout and firmware compatibility. Output: `build/FoloToy-AI-Passport-full.bin`.

Generate assets with `tools/generate_tally_fonts.py` and `tools/generate_tally_audio.py`. `tools/render_tally_review.py` uses Pillow for host previews. See [review materials](../../README.md), [assets](assets/README.md) and [development references](docs/README.md).
