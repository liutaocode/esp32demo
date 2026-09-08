#!/usr/bin/env python3
"""Export an audible host preview from the exact packed firmware banks."""
from pathlib import Path
import re,struct,wave,argparse
from minecraft_adpcm_encode import STEP_TABLE,INDEX_TABLE
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('output',type=Path);a=p.parse_args()
clips=[tuple(map(int,m)) for m in re.findall(r'\{(\d+),(\d+),(\d+),(-?\d+),(\d+),(\d+)\}',(R/'main/apps/listening/listening_catalog.c').read_text())]
banks=[(R/f'assets/music/listening/listening_{b}.bin').read_bytes() for b in ['a','b']]
samples=[]
for i in range(0,384,48):
 offset,size,n,pred,index,bank=clips[i];packed=banks[bank][offset:offset+size];samples.append(pred)
 for j in range(n-1):
  b=packed[j//2];code=b>>4 if j%2 else b&15;step=STEP_TABLE[index];diff=step>>3
  if code&1:diff+=step>>2
  if code&2:diff+=step>>1
  if code&4:diff+=step
  pred=max(-32768,min(32767,pred+(-diff if code&8 else diff)));index=max(0,min(88,index+INDEX_TABLE[code]));samples.append(pred)
 samples.extend([0]*8000)
with wave.open(str(a.output),'wb') as w:
 w.setparams((1,2,16000,0,'NONE','not compressed'));w.writeframes(struct.pack('<'+'h'*len(samples),*samples))
print('Host sample exported from packed banks')
