<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Six Arts Museum Pocket Tour

<p align="center">
  <img src="assets/images/six-arts-museum-cover.png" alt="Illustrated Six Arts Museum pocket-tour cover" width="480">
</p>

An offline, research-led virtual tour of Six Arts Museum for the FoloToy AI
Passport. Eleven compact pages introduce the museum, seven representative folk-art
objects, founder Mitch Dudek, curator Chen Jie, and practical visit information.

The interface uses original code-drawn pixel illustrations, needs no network
connection, and is designed for the device's 240 × 320 display. Both the story
and close-look view on every page have dedicated offline Chinese narration, for
22 voice clips in total.

## Preview

<p align="center">
  <img src="assets/images/six-arts-museum-runtime.png" alt="Six Arts Museum opening-gallery screen" width="240">
</p>

<p align="center"><em>The opening gallery starts an eleven-stop narrated route.</em></p>

## Controls

- UP: previous page.
- DOWN: next page.
- OK: switch between the story and close-look text for the current page.

The app automatically narrates the current content at startup, after navigation,
and after switching views. A new action interrupts the previous clip and starts
the current one without blocking button input. The footer shows when the guide
is speaking.

The tour covers a lion-shaped *queti* bracket, a multi-bay canopy bed, painted
door gods, Ming–Qing carved windows, a ceremonial sedan, the 1877
"Da Zun You Er" plaque, and the stone-statue grotto. The selection is
curatorial rather than a formal list
of the museum's most valuable objects: together, the pieces connect
architecture, domestic life, ritual, belief, and community memory.

## Research basis

The museum's official site reports more than 40,000 displayed pieces in over
60 galleries across four floors, with collections centered on architectural
salvage, wood and stone carving, temple and domestic folk arts, and historic
furnishings. It currently lists daily opening hours of 9:00–18:00.

Key sources:

- [Six Arts Museum — museum and gallery overview](https://www.6arts.org/museum)
- [Six Arts Museum — three historic plaques](https://www.6arts.org/blog/explanation-of-three-plaques-in-six-arts-museum)
- [Six Arts Museum — interview on the collection and representative bed](https://www.6arts.org/blog/2cb8514003e)
- [China News Service — Mitch Dudek's collection story](https://www.br-cn.com/static/content/news/ch_news/2024-10-30/1301148165479095610.html)
- [Xinhua Daily — curator Chen Jie and the museum's preservation approach](https://www.sohu.com/a/897870976_121455647)
- [Suzhou Culture, Radio, Television and Tourism Bureau — registered address](https://wglj.suzhou.gov.cn/szwhgdhlyj/bwg/list_tt.shtml)

Visitor details can change. Confirm hours, tickets, and transport with the
museum before traveling.

## Build and validation

This project targets ESP32-C3 with ESP-IDF 5.5.3 and preserves the AI Passport
8 MB flash, 3 MB application, `cardid`, permanent Recovery, and five-second UP
key bootloader contracts.

```bash
source /path/to/esp-idf-v5.5.3/export.sh
./tools/validate.sh
```

## Assets and licensing

- `assets/fonts/six_arts_zh_16.c` is a Chinese glyph subset generated from Noto
  Sans CJK SC Regular under the SIL Open Font License 1.1. The license is stored
  at `assets/fonts/OFL-NotoSansCJK.txt`.
- All exhibit illustrations are original RGB565 pixel drawings rendered in
  firmware; no museum photographs are copied or embedded.
- `assets/music/six_arts_tts/` contains 22 Chinese TTS source WAV files generated
  from project-authored scripts. `tools/generate_six_arts_tts.py` uses the macOS
  Tingting system voice to create 16 kHz, 16-bit mono audio, then packs it into
  IMA-ADPCM data in `main/six_arts_tts_audio.c`. The device decodes it in chunks
  on a worker task, with no third-party recording or cloud credential required.
- Base firmware: [folotoy/ai-passport](https://github.com/folotoy/ai-passport).
