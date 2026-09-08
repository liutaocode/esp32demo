#!/usr/bin/env python3
"""Generate offline Mandarin speech from original dialogue; verify shipped assets."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import subprocess
import tempfile
import wave
from minecraft_adpcm_encode import encode
ROOT=Path(__file__).resolve().parents[1]
APP=ROOT/'main'
DEST=ROOT/'assets/music/mouthy_bean'
PHRASES=re.findall(r'"([^"]+)"',(APP/'bean_state.c').read_text().split('bean_lines[BEAN_LINES] = {')[1].split('};')[0])
assert len(PHRASES)==40
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--synthesize',action='store_true')
parser.add_argument('--check',action='store_true')
args=parser.parse_args()
assert not(args.synthesize and args.check)
DEST.mkdir(parents=True,exist_ok=True)
if args.synthesize:
    with tempfile.TemporaryDirectory(prefix='bean-tts-') as temp:
        for i,text in enumerate(PHRASES):
            aiff=Path(temp)/'speech.aiff'; wav=Path(temp)/'speech.wav'
            subprocess.run(['say','-v','Tingting','-r','205','-o',str(aiff),text],check=True)
            subprocess.run(['ffmpeg','-v','error','-y','-i',str(aiff),'-ar','16000','-ac','1','-c:a','pcm_s16le',str(wav)],check=True)
            with wave.open(str(wav),'rb') as f:
                data=struct.unpack(f'<{f.getnframes()}h',f.readframes(f.getnframes()))
            audible=[j for j,v in enumerate(data) if abs(v)>130]
            assert audible,text
            data=data[max(0,audible[0]-160):min(len(data),audible[-1]+1600)]
            peak=max(abs(v) for v in data)
            data=tuple(round(v*20000/peak) for v in data)
            with wave.open(str(DEST/f'voice_{i:02d}.wav'),'wb') as f:
                f.setparams((1,2,16000,0,'NONE','not compressed'))
                f.writeframes(struct.pack(f'<{len(data)}h',*data))
blob=bytearray(); rows=[]; clips=[]
for i,text in enumerate(PHRASES):
    path=DEST/f'voice_{i:02d}.wav'
    with wave.open(str(path),'rb') as f:
        assert (f.getnchannels(),f.getsampwidth(),f.getframerate())==(1,2,16000)
        data=struct.unpack(f'<{f.getnframes()}h',f.readframes(f.getnframes()))
    assert 1000<len(data)<=128000
    packed,predictor,step=encode(data)
    rows.append(f'    {{{len(blob)}, {len(packed)}, {len(data)}, {predictor}, {step}}},')
    blob.extend(packed)
    clips.append(dict(id=i,text=text,duration_s=round(len(data)/16000,3),sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
outputs={APP/'bean_audio_index.c':('#include "bean_runtime.h"\nconst bean_clip_t bean_clips[BEAN_LINES] = {\n'+'\n'.join(rows)+f'\n}};\nconst size_t bean_audio_bytes = {len(blob)};\n').encode(),DEST/'mouthy_bean_adpcm.bin':bytes(blob),DEST/'manifest.json':(json.dumps(dict(voice='Tingting',rate=205,sample_rate=16000,clips=clips),ensure_ascii=False,indent=2)+'\n').encode()}
for path,data in outputs.items():
    if args.check: assert path.read_bytes()==data,f'Stale asset: {path}'
    else:path.write_bytes(data)
print(f'Mouthy Bean audio: {len(clips)} original Mandarin lines, {len(blob)} bytes PASS')
