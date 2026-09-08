#!/usr/bin/env python3
"""Render production UI motion and synthesize the review audio; requires Pillow."""
from pathlib import Path
import json,subprocess,tempfile,wave
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT.parent/'review/preview'
OUT.mkdir(parents=True,exist_ok=True)
subprocess.run([str(ROOT/'build/host-tally/preview')],cwd=OUT,check=True)
for p in OUT.glob('*.ppm'):
    Image.open(p).save(p.with_suffix('.png'))
    p.unlink()
board=Image.new('RGB',(1000,360),'#172125');draw=ImageDraw.Draw(board)
for i,name in enumerate(['count','pause','history','maximum']):
    board.paste(Image.open(OUT/f'{name}.png'),(i*250+5,24))
    draw.text((i*250+5,5),name.upper()+' / LVGL PREVIEW',fill='#b5f76b')
board.save(OUT/'contact-sheet.png')
frames=[]
for effect in [1,2,3,4,5]:
    for i in range(17):
        im=Image.open(OUT/f'effect-{effect}-{i:02d}.png').convert('RGB')
        frames.append(im.resize((480,640),Image.Resampling.NEAREST))
frames[0].save(OUT/'effects.gif',save_all=True,append_images=frames[1:],duration=40,loop=0,disposal=2)
with tempfile.TemporaryDirectory(prefix='tally-audio-review-') as tmp:
    binary=Path(tmp)/'sound'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Imain/tally','tests/tally_sound_preview.c','main/tally/tally_sound.c','-lm','-o',str(binary)],cwd=ROOT,check=True)
    pcm=b'';index=[]
    for effect,label in [(1,'下键加一'),(2,'上键减一'),(3,'确定暂停'),(4,'确定继续'),(5,'归档成功')]:
        data=subprocess.check_output([str(binary),str(effect)])
        index.append({'label':label,'start_seconds':len(pcm)/32000,'duration_seconds':len(data)/32000})
        pcm+=data+bytes(16000)
    with wave.open(str(OUT/'key-sounds.wav'),'wb') as wav:
        wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(16000);wav.writeframes(pcm)
    (OUT/'key-sounds.json').write_text(json.dumps(index,ensure_ascii=False,indent=2)+'\n')
print('Host-rendered motion and production sound previews ready; no device involved.')
