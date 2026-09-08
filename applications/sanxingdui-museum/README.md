<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Sanxingdui Museum Pocket Tour

<p align="center">
  <img src="assets/images/sanxingdui-museum-cover.png" alt="Sanxingdui Museum pocket-tour cover" width="480">
</p>

An offline, eleven-stop introduction to Sanxingdui Museum and the ancient Shu
world. Monumental figures, sacred trees, masks, gold, jade, and newly excavated
composite objects form a route from archaeological discovery to open questions.

Every stop includes original pixel art plus a Chinese story and close-look
narration. All 22 voice clips work offline.

## Preview

<p align="center">
  <img src="assets/images/sanxingdui-museum-runtime.png" alt="Sanxingdui Museum opening screen" width="240">
</p>

## How to explore

- UP: previous stop.
- DOWN: next stop.
- OK: switch between story and close-look view.

Narration begins on startup, navigation, and view changes. A new action interrupts
the previous clip immediately, keeping the tour responsive.

## Tour selection

The route features the Large Standing Bronze Figure, No. 1 Sacred Tree,
protruding-eye mask, gold staff, gold mask, kneeling figure bearing a *zun*,
turtle-back lattice vessel, ritual-scene jade *zhang*, director Lei Yu, and visit
information. Interpretations that remain debated are presented as questions,
not settled facts.

## Research basis

The new museum building opened in 2023 with three narrative sections and more
than 1,500 sets of objects. Current visit arrangements can change, especially
during holidays and summer; use official booking channels and check the latest
notice before traveling.

Key sources:

- [Sanxingdui Museum — official site](https://www.sxd.cn/index.asp)
- [Ministry of Culture and Tourism — new museum and exhibition overview](https://www.mct.gov.cn/wlbphone/wlbydd/xxfb/qglb/sc/202308/t20230801_946332.html)
- [Central Commission for Discipline Inspection — representative collection details](https://m.ccdi.gov.cn/content/d0/4d/3539.html)
- [Guanghan Municipal Government — hours and official booking channels](https://www.guanghan.gov.cn/gk/zjah/ahll/1647848.htm)
- [Hebei Department of Culture and Tourism / Xinhua — director Lei Yu](https://whly.hebei.gov.cn/c/2025-06-24/581327.html)
- [Chinese Government Procurement Network — current museum address](https://www.ccgp.gov.cn/cggg/dfgg/zbgg/202505/t20250530_24688328.htm)

## Build

The ready-to-flash merged image is at
`build/FoloToy-AI-Passport-sanxingdui-full.bin`.

```bash
source /path/to/esp-idf-v5.5.3/export.sh
./tools/validate.sh
```

## Assets and licensing

- `assets/fonts/sanxingdui_zh_16.c` is a Noto Sans CJK SC glyph subset under the
  SIL Open Font License 1.1.
- Exhibit images are original code-drawn pixel art; no museum photography,
  official logo, or third-party artwork is embedded.
- `assets/music/sanxingdui_tts/` contains 22 project-authored Chinese narration
  WAV files, packed as IMA-ADPCM for chunked offline playback.
- Base firmware: [folotoy/ai-passport](https://github.com/folotoy/ai-passport).
