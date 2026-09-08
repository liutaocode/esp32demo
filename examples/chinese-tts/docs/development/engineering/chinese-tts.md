<p align="right"><a href="chinese-tts.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Chinese TTS port and acceptance

Pocket Reader links the upstream ESP-TTS Chinese parser and voice-template object,
plus the Speex 1.2.1 fixed-point decoder. Speech recognition, wake words, the
original AMR player and its floating-point tempo path are not used. The embedded
voice is SHA-256 checked before its pronunciation tables reach the binary parser.

## Runtime boundaries

UTF-8 requests of at most 240 bytes enter a single-entry overwrite queue. A
generation counter invalidates older playback and status updates. One worker
owns parser and decoder; it never accesses LVGL. Key callbacks only enqueue events.
Normal speed (2) streams 320-sample, 20 ms frames at 16 kHz. Other speeds decode
one syllable into RAM and apply integer WSOLA before writing frame-sized chunks.
The voice bank is prepared offline at 1.30x the original syllable rate; speed 2
needs no runtime stretching. Speeds 0–5 map to 0.80x, 0.90x, 1.00x, 1.10x, 1.20x
and 1.30x relative to the prepared voice. Cancellation is checked between frames and writes. There is no sentence PCM cache,
16-second duration limit, or Flash write. An ebook integration must split text,
manage reading position and submit subsequent sentences. It is not included here.

The worker stack is 8 KiB. Variable-speed buffers are allocated lazily from the
validated longest record and retained until shutdown. Stop keeps the engine
ready. Shutdown waits for worker acknowledgement; timeout preserves resources.
Serial logs distinguish parsing, synthesis CPU time, first PCM submission, maximum
frame decode time, minimum heap and stack headroom. First submission is not an
acoustic latency measurement; DMA buffering adds delay. Synthesis time excludes
blocked speaker writes.

## Compact resource

The original Xiaole bank is 2,938,039 bytes. Conversion preserves pronunciation
metadata and all 1,749 audio records. This count is not Chinese-character coverage;
the parser still decides pronunciations, numeric normalization and polyphonic words.
The default Speex wideband quality-2 bank is 929,972 bytes, including 171,640
bytes of headers, offsets and pronunciation tables. Output is 16 kHz signed
16-bit mono. This is lossy speech compression. An earlier 654 KB narrowband
version was too muffled in user listening; this version retains the 16 kHz
sample rate and more voice detail. Subjective clarity still requires listening.
Only the checked-in wideband bank is accepted by the current runtime hash.

There is no separate voice partition. The compact bank is part of the application,
which must remain below 3 MiB. The fixed identity at 0x356000 and Recovery at
0x700000 remain unchanged. A future app can use 0x360000–0x700000 (3.625 MiB) for
book storage; this demo does not create or format a filesystem. Previous voice
bytes may remain there after USB installation but are no longer read.

## Reproduction

Vendor provenance is pinned in `components/esp_tts/upstream.json` and
`components/speex/upstream.json`. Preserve their license files when redistributing.
The original bank stays in source control only as a conversion input.
With Speex 1.2.1 and OpenCORE AMR-WB development libraries installed:

```bash
cc -O2 -Imain tools/tts/repack_voice.c main/tts_tempo.c $(pkg-config --cflags --libs speex opencore-amrwb) -lm -o /tmp/repack-voice
/tmp/repack-voice assets/music/chinese_tts/xiaole.dat /tmp/xiaole-compact.dat 2
cmp /tmp/xiaole-compact.dat assets/music/chinese_tts/xiaole-compact.dat
```

The converter decodes AMR-WB at 16 kHz, applies integer WSOLA at 1.30x,
encodes each record independently in Speex wideband, and stores encoder
lookahead for trimming. No downsampling is used.
Encoder/library differences can change bytes. Do not replace the bank without
regenerating expected hashes and checking all records and hardware playback.

Run `./tools/validate.sh` in ESP-IDF 5.5.3. The gate checks source conventions,
asset hashes, font coverage, voice bounds, tempo duration/pitch, partition MD5,
application size, protected regions and the exact embedded voice payload.
Regenerate the UI font with `python3 tools/generate_chinese_tts_font.py` after
changing strings; `--check` verifies coverage. The generator uses pinned
`lv_font_conv@1.5.3` and Source Han Sans SC from the pinned LVGL component.

## Device acceptance before release

1. Verify cold boot and the five-second UP Recovery gesture.
2. Play all twelve examples at speeds 0, 2 and 5. Listen for intelligibility,
   polyphonic words, numbers, punctuation and underruns; record failures.
3. Stop mid-sentence, change examples and speed while playing, then replay.
4. Repeat for ten minutes and inspect heap, stack and responsiveness.
5. Use host fixtures to reject malformed voice data; never erase device identity.
6. Capture a fresh runtime screen through the publisher tool before submission.

Report Build, Host tests, Device tests and Unverified separately. Automated serial
success does not establish pronunciation or subjective listening quality.
