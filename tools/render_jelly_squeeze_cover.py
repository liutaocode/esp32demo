#!/usr/bin/env python3
"""Compose the Jelly Squeeze promotional cover from the app's own palette and shapes.

The result is an illustration built with Pillow, not a device capture and not a
screenshot. It reuses the colors and the deformation table the firmware uses so
the artwork cannot drift away from what the screen actually shows.
"""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf"
W, H = 1086, 1448

SKY = (22, 137, 232)
SKY_SOFT = (150, 208, 246)
INK = (23, 32, 42)
PAPER = (244, 244, 234)
GRASS = (130, 190, 45)
GRASS_DARK = (85, 149, 29)
STEEL = (126, 140, 160)
STEEL_RIM = (176, 188, 203)
STEEL_DIM = (90, 102, 117)
YELLOW = (255, 217, 40)
FLOOR = (224, 185, 140)

# 与固件 jelly_squeeze.c 中的口味表一致。
FLAVORS = [
    ("草莓", (244, 101, 126), (201, 60, 88), (255, 182, 196)),
    ("柠檬", (255, 210, 77), (217, 165, 25), (255, 239, 175)),
    ("青提", (143, 214, 90), (95, 165, 49), (213, 242, 182)),
    ("蜜桃", (255, 164, 107), (220, 118, 56), (255, 211, 182)),
    ("蓝莓", (124, 155, 242), (76, 107, 199), (195, 211, 251)),
    ("葡萄", (185, 139, 234), (139, 95, 194), (225, 204, 247)),
    ("薄荷", (111, 220, 200), (58, 168, 149), (194, 243, 234)),
    ("可乐", (192, 138, 87), (142, 97, 52), (232, 203, 174)),
]
# 与固件 jelly_squeeze_state.c 中的五种标准体型一致（宽度，高度）。
SHAPES = [(86, 42), (72, 50), (60, 60), (50, 72), (42, 86)]


def font(size):
    return ImageFont.truetype(str(FONT), size)


def jelly(draw, box, flavor, outline=9):
    """Draw one jelly using the same rounded body, gloss, eyes and smile as the device."""
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    body, shade, blush = flavor[1], flavor[2], flavor[3]
    radius = int(min(w, h) * 0.48)
    draw.rounded_rectangle(box, radius=radius, fill=body, outline=INK, width=outline)
    sw, sh = w - int(min(w, h) * 0.40), int(h * 0.22)
    draw.rounded_rectangle(
        (x0 + (w - sw) // 2, y1 - sh - outline, x0 + (w + sw) // 2, y1 - outline),
        radius=sh // 2, fill=shade)
    gw, gh = int(w * 0.26), int(h * 0.20)
    draw.rounded_rectangle((x0 + int(w * 0.20), y0 + int(h * 0.14),
                            x0 + int(w * 0.20) + gw, y0 + int(h * 0.14) + gh),
                           radius=gh // 2, fill=(255, 255, 255))
    ew, eh = max(8, int(w * 0.17)), max(8, int(h * 0.20))
    ey = y0 + int(h * 0.32)
    for side in (-1, 1):
        ex = x0 + w // 2 + side * int(w * 0.22) - ew // 2
        draw.rounded_rectangle((ex, ey, ex + ew, ey + eh), radius=ew // 2, fill=INK)
        s = max(3, ew // 3)
        draw.rounded_rectangle((ex + ew // 5, ey + eh // 5, ex + ew // 5 + s, ey + eh // 5 + s),
                               radius=s // 2, fill=(255, 255, 255))
    cw, ch = max(6, int(w * 0.16)), max(4, int(h * 0.09))
    cy = ey + eh + int(h * 0.05)
    for side in (-1, 1):
        cx = x0 + w // 2 + side * int(w * 0.33) - cw // 2
        draw.rounded_rectangle((cx, cy, cx + cw, cy + ch), radius=ch // 2, fill=blush)
    mw = max(12, int(w * 0.22))
    mh = max(4, int(h * 0.05))
    mx, my = x0 + w // 2 - mw // 2, cy + ch // 2
    for i, drop in enumerate((0, mh // 2, 0)):
        draw.rounded_rectangle((mx + i * mw // 3, my + drop, mx + (i + 1) * mw // 3, my + drop + mh),
                               radius=mh // 2, fill=INK)


def outlined_text(draw, xy, text, size, fill, edge=INK, width=7, anchor="mm"):
    face = font(size)
    x, y = xy
    for dx in range(-width, width + 1):
        for dy in range(-width, width + 1):
            if dx * dx + dy * dy <= width * width:
                draw.text((x + dx, y + dy), text, font=face, fill=edge, anchor=anchor)
    draw.text((x, y), text, font=face, fill=fill, anchor=anchor)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path,
                        default=ROOT / "projects/jelly-squeeze/assets/cover-draft.png")
    args = parser.parse_args()
    args.out.parent.mkdir(parents=True, exist_ok=True)

    image = Image.new("RGB", (W, H), SKY)
    draw = ImageDraw.Draw(image)
    for y in range(H):
        blend = min(1.0, y / (H * 0.72))
        draw.line([(0, y), (W, y)],
                  fill=tuple(int(SKY[i] + (SKY_SOFT[i] - SKY[i]) * blend) for i in range(3)))
    for cx, cy, s in ((150, 180, 1.0), (880, 300, 0.7), (620, 130, 0.5)):
        draw.rounded_rectangle((cx, cy, cx + int(230 * s), cy + int(64 * s)),
                               radius=int(32 * s), fill=(255, 255, 255))
        draw.ellipse((cx + int(56 * s), cy - int(40 * s), cx + int(160 * s), cy + int(52 * s)),
                     fill=(255, 255, 255))

    # 压模机：两根导轨夹着一块闸门，中间留出门洞。
    machine = (78, 470, W - 78, 1096)
    draw.rounded_rectangle(machine, radius=26, fill=(230, 245, 252), outline=INK, width=10)
    for side in (0, 1):
        rx = machine[0] + 10 if side == 0 else machine[2] - 44
        draw.rectangle((rx, machine[1] + 10, rx + 34, machine[3] - 10), fill=STEEL_RIM)
        for i in range(8):
            draw.rectangle((rx + 6, machine[1] + 40 + i * 66, rx + 28, machine[1] + 78 + i * 66),
                           fill=STEEL)

    gate_top, gate_bottom = 606, 1012
    hole_w, hole_h = 344, 244
    hole_left, hole_right = W // 2 - hole_w // 2, W // 2 + hole_w // 2
    draw.rectangle((machine[0] + 10, gate_top, hole_left, gate_bottom), fill=STEEL)
    draw.rectangle((hole_right, gate_top, machine[2] - 10, gate_bottom), fill=STEEL)
    draw.rectangle((hole_left, gate_top, hole_right, gate_bottom - hole_h), fill=STEEL)
    draw.rectangle((machine[0] + 10, gate_top, machine[2] - 10, gate_top + 16), fill=STEEL_RIM)
    for i in range(3):
        for x in (machine[0] + 66, machine[2] - 100):
            draw.ellipse((x, gate_top + 52 + i * 96, x + 34, gate_top + 86 + i * 96), fill=STEEL_DIM)
    frame_w = 12
    draw.rectangle((hole_left - frame_w, gate_bottom - hole_h - frame_w, hole_left, gate_bottom), fill=INK)
    draw.rectangle((hole_right, gate_bottom - hole_h - frame_w, hole_right + frame_w, gate_bottom), fill=INK)
    draw.rectangle((hole_left - frame_w, gate_bottom - hole_h - frame_w,
                    hole_right + frame_w, gate_bottom - hole_h), fill=INK)

    # 地面与正在挤过门洞的那只果冻。
    draw.rectangle((machine[0] + 10, gate_bottom, machine[2] - 10, machine[3] - 10), fill=FLOOR)
    draw.rectangle((machine[0] + 10, gate_bottom, machine[2] - 10, gate_bottom + 10), fill=INK)
    for x in range(machine[0] + 30, machine[2] - 40, 74):
        draw.rounded_rectangle((x, gate_bottom + 30, x + 42, gate_bottom + 48), radius=8,
                               fill=(196, 154, 108))
    hero_w, hero_h = 310, 232
    jelly(draw, (W // 2 - hero_w // 2, gate_bottom - hero_h, W // 2 + hero_w // 2, gate_bottom),
          FLAVORS[0], outline=11)

    outlined_text(draw, (W // 2, 250), "一挤就过", 190, (255, 255, 255), width=11)
    outlined_text(draw, (W // 2, 390), "看门洞，把果冻挤成那形状", 60, YELLOW, width=7)

    # 五种体型：闸门只会要求这五个形状。
    row_y = 1208
    slot = (W - 140) // len(SHAPES)
    for index, (sw, sh) in enumerate(SHAPES):
        scale = 1.8
        bw, bh = int(sw * scale), int(sh * scale)
        cx = 70 + slot * index + slot // 2
        jelly(draw, (cx - bw // 2, row_y - bh // 2, cx + bw // 2, row_y + bh // 2),
              FLAVORS[index + 1], outline=7)

    draw.rectangle((0, H - 132, W, H), fill=GRASS)
    draw.rectangle((0, H - 132, W, H - 118), fill=(167, 217, 62))
    for x in range(0, W, 96):
        draw.rectangle((x, H - 46, x + 56, H), fill=GRASS_DARK)
    outlined_text(draw, (W // 2, H - 66), "上拉高　下压扁　确定稳住", 54, PAPER, width=6)

    image.save(args.out)
    print(f"{args.out} {image.size[0]}x{image.size[1]}")


if __name__ == "__main__":
    main()
