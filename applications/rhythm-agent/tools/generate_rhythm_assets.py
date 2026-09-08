#!/usr/bin/env python3
"""Build small Mandarin cues locally, pack ADPCM, and verify shipped assets."""
from pathlib import Path
import argparse, hashlib, json, struct, subprocess, tempfile, wave
from minecraft_adpcm_encode import encode
ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/'assets/music/rhythm_agent'
PHRASES=['听好暗号','轮到你了','破解成功','再试一次']
p=argparse.ArgumentParser();p.add_argument('--synthesize',action='store_true');p.add_argument('--check',action='store_true');args=p.parse_args()
if args.synthesize:
    with tempfile.TemporaryDirectory(prefix='rhythm-tts-') as tmp:
        for i,phrase in enumerate(PHRASES):
            aiff=Path(tmp)/'voice.aiff';wav=Path(tmp)/'voice.wav'
            subprocess.run(['say','-v','Tingting','-r','225','-o',str(aiff),phrase],check=True)
            subprocess.run(['ffmpeg','-v','error','-y','-i',str(aiff),'-ar','16000','-ac','1','-c:a','pcm_s16le',str(wav)],check=True)
            with wave.open(str(wav),'rb') as f:
                n=f.getnframes();samples=struct.unpack(f'<{n}h',f.readframes(n))
            audible=[j for j,x in enumerate(samples) if abs(x)>140];assert audible
            samples=samples[max(0,audible[0]-100):audible[-1]+500]
            peak=max(map(abs,samples));samples=tuple(round(x*17000/peak) for x in samples)
            with wave.open(str(DEST/f'voice_{i}.wav'),'wb') as f:
                f.setparams((1,2,16000,0,'NONE','not compressed'));f.writeframes(struct.pack(f'<{len(samples)}h',*samples))
blob=bytearray();rows=[];manifest=[]
for i,phrase in enumerate(PHRASES):
    path=DEST/f'voice_{i}.wav'
    with wave.open(str(path),'rb') as f:
        assert (f.getnchannels(),f.getsampwidth(),f.getframerate())==(1,2,16000)
        n=f.getnframes();pcm=f.readframes(n);samples=struct.unpack(f'<{n}h',pcm)
    data,predictor,step=encode(samples)
    assert 0<n<=64000 and len(data)==n//2
    rows.append(f'    {{{len(blob)}, {len(data)}, {n}, {predictor}, {step}}},')
    blob.extend(data);manifest.append({'id':i,'text':phrase,'seconds':round(n/16000,3),'wav_sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
outputs={DEST/'rhythm_voice.bin':bytes(blob),ROOT/'main/apps/rhythm_agent/rhythm_voice_index.c':('#include "rhythm_audio.h"\nconst ra_clip_t ra_clips[4] = {\n'+'\n'.join(rows)+f'\n}};\nconst size_t ra_voice_bytes = {len(blob)};\n').encode(),DEST/'manifest.json':(json.dumps({'voice':'macOS Tingting','rate':225,'sample_rate':16000,'cues':manifest},ensure_ascii=False,indent=2)+'\n').encode()}
for path,data in outputs.items():
    if args.check: assert path.read_bytes()==data,f'Stale asset: {path}'
    else: path.write_bytes(data)
print(f'Rhythm audio: four Mandarin cues, {len(blob)} ADPCM bytes, PASS')
