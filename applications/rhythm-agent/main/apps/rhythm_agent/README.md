# Rhythm Agent

English | [简体中文](README.zh_CN.md)

An offline three-button rhythm-memory game. Listen to a short sequence while its
keys light up, then repeat both the keys and the pauses. The first press starts
your own timeline: there is no race to hit an externally scheduled first beat.

## Play

- Training: eight locks, three to six notes, 650 ms beats, unlimited retries.
- Challenge: eight locks, four to eight notes, 500 ms beats, three failed attempts.
- From the third lock, some gaps last two beats. No simultaneous presses or holds
  are required for musical input. Release the key between notes.
- Each key has a distinct generated tone. The vertical on-screen pads follow
  physical UP / DOWN / OK order. Firmware input uses immediate PRESS events;
  CLICK and DOUBLE events do not add notes.
- Long OK pauses. Its preceding press is rolled back, including any changed score
  or life. Continuing an interrupted attempt replays the same lock without a penalty.
- On feedback, click OK to advance or retry. At the result, choose the same task
  for a friend or a new task. Long OK returns home outside play.
- Settings provide four volume levels, optional Mandarin prompts and two help pages.
  Settings reset at reboot. Only the challenge high score persists in its own NVS namespace.
- Menus dim after two minutes. The first waking press is consumed. Active play stays lit.

## Scoring

Training advances when all keys are correct, regardless of timing, and waits
without an input deadline. Its rhythm score is feedback for practice. Challenge
requires correct keys and a mean rhythm score of at least 75.
Each gap is scored independently against the demonstrated interval, so timing
drift does not accumulate from the first press. Correct notes score 100 within
110/80 ms, 75 within 220/160 ms, otherwise 25; wrong or missed notes score zero.
The thresholds refer to training/challenge. The first note has no timing penalty.
Callback timestamps exclude rendering delays. In challenge only, a note more
than 1100 ms late relative to the previous accepted key ends the attempt.
A passed lock adds its accuracy to the total, up to 800; failed attempts add none.
A four-digit task code seeds the same patterns for same-mode replays on this version.

## Implementation and checks

`rhythm_state.c` is portable logic. `rhythm_agent.c` owns the LVGL page and input
queue. Audio and battery/NVS each run in a lifetime worker that never owns a UI
pointer. Only the LVGL timer mutates widgets. Queue overflow pauses rather than
silently grading lost input. Exit disables input, removes the timer and cancels audio.
Audio uses fixed-point tones and four short embedded Mandarin ADPCM cues; it streams
160 PCM samples per write, including exact silent gaps. No microphone, network,
account, API key, or runtime speech service is required. Missing audio falls back
to a timed light sequence. Storage errors preserve the best score in RAM and never
erase NVS. A USB capture mirrors a frozen full frame with `FAP_SCREENSHOT_V1`.

Run `python3 tools/test_rhythm.py` and `python3 tools/test_rhythm_ui.py`, then the
complete `./tools/validate.sh` with ESP-IDF 5.5.3 active. Host rendering uses the
production page and checks Chinese glyphs, row widths, parent clipping, key gesture
sequences and lifecycle. It is not hardware acceptance.

Preserve the 3 MB application limit, identity region and permanent Recovery.
Device checks must still confirm speaker timbre, acoustic/visual latency, physical
button cadence, NVS survival, low battery behavior and a fresh serial screenshot.
