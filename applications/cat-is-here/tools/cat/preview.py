#!/usr/bin/env python3
"""Package production-rendered images with an explicit host-preview label."""
from pathlib import Path
import argparse,subprocess,tempfile
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',type=Path,default=ROOT/'build/cat-host/preview');a=p.parse_args()
font_path=ROOT/'managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf'
font=ImageFont.truetype(str(font_path),18); small=ImageFont.truetype(str(font_path),12)
out=ROOT/'review';out.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='cat-frames-') as temp:
    tmp=Path(temp);subprocess.run([str(a.binary.resolve()),'--capture'],cwd=tmp,check=True)
    names=['coat-0-pose-00','coat-0-pose-06','coat-0-pose-07','coat-0-pose-04','nest','memories']
    canvas=Image.new('RGB',(760,735),'#ece5d7');ImageDraw.Draw(canvas).text((15,10),'小猫在呢 · 本机预览（非设备截图）',font=font,fill='#49392f')
    for i,name in enumerate(names):
        with Image.open(tmp/f'{name}.ppm') as im: canvas.paste(im,(10+i%3*250,45+i//3*345))
    canvas.save(out/'host-preview.png')
    coats=Image.new('RGB',(760,385),'#ece5d7');ImageDraw.Draw(coats).text((15,10),'三种毛色 · 本机预览（非设备截图）',font=font,fill='#49392f')
    for i in range(3):
        with Image.open(tmp/f'coat-{i}-pose-00.ppm') as im: coats.paste(im,(10+i*250,45))
    coats.save(out/'host-coats.png')
    frames=[]
    for path in sorted(tmp.glob('motion-*.ppm')):
        im=Image.new('RGB',(240,340),'#ece5d7');ImageDraw.Draw(im).text((8,2),'本机预览 · 非设备截图',font=small,fill='#49392f')
        with Image.open(path) as source: im.paste(source,(0,20))
        frames.append(im)
    frames[0].save(out/'host-interaction.gif',save_all=True,append_images=frames[1:],duration=120,loop=0,optimize=False)
    frames=[]
    for path in sorted(tmp.glob('dance-*.ppm')):
        im=Image.new('RGB',(240,340),'#ece5d7');ImageDraw.Draw(im).text((8,2),'本机预览 · 非设备截图',font=small,fill='#49392f')
        with Image.open(path) as source: im.paste(source,(0,20))
        frames.append(im)
    frames[0].save(out/'host-dance.gif',save_all=True,append_images=frames[1:],duration=80,loop=0,optimize=False)
print(f'Host previews written to {out}; not device captures or publishing covers.')
