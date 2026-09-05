English | [简体中文](README.zh_CN.md)

# Grassland Lab — Plants vs. Zombies Almanac



A pocket fan almanac for FoloToy AI Passport with a Simplified Chinese interface,
24 original pixel illustrations, and 48 offline Mandarin TTS clips. It covers
16 plants and 8 zombies from the classic first game; it is a starter selection,
not a complete encyclopedia or a playable tower-defense game.

Browse a character's ability and practical tip, listen to its explanation, or
pass the device around for a five-question clue challenge. A result card shows
a playful title and the round's number. Replaying that round uses the same
questions and choices so friends can compare scores fairly. Leaving the result
and starting another challenge generates a new round.

The interface uses a cream paper and forest-green field-guide palette. Three Chinese font sizes separate headings, reading text and metadata; portrait stages, subtle selection rows and a fixed key bar keep the 240 × 320 screen readable. Speech status remains next to the portrait or clue.

## Controls

| Page | UP / DOWN click | OK click | Long press |
| --- | --- | --- | --- |
| Home | Select plants, zombies, or challenge | Enter | None |
| Almanac | Previous / next card, wrapping within category | Read name, ability and tip | OK: home; UP: replay |
| Challenge | Select one of three answers | Submit, then continue after feedback | UP: replay clue; OK: home |
| Result | None | Replay the same round | OK: home |

Each clue also appears as text, so the challenge works without sound. Revealing
an answer automatically reads its explanation. Flipping cards or returning home
cancels speech; repeated requests replace pending speech. Recognized characters
and scores last only until app restart. There is no network, account, storage,
or cloud TTS dependency at runtime. After 60 idle seconds the display dims; the
first input wakes it without changing the page. Battery failure displays `--%`.

## Build and assets

Activate ESP-IDF 5.5.3, then run from the repository root:

```bash
FAP_APP=pvz_almanac ./tools/validate.sh
```

The shared gate writes `build/FoloToy-AI-Passport-full.bin`. Keep a named copy for
this variant before building another app. The default variant stays unchanged.
The existing 3 MB app limit, identity and Recovery addresses, and five-second
UP bootloader hook are preserved. See the
[BLE installation contract](../../../docs/development/engineering/ble-recovery-compatibility.md).

`catalog.csv` is the editable source. The asset generator produces a C catalog,
audio index, an embedded ADPCM pack, a speech manifest and the Chinese font:

```bash
python3 tools/generate_pvz_assets.py --synthesize --font
python3 tools/generate_pvz_assets.py --check
```

Regeneration uses macOS `say` with the neutral Tingting voice, FFmpeg,
`lv_font_conv@1.5.3` and the bundled Source Han Sans font. Normal firmware builds
use the generated assets without those tools. Every card has one complete
explanation clip and one separate clue clip that does not reveal the name.

## Architecture and checks

- `pvz_state.c`: deterministic navigation, category boundaries, unique question
  decks, same-category distractors, scoring and session collection bits.
- `pvz_view.c` / `pvz_art.c`: actual LVGL UI and original code-drawn illustrations.
- `pvz_almanac.c`: static non-blocking input queue, LVGL timer, battery and dimming.
- `pvz_audio_runtime.c`: interruptible audio worker with 512-byte PCM chunks,
  generation-tagged status and explicit shutdown acknowledgement. It never
  accesses LVGL. `enter` and `exit` require the LVGL lock outside button callbacks.
- Host tests cover 12,000 challenge runs, navigation, score boundaries, repeated
  rounds, catalog integrity and audio bounds. Asset checks compare the manifest,
  encoded bytes and index with the saved WAV sources.

Device acceptance still requires checking Mandarin pronunciation, speaker volume,
rapid navigation during playback, long presses, idle wake, and install/recovery
on an actual board. A firmware build or host rendering is not a device test.

Host LVGL rendering and mocked input/lifecycle checks (requires CMake and a C compiler):

```bash
cmake -S tests/pvz_ui -B /tmp/pvz-ui-build
cmake --build /tmp/pvz-ui-build -j 4
mkdir -p /tmp/pvz-preview
/tmp/pvz-ui-build/pvz_ui_test /tmp/pvz-preview
/tmp/pvz-ui-build/pvz_input_test
```

## Content and provenance

Character names and game identity belong to their respective owners. This is an
unofficial fan project. Descriptions and clues are independently written Chinese
summaries; illustrations are drawn with LVGL shapes. No original game sprites,
music, character recordings or copied almanac flavor text are included.
Gameplay facts were checked against the classic
[almanac reference](https://gamefaqs.gamespot.com/pc/959255-plants-vs-zombies/faqs/61070)
and [day-mode guide](https://strategywiki.org/wiki/Plants_vs._Zombies/Day).
Assets and their provenance are registered in the [asset index](../../../assets/README.md).
