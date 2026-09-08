#!/usr/bin/env python3
"""Compose the e-book reader cover from the interface's own layout constants.

Every coordinate and colour below is copied from main/apps/ebook/ebook.c, so the
picture matches what the device draws. It is a host illustration, not a device
photograph and not an LVGL capture.
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "projects/ebook/assets/cover.png"
TYPEFACE = ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf"

INK = (0x17, 0x20, 0x2A)
PAPER = (0xF4, 0xF4, 0xEA)
SKY_DARK = (0x08, 0x72, 0xC9)
YELLOW = (0xFF, 0xD9, 0x28)
FRAME = (0x76, 0x50, 0x2D)
BOARD = (0x5A, 0x3A, 0x24)
SUB = (0xF0, 0xE6, 0xD2)
SPINE = [(0xC2, 0x45, 0x2F), (0x1F, 0x6F, 0xB2), (0x5E, 0x8C, 0x3A),
         (0xC9, 0x8A, 0x2B), (0x8A, 0x5A, 0x9E), (0x2F, 0x7F, 0x79)]

# 阅读页用的是"护眼"主题,封面上最能说明这台东西是拿来读书的。
EYE_BG = (0xF2, 0xE2, 0xC4)
EYE_INK = (0x3B, 0x2E, 0x22)

SCREEN_W, SCREEN_H = 240, 320
SCALE = 2
W, H = 1152, 1536

BOOKS = [("三体", "43%"), ("活着", "12%"), ("小王子", "1.2 MB"),
         ("围城", "78%"), ("百年孤独", "2.4 MB")]
PAGE = ["他把书传进这块小屏幕里,", "一本三百页的小说,只占几", "百 KB。翻页时纸页沙沙响,",
        "下一页先重后轻,上一页反", "过来,不用看屏幕也知道翻", "对了没有。", "",
        "两秒半之后,标题栏和进度", "行自己退场,整块屏幕只剩", "文字。", "",
        "合上再打开,还在原来那一", "页。"]


def font(size):
    return ImageFont.truetype(str(TYPEFACE), size)


def shelf_screen():
    img = Image.new("RGB", (SCREEN_W, SCREEN_H), PAPER)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, SCREEN_W, 22], fill=SKY_DARK)
    d.text((6, 3), "我的书架", font=font(15), fill=(255, 255, 255))
    d.text((196, 5), "82%", font=font(11), fill=(255, 255, 255))

    d.rectangle([4, 26, 236, 298], fill=FRAME, outline=INK, width=4)
    for i in range(6):
        slot_y = 34 + i * 44
        d.rectangle([8, slot_y + 32, 232, slot_y + 38], fill=BOARD)
    for i, (title, note) in enumerate(BOOKS):
        slot_y = 34 + i * 44
        selected = i == 0
        d.rectangle([14, slot_y, 226, slot_y + 32], fill=SPINE[i], outline=INK,
                    width=3 if selected else 1)
        d.text((36, slot_y + 7), title, font=font(15), fill=(255, 255, 255))
        d.text((224 - d.textlength(note, font=font(11)), slot_y + 10), note,
               font=font(11), fill=SUB)
        if selected:
            d.rectangle([20, slot_y - 4, 30, slot_y + 36], fill=YELLOW, outline=INK, width=2)
    d.text((8, 302), "5 本 · 余 2.9 MB", font=font(11), fill=(0x5A, 0x6A, 0x72))
    return img


def reader_screen():
    """全屏阅读:标题栏与页脚都收起来了,只剩文字。"""
    img = Image.new("RGB", (SCREEN_W, SCREEN_H), EYE_BG)
    d = ImageDraw.Draw(img)
    y = 6
    for line in PAGE:
        d.text((8, y), line, font=font(16), fill=EYE_INK)
        y += 22
    return img


def main():
    canvas = Image.new("RGB", (W, H), (0x2A, 0x1E, 0x14))
    d = ImageDraw.Draw(canvas)
    d.rectangle([0, 0, W, 250], fill=(0x1A, 0x12, 0x0C))
    d.text((70, 74), "电子书", font=font(96), fill=(0xF2, 0xE2, 0xC4))
    d.text((72, 178), "手机传书 · 满屏只剩文字 · 接着上次读",
           font=font(34), fill=(0xC8, 0xA2, 0x72))

    for index, screen in enumerate((shelf_screen(), reader_screen())):
        big = screen.resize((SCREEN_W * SCALE, SCREEN_H * SCALE), Image.NEAREST)
        x = 60 + index * (SCREEN_W * SCALE + 72)
        y = 380
        d.rectangle([x - 8, y - 8, x + SCREEN_W * SCALE + 8, y + SCREEN_H * SCALE + 8],
                    fill=INK)
        canvas.paste(big, (x, y))

    d.text((70, 1120), "手机扫码就能把书传进去,书架上一本本立着",
           font=font(34), fill=(0xF0, 0xE6, 0xD2))
    d.text((70, 1186), "翻开两秒半,页边的东西自己退场,只剩文字",
           font=font(34), fill=(0xF0, 0xE6, 0xD2))
    d.text((70, 1252), "合上再打开,还在原来那一页",
           font=font(34), fill=(0xF0, 0xE6, 0xD2))
    d.text((70, 1350), "五套配色 · 三档字号 · 书签 · 阅读时长 · 翻页沙沙声",
           font=font(30), fill=(0xC8, 0xA2, 0x72))
    d.text((70, 1440), "主机绘制的界面示意图,非真机照片",
           font=font(22), fill=(0x7A, 0x6A, 0x58))

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(OUTPUT)
    print(f"{OUTPUT} ({OUTPUT.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
