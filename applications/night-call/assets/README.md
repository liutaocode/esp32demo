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

## Night Call

`fonts/night_call_zh_{12,16,24}.c` are Source Han Sans SC subsets generated from actual Chinese UI/story strings. The original font is distributed with LVGL under its font license. `music/night_call/` contains original authored dialogue rendered with macOS Tingting, source WAV clips, a SHA-256 manifest and the embedded IMA ADPCM pack. No borrowed characters, music or film samples are used. Generation and verification scripts are in `tools/generate_night_call_*`.
