#!/usr/bin/env python3
"""Compose the Clean Sweep community cover from the host-rendered game screen.

The cover is drawn here rather than painted by an image model: the tetrominoes
use the same palette and brick shading as the running app, and the framed screen
in the middle is the real LVGL render, so nothing on the cover promises a
feature the firmware does not have.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
IMAGES = ROOT / "assets/images/clean_sweep"
FONT = ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf"
W, H = 1152, 1536
NIGHT, GRID, INK, PAPER = (14, 26, 43), (22, 38, 63), (23, 32, 42), (244, 244, 234)
YELLOW, GLOW = (255, 217, 40), (255, 246, 196)
COLORS = [(0x35, 0xD6, 0xE8), (0x3E, 0x6B, 0xE6), (0xFF, 0x9A, 0x2E), (0xFF, 0xD9, 0x28),
          (0x62, 0xD6, 0x3C), (0xF0, 0x44, 0x38), (0xB2, 0x64, 0xF0)]
SHAPES = {
    "I": [(0, 0), (1, 0), (2, 0), (3, 0)],
    "J": [(0, 0), (0, 1), (1, 1), (2, 1)],
    "L": [(2, 0), (0, 1), (1, 1), (2, 1)],
    "O": [(0, 0), (1, 0), (0, 1), (1, 1)],
    "S": [(1, 0), (2, 0), (0, 1), (1, 1)],
    "T": [(1, 0), (0, 1), (1, 1), (2, 1)],
}


def shade(color, percent):
    if percent >= 0:
        return tuple(int(c + (255 - c) * percent / 100) for c in color)
    return tuple(int(c * (100 + percent) / 100) for c in color)


def brick(draw, x, y, size, color):
    draw.rounded_rectangle((x, y, x + size - 1, y + size - 1), 6, fill=shade(color, -50))
    draw.rounded_rectangle((x + 5, y + 5, x + size - 10, y + size - 10), 5, fill=color)
    draw.rounded_rectangle((x + 11, y + 11, x + size // 3, y + size // 3), 3,
                           fill=shade(color, 50))


def piece(draw, name, x, y, size, color):
    for cx, cy in SHAPES[name]:
        brick(draw, x + cx * size, y + cy * size, size, color)


def centered(draw, text, y, font, fill, shadow=INK, offset=6):
    width = draw.textbbox((0, 0), text, font=font)[2]
    x = (W - width) // 2
    if shadow:
        draw.text((x + offset, y + offset), text, font=font, fill=shadow)
    draw.text((x, y), text, font=font, fill=fill)


cover = Image.new("RGB", (W, H), NIGHT)
draw = ImageDraw.Draw(cover)
for x in range(0, W, 48):
    draw.line((x, 0, x, H), fill=GRID)
for y in range(0, H, 48):
    draw.line((0, y, W, y), fill=GRID)

title = ImageFont.truetype(str(FONT), 150)
tagline = ImageFont.truetype(str(FONT), 58)
keys = ImageFont.truetype(str(FONT), 46)
chip = ImageFont.truetype(str(FONT), 40)

centered(draw, "俄罗斯方块", 96, title, PAPER)
centered(draw, "三个键，消到一格不留", 330, tagline, YELLOW, shadow=None)

# 角落里的四格方块,和游戏里同一套配色。
piece(draw, "L", 40, 200, 44, COLORS[2])
piece(draw, "S", 972, 208, 44, COLORS[4])
piece(draw, "T", 60, 1236, 44, COLORS[6])
piece(draw, "I", 900, 1256, 44, COLORS[0])

screen = Image.open(IMAGES / "tetris.png").resize((600, 800), Image.NEAREST)
sx, sy = (W - 600) // 2, 440
draw.rectangle((sx - 18, sy - 18, sx + 617, sy + 817), fill=INK)
draw.rectangle((sx - 8, sy - 8, sx + 607, sy + 807), fill=GLOW)
cover.paste(screen, (sx, sy))

centered(draw, "上左　下右　确定转　双击落底", 1310, keys, PAPER, shadow=None)
labels = ["满行就消", "离线可玩", "全中文"]
box_w, gap = 268, 24
start = (W - (box_w * len(labels) + gap * (len(labels) - 1))) // 2
for index, label in enumerate(labels):
    x = start + index * (box_w + gap)
    draw.rounded_rectangle((x, 1394, x + box_w, 1476), 14, fill=INK, outline=YELLOW, width=4)
    width = draw.textbbox((0, 0), label, font=chip)[2]
    draw.text((x + (box_w - width) // 2, 1412), label, font=chip, fill=PAPER)

cover.save(IMAGES / "community-cover.png")
print(IMAGES / "community-cover.png")
