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

### Six Arts Museum Chinese subset

- `fonts/six_arts_zh_16.c` is a 16 px, 2 bpp LVGL subset generated from Noto
  Sans CJK SC Regular for the museum tour's visible Chinese strings. It falls
  back to LVGL Montserrat 14 for Latin letters and digits and is compiled by
  `main/CMakeLists.txt`.
- The source is [Noto Sans CJK](https://github.com/notofonts/noto-cjk), licensed
  under the SIL Open Font License 1.1. The shared license text is stored as
  `fonts/OFL-NotoSansCJK.txt`.
- Regenerate from the visible Chinese strings in `main/six_arts_museum.c` with
  `python3 tools/generate_six_arts_font.py /path/to/NotoSansCJKsc-Regular.otf`.
  The script pins `lv_font_conv` 1.5.3, 16 px, 2 bpp, no compression, and the
  `lv_font_montserrat_14` fallback.

## Images

Store reusable source images and generated display assets in `images/`.

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

### Six Arts Museum README artwork

- `images/six-arts-museum-cover.png` is the 1152 × 1536 community cover created
  for this project from its original exhibit concepts and runtime visual
  language. It contains no museum photography, logos, or third-party artwork.
- `images/six-arts-museum-runtime.png` is a 240 × 320 framebuffer capture of the
  opening gallery. It contains only the app interface and no device or account
  identifiers.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.

### Six Arts Museum offline guide narration

- `music/six_arts_tts/` stores 22 source WAV files in page order, named from
  `01_museum_story.wav` through `22_location_detail.wav`.
- All clips use project-authored narration synthesized by
  `tools/generate_six_arts_tts.py` with the macOS Tingting system voice. No
  museum or other third-party recording is included.
- WAV sources are 16 kHz, 16-bit mono PCM. The same script compresses and packs
  them as 4-bit IMA-ADPCM in `main/six_arts_tts_audio.c`.
- Firmware decodes small chunks from flash into a PCM buffer before calling
  `bsp_audio_write()`, so a whole clip is never loaded into internal RAM.
