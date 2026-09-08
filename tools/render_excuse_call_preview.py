#!/usr/bin/env python3
"""Convert production UI snapshots and synthesize review-only ringtone samples."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import wave
from PIL import Image, ImageDraw, ImageFont
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--input',type=Path,default=Path('/tmp/excuse-call-previews'))
args=parser.parse_args()
out=ROOT/'projects/excuse-call/assets'; out.mkdir(parents=True,exist_ok=True)
for name in ['home','ringing','stopped','timeout','no-audio','no-buttons']:
    with Image.open(args.input/(name+'.ppm')) as im: im.save(out/(name+'.png'))
font=ImageFont.truetype(str(ROOT/'managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf'),18)
im=Image.new('RGB',(760,368),'#E9EFE7')
ImageDraw.Draw(im).text((12,8),'来电界面 · 电脑端预览（非设备截图）',font=font,fill='#17202A')
for i,name in enumerate(['home','ringing','stopped']):
    with Image.open(out/(name+'.png')) as screen: im.paste(screen,(10+i*250,40))
im.save(out/'ui-preview.png')
for ring in range(1,6):
    (out/f'ringtone-{ring}.wav').write_bytes((ROOT/f'assets/music/excuse_call/ringtone-{ring}.wav').read_bytes())
