#!/usr/bin/env python3
"""Convert production LVGL snapshots; these are host previews, not USB evidence."""
from pathlib import Path
import argparse
from PIL import Image, ImageDraw, ImageFont
ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--input', type=Path, required=True)
args = parser.parse_args()
p = ROOT / 'projects/ricochet-rush/assets'
p.mkdir(parents=True, exist_ok=True)
for f in args.input.glob('*.ppm'):
    with Image.open(f) as im:
        im.save(p / (f.stem + '.png'))
font = ImageFont.truetype(str(ROOT / 'managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf'), 20)
names = [('home', '开始'), ('aim', '瞄准'), ('flight', '连弹'), ('danger', '边界测试'), ('pause', '暂停'), ('victory', '通关测试')]
out = Image.new('RGB', (784, 756), '#e8f0eb')
d = ImageDraw.Draw(out)
d.text((24, 15), '再弹一轮 · 本机界面预览（非实机截图）', font=font, fill='#172c42')
for i, (name, label) in enumerate(names):
    x = 16 + (i % 3) * 256
    y = 54 + (i // 3) * 350
    with Image.open(p / (name + '.png')) as im:
        out.paste(im, (x, y))
    d.text((x + 70, y + 322), label, font=font, fill='#172c42')
out.save(p / 'ui-preview.png')
