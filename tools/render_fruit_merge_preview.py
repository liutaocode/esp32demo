#!/usr/bin/env python3
"""Convert production LVGL PPM snapshots into clearly labeled host previews."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--input', type=Path, default=Path('/tmp/fruit-merge-previews'))
args = parser.parse_args()
output = ROOT / 'projects/fruit-merge/assets'
output.mkdir(parents=True, exist_ok=True)
for p in args.input.glob('*.ppm'):
    with Image.open(p) as im:
        im.save(output / (p.stem + '.png'))
names = ['home', 'rules', 'playing', 'chain', 'fruit-book-large', 'round-result']
canvas = Image.new('RGB', (760, 704), '#e9efe7')
font = ImageFont.truetype(str(ROOT / 'managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf'), 18)
ImageDraw.Draw(canvas).text((16, 5), '再合一颗 · 本机模拟预览（非设备截图）', font=font, fill='#17202a')
for i, name in enumerate(names):
    with Image.open(output / (name + '.png')) as im:
        canvas.paste(im, (10 + i % 3 * 250, 35 + i // 3 * 330))
canvas.save(output / 'preview.png')
