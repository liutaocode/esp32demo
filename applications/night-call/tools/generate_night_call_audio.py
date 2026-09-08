#!/usr/bin/env python3
"""Build offline Mandarin dialogue with macOS Tingting and bounded IMA ADPCM."""
import argparse,hashlib,json,struct,subprocess,tempfile,wave
from pathlib import Path
from minecraft_adpcm_encode import encode
R=Path(__file__).resolve().parents[1]; A=R/'main/apps/night_call'; D=R/'assets/music/night_call'
RATE=16000
texts=json.loads((A/'voice_text.json').read_text())
p=argparse.ArgumentParser(); p.add_argument('--synthesize',action='store_true'); p.add_argument('--check',action='store_true'); args=p.parse_args()
D.mkdir(parents=True,exist_ok=True)
if args.synthesize:
    with tempfile.TemporaryDirectory(prefix='night-call-tts-') as t:
        for i,txt in enumerate(texts):
            ai=Path(t)/'line.aiff'; wav=Path(t)/'line.wav'
            subprocess.run(['say','-v','Tingting','-r','240','-o',str(ai),txt],check=True)
            subprocess.run(['ffmpeg','-v','error','-y','-i',str(ai),'-ar',str(RATE),'-ac','1','-c:a','pcm_s16le',str(wav)],check=True)
            with wave.open(str(wav),'rb') as f:
                samples=struct.unpack('<'+str(f.getnframes())+'h',f.readframes(f.getnframes()))
            active=[i for i,v in enumerate(samples) if abs(v)>140]; assert active
            samples=samples[max(0,active[0]-200):min(len(samples),active[-1]+1200)]
            peak=max(abs(v) for v in samples)
            samples=[round(v*24500/peak) for v in samples]
            # Short fades protect starts/ends when a new choice interrupts a call.
            for j in range(min(160,len(samples)//2)):
                samples[j]=round(samples[j]*j/160); samples[-1-j]=round(samples[-1-j]*j/160)
            with wave.open(str(D/f'line_{i:02d}.wav'),'wb') as f:
                f.setparams((1,2,RATE,0,'NONE','not compressed')); f.writeframes(struct.pack('<'+str(len(samples))+'h',*samples))
            print(f'voice {i+1}/{len(texts)}',flush=True)
blob=bytearray(); rows=[]; clips=[]
for i,txt in enumerate(texts):
    fpath=D/f'line_{i:02d}.wav'
    with wave.open(str(fpath),'rb') as f:
        assert (f.getframerate(),f.getnchannels(),f.getsampwidth())==(RATE,1,2)
        pcm=f.readframes(f.getnframes()); samples=struct.unpack('<'+str(len(pcm)//2)+'h',pcm)
    assert 0<len(samples)<=RATE*18
    data,pred,step=encode(samples)
    rows.append(f'    {{{len(blob)}, {len(data)}, {len(samples)}, {pred}, {step}}},')
    clips.append(dict(id=i,text=txt,seconds=round(len(samples)/RATE,3),wav_sha256=hashlib.sha256(fpath.read_bytes()).hexdigest()))
    blob.extend(data)
outputs={A/'night_call_audio_index.c':('#include "night_call_audio.h"\nconst nc_clip_t nc_clips[NC_AUDIO_COUNT] = {\n'+'\n'.join(rows)+'\n};\nconst size_t nc_audio_bytes = '+str(len(blob))+';\n').encode(),D/'night_call_adpcm.bin':bytes(blob),D/'manifest.json':(json.dumps(dict(voice='macOS Tingting',rate=240,sample_rate=RATE,clips=clips),ensure_ascii=False,indent=2)+'\n').encode()}
for f,data in outputs.items():
    if args.check: assert f.read_bytes()==data, f'Stale audio: {f}'
    else: f.write_bytes(data)
print(f'Audio pack: {len(blob)} bytes, {sum(c["seconds"] for c in clips):.1f}s')
