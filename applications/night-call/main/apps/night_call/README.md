[简体中文](README.zh_CN.md) | English

# Night Call implementation

The application is a deterministic branching narrative. `story.json` is the
editable story source. `generate_night_call_story.py` emits immutable C tables
and `voice_text.json`; the latter is the exact subtitle text spoken in the pack.
There are 22 nodes and seven endings, with two choices per node. Choosing an
ending clears only the current call. The true finale requires ending bits 2 and
4, representing both completed rescues; merely reading their clues is insufficient.

`night_call_state.c` has no SDK dependencies. Its 24-byte versioned save record
uses explicit byte order, reserved bytes and an FNV checksum. Decode validates
chapter/node consistency and collection bounds before replacing live state.

Button callbacks only enqueue POD events without waiting. The 40 ms LVGL timer
reduces them and owns every screen object. Single clicks are used deliberately
so long presses cannot also commit a choice. Long-UP replay and long-DOWN mute
are independent of selection. No double-press behavior is needed. The page timer
and battery timer are removed and queued events discarded before screen deletion.

Audio and storage workers have application lifetime and never own LVGL pointers.
The audio worker decodes 256 PCM samples per write from an embedded IMA ADPCM
pack. A generation counter cancels stale clips and a single-slot queue keeps the
latest request. Playback/format/task failures fall back to readable subtitles.
NVS writes use a separate single-slot queue and the `nightcall` namespace. No
storage erasure is used as error recovery. Firmware removal leaves other namespaces
unchanged. Power loss before the save notice disappears may lose the latest action.

The screen uses the shared pixel frame in a night palette. Chinese subsets at
12, 16 and 24 pixels include the actual UI and story characters. Host checks render
the production LVGL tree, detect placeholder glyphs, reject Latin UI text, and
measure label bounds and parent clipping across every node and ending. The
read-only screenshot hook streams full-width refresh strips before display byte
swapping, while holding the LVGL lock. It needs no persistent framebuffer and
rejects unexpected strip ordering. Host tests compare the entire USB payload with
the pixels delivered to the simulated panel, including partial-write recovery.
No screenshot has been taken from the device during this preparation.

## Reproduction

```sh
python3 tools/generate_night_call_story.py
python3 tools/generate_night_call_audio.py --synthesize
python3 tools/generate_night_call_font.py
./tools/validate.sh
```

Audio regeneration requires macOS Tingting, `say` and FFmpeg. Checked-in WAVs and
the packed file permit verification/building without the voice synthesizer.
Fonts use the LVGL-distributed Source Han Sans SC font and `lv_font_conv@1.5.3`.
`--check` modes verify existing artifacts without replacing them. The complete
gate needs the pinned managed LVGL dependency and an activated ESP-IDF 5.5.3.
See the review package for device acceptance and release boundaries.
