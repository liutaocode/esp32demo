<p align="right">
  <a href="README.zh_CN.md">Simplified Chinese</a> · <strong>English</strong>
</p>

# Lao Luo Quotes



Lao Luo Quotes is an offline pocket quote radio for FoloToy AI Passport. It
opens on a randomly selected quote, keeps the original pixel-device identity,
and reads every entry aloud with a neutral synthesized Mandarin voice.

## Controls

- UP: previous quote.
- DOWN: next quote.
- OK: play or replay the current TTS clip.

The battery remains visible. The backlight dims after one idle minute and turns
off after three; the first button press wakes the screen without changing the
quote.

## Content and audio

The initial collection contains 12 short entries across action, ideals,
thinking, product craft, change, and personal choice. `quotes.csv` is the
editable source of truth and includes a source title and URL for every entry.
The first collection was checked against [Wikiquote](https://zh.wikiquote.org/wiki/%E7%BD%97%E6%B0%B8%E6%B5%A9),
a [2006 profile](https://news.sohu.com/20060404/n242621013.shtml), and a
[2016 interview transcript](https://www.ithome.com/0/214/423.htm).

The bundled WAV files were generated with the neutral macOS Tingting system
voice. They are not recordings or an imitation of Luo Yonghao. The build packs
them as IMA ADPCM and decodes 512 samples at a time in a worker task, keeping
audio work out of button callbacks.

## Add a quote

1. Add a row to `quotes.csv` with a unique ID, category, quote, source title,
   and source URL.
2. Regenerate the catalog, WAV clips, ADPCM pack, and Chinese font subset:

```bash
python3 tools/generate_laoluo_quotes_assets.py \
  --synthesize --voice Tingting \
  --font /path/to/NotoSansCJKsc-Regular.otf
```

The font uses Noto Sans CJK SC under the SIL Open Font License 1.1. The license
is stored in `assets/fonts/OFL-NotoSansCJK.txt`.

## Build

Use ESP-IDF 5.5.3:

```bash
FAP_APP=laoluo_quotes ./tools/validate.sh --firmware
```

This is an unofficial fan-made application and is not affiliated with or
endorsed by Luo Yonghao or his companies.

## Folder layout

- `quotes.csv`: sourced, editable quote collection.
- `assets/images/laoluo_quotes/`: shared publication and README artwork.
- `laoluo_quotes.c`: user interface and controls.
- `laoluo_quotes_state.*`: testable browsing state.
- `laoluo_quotes_catalog.*`: generated quote data.
- `laoluo_quotes_audio.*` and `laoluo_adpcm.*`: generated narration data and
  playback decoding.
