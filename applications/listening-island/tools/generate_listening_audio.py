#!/usr/bin/env python3
"""Original English -> Kokoro v1.0 voice af_heart -> 16 kHz IMA ADPCM banks."""
from pathlib import Path
import argparse, json, hashlib, subprocess, struct, wave, zlib
from minecraft_adpcm_encode import encode
ROOT=Path(__file__).resolve().parents[1]
APP=ROOT/'main/apps/listening'
OUT=ROOT/'assets/music/listening'
p=argparse.ArgumentParser();p.add_argument('--synthesize',action='store_true');p.add_argument('--check',action='store_true');args=p.parse_args()
data=json.loads((APP/'catalog.json').read_text());rows=data['sentences'];assert len(rows)==384
assert len(set(r['en'] for r in rows))==384
assert all(r['id']==i and r['topic']==i//48 and 0<len(r['zh'])<=15 for i,r in enumerate(rows))
if args.synthesize:
 import torch, numpy as np, soundfile as sf
 from kokoro import KPipeline
 torch.set_num_threads(4)
 pipeline=KPipeline(lang_code='a',repo_id='hexgrad/Kokoro-82M',device='cpu')
 for r in rows:
  out=OUT/f"{r['id']:03}.wav"
  if out.exists():continue
  audio=np.concatenate([a.numpy() for _,_,a in pipeline(r['en'],voice='af_heart',speed=0.87)])
  tmp=OUT/'working.wav';sf.write(tmp,audio,24000)
  subprocess.run(['ffmpeg','-v','error','-y','-i',str(tmp),'-af','silenceremove=start_periods=1:start_threshold=-45dB,areverse,silenceremove=start_periods=1:start_threshold=-45dB,areverse,apad=pad_dur=0.18','-ar','16000','-ac','1','-c:a','pcm_s16le',str(out)],check=True)
  print('Synthesized',r['id'],r['en'],flush=True)
 (OUT/'working.wav').unlink(missing_ok=True)
banks=[bytearray(),bytearray()];clips=[];manifest=[]
for r in rows:
 path=OUT/f"{r['id']:03}.wav"
 with wave.open(str(path)) as w:
  assert (w.getnchannels(),w.getsampwidth(),w.getframerate())==(1,2,16000)
  n=w.getnframes();pcm=w.readframes(n)
 assert 8000<n<240000
 samples=struct.unpack(f'<{n}h',pcm);peak=max(abs(s) for s in samples);assert peak>1000
 # Peak normalize from immutable source WAVs, retaining 1.5 dB headroom.
 gain=(32767*10**(-1.5/20))/peak
 samples=tuple(round(v*gain) for v in samples)
 assert max(abs(v) for v in samples)<=27572
 blob,pred,step=encode(samples)
 bank=0 if len(banks[0])+len(blob)<=1600000 else 1
 assert bank==0 or len(banks[1])+len(blob)<=0x3A0000
 clips.append(f' {{{len(banks[bank])},{len(blob)},{n},{pred},{step},{bank}}},')
 banks[bank].extend(blob)
 manifest.append(dict(**r,samples=n,bank=bank,normalization_gain=round(gain,6),target_peak_dbfs=-1.5,sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
src='#include "listening_catalog.h"\nconst char *const li_topics[LI_TOPICS]={'+','.join(json.dumps(s,ensure_ascii=False) for s in data['topics'])+'};\n'
src+='const char *const li_meanings[LI_COUNT]={'+','.join(json.dumps(r['zh'],ensure_ascii=False) for r in rows)+'};\n'
src+='const char *const li_english[LI_COUNT]={'+','.join(json.dumps(r['en']) for r in rows)+'};\n'
src+='const li_clip_t li_clips[LI_COUNT]={\n'+'\n'.join(clips)+'\n};\n'
src+='const size_t li_bank_sizes[2]={'+','.join(str(len(b)) for b in banks)+'};\n'
src+=f'const uint32_t li_resource_crc = {zlib.crc32(banks[1])}u;\n'
outputs={APP/'listening_catalog.c':src.encode(),OUT/'listening_a.bin':bytes(banks[0]),OUT/'listening_b.bin':bytes(banks[1]),OUT/'manifest.json':(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode()}
for path,content in outputs.items():
 if args.check:assert path.read_bytes()==content,f'Stale {path}'
 else:path.write_bytes(content)
print(json.dumps(dict(sentences=len(rows),audio_seconds=sum(m['samples'] for m in manifest)/16000,bank_bytes=list(map(len,banks)))))
