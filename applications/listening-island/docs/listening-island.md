[简体中文](listening-island.zh_CN.md) | English

# English Listening Practice: design and operation

## Experience

A small listening companion for everyday moments. Eight islands contain 48 original sentences each: home, food, school, animals, sport, weather, trips, and bedtime. Difficulty is roughly elementary; this is an original practice collection, not a textbook alignment or a measured learning-outcome claim. Navigation and controls remain Chinese. Listening and answer feedback display both the English sentence and its Chinese translation; quiz questions hide the English until answered. English is also delivered through synthesized speech.

The retention idea is short optional interaction and visible collection progress: one topic can play hands-free, while a treasure round asks eight questions. Nothing is locked behind a daily streak, payment, microphone, network, or countdown to answer. Commercial popularity has not been validated.

## Controls

| Screen | Up / Down | OK | Hold Up | Hold OK |
| --- | --- | --- | --- | --- |
| Home | Select one of five entries | Enter | No action | Stay home |
| Topic / stamps | Previous / next island | Start / return | No action | Home |
| Listening | Previous / next sentence | Pause / resume from sentence start | Replay | Home and stop |
| Treasure | Select first / second Chinese meaning | Submit | Replay English | Home and stop |
| Feedback | Replay correct sentence | Next question | Replay | Home and stop |
| Settings | Select volume / timer | Cycle setting | No action | Home |
| End | No action | Home | No action | Home |

Listening plays each complete sentence twice with an 800 ms gap, followed by 2.5 seconds for speaking along. Finishing the selected collection or reaching the 5/10/15 minute wall-clock timer stops playback; the timer continues while paused. No background microphone recording or speech recognition is performed. There is no artificial score for pronunciation.

Treasure questions sample eight unique sentences within the selected topic, shuffle the two answer positions, and include one previously missed sentence when available. A correct answer removes that sentence from the mistake set. Wrong answers reveal the correct translation and add it to mistake replay. Replaying does not clear a mistake. Six, seven, or eight correct answers award one, two, or three stamps; only the best rating per island is retained.

## Implementation and storage

Pure state lives in `main/apps/listening/listening_state.c`. Production LVGL UI is separate from a single audio/storage worker. Button callbacks only enqueue bounded events. The LVGL timer owns all page changes; the main task reads battery levels outside LVGL. Audio cancellation uses monotonically changing request IDs and checks every 512 PCM samples. Old completions cannot advance new sentences. At most 1024 PCM bytes and 256 compressed bytes are used in the decoding loop.

Generated speech is 16 kHz mono IMA ADPCM. Replays and listening/quiz modes reuse the same stored clip. One bank uses up to 1,600,000 bytes of application space; the second uses the added data partition at `0x360000`, size `0x3A0000`. The application remains below 3 MB. Identity at `0x356000`, Recovery at `0x700000`, and the five-second UP boot hook remain unchanged. The runtime checks resource CRC before enabling sound. The build gate checks exact resource inclusion in the merged image. The release manifest reports measured sizes and durations; this layout favors complete natural sentences and ample content over decorative bitmap assets.

Progress uses the dedicated `listen_island` NVS namespace, a versioned blob, range validation, and explicit save-error UI. Version 1 volume levels are migrated to five new levels (20/40/60/80/100, default 60), preserving listening records and stamps. NVS initialization failures never erase another application's records. Abrupt power loss before the queued save commits may lose the latest change. Missing audio blocks quiz submission; missing battery reads show a placeholder; no audio request marks a sentence heard until playback completes. Persistent services and the single screen live until power-off; no page deletion leaves a worker holding UI pointers. The optional runtime shutdown requests cancellation and exposes a worker exit acknowledgement.

## Build and verification

```bash
source /path/to/esp-idf-v5.5.3/export.sh
./tools/validate.sh
```

The gate checks repository rules, all generated assets, the pure state model, all compressed clips, production LVGL glyphs and bounds for every translation, baseline host tests, the firmware build, the Recovery contract, and the merged resource. It never opens the serial device or flashes it.

Regenerate speech using a Python environment with `kokoro`, PyTorch, NumPy, SoundFile and FFmpeg: `python tools/generate_listening_audio.py --synthesize`. Existing WAVs are reused. Regenerate the Chinese fonts with `python tools/generate_listening_fonts.py`. Synthesis model and voice details are in the [asset record](../assets/music/listening/README.md).

## Required device checks after explicit installation approval

1. Install the exported segments or use the mini-program Recovery flow. Never raw-write this merged image from zero over provisioned identity data.
2. Check boot, Chinese glyphs, battery display, physical key click/hold behavior, title and footer boundaries.
3. Listen to material from both banks and all eight topics; check pronunciation, rate, sentence endings, hiss, clicks, and comfortable volume.
4. Rapidly skip, pause, replay, return home and change modes; verify old audio cannot advance the new question.
5. Verify wrong-answer replay, stamp awards, restart persistence, power interruption, and resource failure handling.
6. Run a full listening collection and each timer setting; check heap, prolonged responsiveness, speaker and enclosure behavior.
7. Confirm five-second UP Recovery entry and mini-program install compatibility on the device.
8. Capture fresh serial framebuffer images with matching receipts, then prepare the final portrait cover and submission preview.

Build success and host renders do not prove these hardware results.
