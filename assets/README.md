<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

### Minecraft guide Chinese subset

- `fonts/minecraft_zh_16.c` is a 16 px, 2 bpp LVGL subset generated from
  Noto Sans CJK SC Regular for the Chinese strings used by the guide. It falls
  back to LVGL Montserrat 14 for Latin letters and digits and is compiled by
  `main/CMakeLists.txt`.
- Source: [Noto Sans CJK](https://github.com/notofonts/noto-cjk), licensed under
  the SIL Open Font License 1.1. The license text is stored as
  `fonts/OFL-NotoSansCJK.txt`.
- Regenerate with `lv_font_conv`, using the visible Chinese strings from
  `main/minecraft_guide.c` as `--symbols`, `--size 16`, `--bpp 2`, and
  `--lv-fallback lv_font_montserrat_14`. Keep `--no-compress`: this firmware
  disables `LV_USE_FONT_COMPRESSED`, so compressed glyphs will not render.

## Images

Store reusable source images and generated display assets in `images/`.

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.

### Minecraft guide entry narration

- `music/minecraft_guide_entries/` contains twenty project-authored Chinese TTS
  clips, one per guide entry. They are 16 kHz, 16-bit, mono PCM, generated
  locally with the macOS Tingting system voice and contain no third-party
  recording.
- `tools/generate_minecraft_guide_audio.py` packs the ordered WAV files into
  `main/minecraft_guide_audio.c` as IMA ADPCM. The firmware decodes each clip in
  512-sample chunks and interrupts the current clip when the user switches to
  another entry.

## Submitted application assets

Application font sources in `fonts/` are LVGL subsets of Noto Sans CJK or Source Han Sans, covered by the included SIL OFL 1.1 license. The corresponding `tools/generate_*` scripts record generation commands; firmware compiles the generated C files directly.

`music/vibe_check/`, `music/idiom_pet/`, `music/word_sprite/`, `music/pvz_almanac/` and `music/laoluo_quotes/` contain offline narration assets. System voices are macOS Tingting (Mandarin) and Samantha (English); manifests record the text and asset hashes. WAV files support rebuilding and checks; ADPCM data is used by firmware. Lao Luo quote sources are listed in the application `quotes.csv`.
