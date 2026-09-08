#!/usr/bin/env python3
"""Convert production LVGL snapshots to explicitly labeled host previews."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--input',type=Path,default=Path('/tmp/code-theater-v3-previews'))
args=parser.parse_args()
output=ROOT/'projects/code-theater/assets';output.mkdir(parents=True,exist_ok=True)
names=['boot','coding','interrupted','choice','banned','appeal-failed','appeal-passed','delivery','cards']
for name in names+['recovered','full-cards','no-buttons']:
    with Image.open(args.input/(name+'.ppm')) as im: im.save(output/(name+'.png'))
canvas=Image.new('RGB',(760,1034),'#e9efe7')
font_path=ROOT/'managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf'
font=ImageFont.truetype(str(font_path),18)
ImageDraw.Draw(canvas).text((16,5),'代码小搭子 · 本机预览（非设备截图）',font=font,fill='#17202a')
for i,name in enumerate(names):
    with Image.open(output/(name+'.png')) as im:
        canvas.paste(im,(10+i%3*250,35+i//3*330))
canvas.save(output/'preview.png')
small=ImageFont.truetype(str(font_path),12)
for prefix,target in [('motion','motion.gif'),('interaction','interaction.gif')]:
    frames=[]
    for path in sorted(args.input.glob(prefix+'-*.ppm')):
        frame=Image.new('RGB',(240,340),'#262624')
        with Image.open(path) as source: frame.paste(source.convert('RGB'),(0,20))
        ImageDraw.Draw(frame).text((7,2),'本机预览 · 非设备截图',font=small,fill='#d8cdbf')
        frames.append(frame)
    if frames:
        frames[0].save(output/target,save_all=True,append_images=frames[1:],duration=120,loop=0,optimize=False)
