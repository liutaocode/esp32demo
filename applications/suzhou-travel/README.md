<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Suzhou Travel Guide

This standalone ESP-IDF application turns the FoloToy AI Passport into a small
offline guide to ten classic Suzhou sights. Its 240 × 320 interface combines an
original ink-wash garden banner with rice-paper colors, celadon green, cinnabar
accents, and lattice-window details.

## Included sights

Humble Administrator's Garden, Lingering Garden, Tiger Hill, Hanshan Temple,
Pingjiang Road, Shantang Street, Lion Grove Garden, Master of the Nets Garden,
Canglang Pavilion, and Jinji Lake.

## Controls

- `UP`: previous sight
- `DOWN`: next sight
- `OK`: switch between the short introduction and practical highlights

The current page is narrated on startup and after every UP, DOWN, or OK action.
Each introduction and highlight page has its own offline TTS clip; a new action
interrupts the previous narration instead of blocking the controls.

The battery indicator stays in the upper-right corner and degrades to `--` when
the optional gauge is unavailable. The application is fully offline and stores
no location or personal data.

## Build

Activate ESP-IDF 5.5.3, then build from this directory:

```bash
idf.py set-target esp32c3
idf.py build
```

The folder is self-contained and includes its own BSP, default configuration,
protected partition layout, dependency lock, and bootloader Recovery hook. It
does not depend on the Minecraft firmware repository. When this folder becomes
its own repository, keep those hardware contracts intact. Do not overwrite the
`cardid` or permanent Recovery regions while flashing.

For a provisioned device connected over USB, use the segmented command above
(`idf.py flash`). It writes the bootloader at `0x0`, the partition table at
`0x8000`, and the application at `0x10000` while leaving `cardid` and permanent
Recovery untouched. The merged `FoloToy-Suzhou-Travel-full.bin` is an image for
offset `0x0`; never write that whole file at `0x8000` or `0x10000`, because the
device will fail to parse its partition table and repeatedly reboot to a black
screen.

If the screen is black, inspect the USB log with:

```bash
idf.py -p <PORT> monitor
```

`partition 0 invalid magic number 0x3e9` means a complete image was written at
the partition-table address. Exit the monitor with `Ctrl+]`, then repair the
three normal segments with `idf.py -p <PORT> flash`.

Run the hardware-independent state test with:

```bash
cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_suzhou_travel_state.c main/suzhou_travel_state.c \
  -o /tmp/test_suzhou_travel_state
/tmp/test_suzhou_travel_state
```

Or run the complete host and firmware gate after activating ESP-IDF 5.5.3:

```bash
./tools/validate.sh
```

The gate builds a merged image at
`build/FoloToy-Suzhou-Travel-full.bin` and verifies the 8 MB Flash header,
3 MB factory-app ceiling, protected `cardid` and Recovery regions, partition
table checksum, and Recovery boot hook.

## Asset provenance

The garden header is original project artwork generated with the built-in OpenAI
image tool. The production prompt requested a text-free Suzhou garden scene with
a moon gate, stone bridge, white walls, dark roof tiles, bamboo, red maple, mist,
and a restrained ink-and-mineral-pigment style on warm xuan paper. The generated
source and 240 × 96 preview live in `assets/images/`; the app compiles a
46,080-byte RGB565 derivative from Flash.

`tools/generate_assets.py` regenerates the RGB565 C source with FFmpeg and the
Chinese font subset with `lv_font_conv` 1.5.3. The font source is Noto Sans CJK
SC Regular under the SIL Open Font License 1.1 and is intentionally not copied
into this application.

`tools/generate_suzhou_tts.py` uses the macOS Tingting system voice to generate
twenty project-authored 16 kHz, 16-bit mono WAV clips under
`assets/music/suzhou_tts/`, then packs them into IMA-ADPCM for chunked playback
from Flash. No third-party recording is included.
