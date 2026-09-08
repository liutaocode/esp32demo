#!/usr/bin/env python3
"""Convert production LVGL snapshots to labeled host previews (not USB evidence)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
ROOT = Path(__file__).resolve().parents[1]
p = ROOT / 'projects/pocket-breach/assets'
for f in p.glob('*.ppm'):
    with Image.open(f) as im:
        im.save(f.with_suffix('.png'))
    f.unlink()
names = [('home', '开始'), ('sand', '沙城仓库'), ('harbor', '海港货站'),
         ('neon', '霓虹基地'), ('reload', '自动换弹'), ('victory', '结算测试')]
font = ImageFont.truetype(str(ROOT / 'managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf'), 20)
out = Image.new('RGB', (784, 764), '#dce8e7')
draw = ImageDraw.Draw(out)
draw.text((16, 12), '口袋突围 · 实际代码渲染预览（非设备截图）', font=font, fill='#17333f')
for i, (name, label) in enumerate(names):
    x, y = 16 + i % 3 * 256, 52 + i // 3 * 350
    with Image.open(p / (name + '.png')) as im:
        out.paste(im, (x, y))
    draw.text((x + 60, y + 322), label, font=font, fill='#17333f')
out.save(p / 'ui-preview.png')
