#!/usr/bin/env python3
"""Compose the Pocket Arcade community cover from the verified device screen.

    python3 tools/render_pocket_arcade_cover.py \
        projects/pocket-arcade/assets/device-lobby.png \
        projects/pocket-arcade/assets/cover.png

The cover is portrait 3:4 at 1152 x 1536, drawn in the application's own flat
pixel style, and its centre is the lobby exactly as the application draws it,
scaled up whole. No hardware is drawn: it is a picture of the play, and nothing
on it claims to be a photograph.
"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf"
W, H = 1152, 1536
INK = (0x17, 0x20, 0x2A)
SKY = (0x16, 0x89, 0xE8)
SKY_DARK = (0x08, 0x72, 0xC9)
GRASS = (0x82, 0xBE, 0x2D)
GRASS_DARK = (0x55, 0x95, 0x1D)
GRASS_LIGHT = (0xA7, 0xD9, 0x3E)
PLATE = (0x76, 0x50, 0x2D)
PLATE_TOP = (0x69, 0xA7, 0x2C)
PAPER = (0xF4, 0xF4, 0xEA)
WHITE = (0xFF, 0xFF, 0xFF)
DIRT = (0x75, 0x45, 0x2E)


def centred(draw, text, font, cx, y, fill, shadow=None):
    left, top, right, bottom = draw.textbbox((0, 0), text, font=font)
    x = cx - (right - left) / 2 - left
    if shadow:
        draw.text((x + 6, y + 6), text, font=font, fill=shadow)
    draw.text((x, y), text, font=font, fill=fill)
    return bottom - top


def main() -> int:
    shot_path = Path(sys.argv[1])
    out = Path(sys.argv[2])
    shot = Image.open(shot_path).convert("RGB")
    if shot.size != (240, 320):
        raise SystemExit(f"expected a 240x320 screen, got {shot.size}")

    cover = Image.new("RGB", (W, H), SKY)
    draw = ImageDraw.Draw(cover)

    # Sky band, so the top reads a shade deeper than the horizon.
    draw.rectangle([0, 0, W, 360], fill=SKY_DARK)

    # One cloud, the same shape the application draws in its own corner.
    def cloud(x, y, scale):
        draw.rectangle([x + 4, y + 28, x + 172, y + 68], fill=INK)
        draw.rectangle([x + 20, y + 16, x + 160, y + 56], fill=WHITE)
        draw.rectangle([x + 48, y, x + 88, y + 36], fill=WHITE)
        draw.rectangle([x + 108, y + 4, x + 144, y + 36], fill=WHITE)
    cloud(852, 96, 1)
    cloud(96, 190, 1)

    # Title plate, in the application's own furniture.
    plate = [96, 96, 1056, 268]
    draw.rectangle([plate[0] + 16, plate[1] + 20, plate[2] + 16, plate[3] + 20], fill=INK)
    draw.rectangle(plate, fill=PLATE, outline=INK, width=10)
    draw.rectangle([plate[0], plate[1], plate[2], plate[1] + 26], fill=PLATE_TOP)

    title_font = ImageFont.truetype(str(FONT), 108)
    sub_font = ImageFont.truetype(str(FONT), 46)
    centred(draw, "口袋游戏厅", title_font, W // 2, plate[1] + 40, WHITE, shadow=INK)

    # The lobby interface, scaled up whole so no pixel is invented. No device
    # body is drawn: the publisher's rules only allow one that matches its own
    # hardware reference, and this cover is about the play, not the board.
    scale = 3
    screen = shot.resize((240 * scale, 320 * scale), Image.NEAREST)
    sx = (W - screen.width) // 2
    sy = 322
    draw.rectangle([sx + 20, sy + 24, sx + screen.width + 20, sy + screen.height + 24], fill=INK)
    draw.rectangle([sx - 14, sy - 14, sx + screen.width + 14, sy + screen.height + 14], fill=INK)
    cover.paste(screen, (sx, sy))

    # Ground, then the one line that says what this is.
    draw.rectangle([0, 1440, W, H], fill=GRASS)
    draw.rectangle([0, 1440, W, 1456], fill=GRASS_LIGHT)
    for x in range(0, W, 120):
        draw.rectangle([x, 1502, x + 72, 1526], fill=GRASS_DARK)
        draw.rectangle([x + 72, 1512, x + 120, 1526], fill=DIRT)

    band = [72, 1320, 1080, 1424]
    draw.rectangle([band[0] + 14, band[1] + 16, band[2] + 14, band[3] + 16], fill=INK)
    draw.rectangle(band, fill=PAPER, outline=INK, width=8)
    centred(draw, "三十二个游戏　三颗按键　完全离线", sub_font, W // 2, band[1] + 26, INK)

    out.parent.mkdir(parents=True, exist_ok=True)
    cover.save(out)
    print(out, cover.size)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
