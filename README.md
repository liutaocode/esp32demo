<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Pocket Reader — offline Chinese TTS demo

Turn AI Passport into a pocket reader. Choose one of twelve Chinese examples,
press OK to hear it, and compare six reading speeds. Examples cover common
characters, everyday sentences, numbers, polyphonic words, tones, punctuation,
and a short classical poem. Speech is synthesized locally from text; no network,
account, or prerecorded sentence files are required.

## Controls

- UP / DOWN: choose the previous / next example and stop current speech.
- OK: start or stop reading.
- Hold OK: stop reading and cycle speed from 0 (slowest) to 5 (fastest).
- Hold UP for five seconds at power-on: enter the permanent Recovery installer.

The screen shows the selected text, speech status, speed, battery at startup,
and measured audio duration / synthesis time. Synthesis time excludes blocked
speaker writes; it is not end-to-end latency. Pronunciation and pacing, especially
polyphonic words and numeric normalization, must be checked by listening.
The demo does not guarantee coverage of a particular common-character list.

## Build and installation

Use ESP-IDF **5.5.3**, ESP32-C3, **8 MB Flash**, no PSRAM.

```bash
source /path/to/esp-idf-v5.5.3/export.sh
./tools/validate.sh
```

Install the verified `build/FoloToy-AI-Passport-full.bin` through the mini-program.
The application stays below 3 MiB, with a 929,972-byte compact voice embedded in
it. No separate voice partition is required. The gate verifies the embedded data
and the protected identity and Recovery layout. For USB development, use
`idf.py -p PORT flash` to write separate bootloader, table and application segments.
Never erase the device or overwrite its identity and permanent Recovery.

## Reuse

`main/tts_runtime.h` exposes a background speech worker. Start it after BSP audio
initialization, then call `tts_runtime_say(text, speed)` with valid UTF-8 text up to
240 bytes. New requests replace older speech. One controlling task owns the API;
the worker owns the TTS instance and codec writes. See the
[port and acceptance notes](docs/development/engineering/chinese-tts.md).

The app boots directly into the demo. Its explicit exit function disables event
consumption, joins the worker, and then deletes the screen. It retains worker
resources if a timed shutdown fails instead of deleting a blocked audio task.

## Third-party distribution

The application glue is source-available under this repository's license.
The ESP-TTS engine and voice template are upstream **precompiled RISC-V libraries**,
not fully editable synthesis-engine source. Their exact revision and SHA-256
hashes are in `components/esp_tts/upstream.json`.

The upstream ESPRESSIF MIT license permits distribution for use on Espressif
products and requires retention of its copyright and permission notice. The
upstream `esp_tts.h` header carries Apache-2.0 terms. Keep both bundled license
texts with redistributions; describing a project as noncommercial does not replace
these conditions. Font licensing and asset provenance are in
[assets](assets/README.md).

## Validation status

Host/build validation is separate from hardware acceptance. Board playback,
pronunciation, memory under sustained use, Recovery installation, and a fresh
serial screenshot remain required before treating the demo as hardware-verified.
No community or GitHub release is claimed by this source tree.

## Streaming and storage budget

The original codec/tempo path took 6.625 s to synthesize 5.157 s of audio on this
C3. Whole-sentence buffering was a temporary workaround for those underruns.
The compact version uses a 16 kHz fixed-point Speex decoder. Normal speed writes
20 ms frames as they are decoded. Other speeds prepare only one syllable for
integer pitch-preserving time stretching. Default pacing is adjusted offline to
1.30x before compression. Speeds 0–5 vary from 0.80x to 1.30x relative to that
prepared voice. There is no whole-sentence cache or
16-second audio limit. Text requests remain limited to 240 UTF-8 bytes; an ebook
caller should submit sentences and coordinate pause, resume and page changes.

All original pronunciation metadata and 1,749 audio records are retained. The
voice bank shrinks from 2,938,039 to 929,972 bytes (about 68% smaller). Compression
is lossy, but retains the 16 kHz sample rate after the earlier 8 kHz variant
proved too muffled in user listening. These records are not
1,749 distinct Chinese characters. Coverage still depends on the upstream parser.
The original bank remains in the repository only as conversion input.

The complete app is approximately 1.68 MB, including the voice and UI. The
`0x360000`–`0x700000` region provides 3.625 MiB that a future app can partition for
books; this demo does not mount a book filesystem. Unused app partition capacity
is separate from that storage budget.

For USB testing, newline-terminated `TTS_DEMO_PLAY`, `TTS_DEMO_NEXT`, and
`TTS_DEMO_SPEED` follow the physical-key control path. PLAY toggles start/stop,
NEXT selects the next example, and SPEED cancels and changes speed.

The decoder is Speex 1.2.1 under its BSD license; retain
`components/speex/COPYING`. Conversion details are in the port notes.
