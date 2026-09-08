#!/usr/bin/env python3
"""Build five recorded ringtone variants and real phone vibration for Flash PCM."""
from pathlib import Path
import argparse,array,hashlib,json,math,subprocess,sys,wave
ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/'assets/music/excuse_call'
RATE=16000
SAMPLES=72000
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check',action='store_true')
args=parser.parse_args()
manifest=ASSETS/'audio-manifest.json'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
if args.check:
    m=json.loads(manifest.read_text())
    for item in m['files']:
        p=ROOT/item['path']; assert sha(p)==item['sha256'],str(p)
    assert m['samples_per_ring']==SAMPLES and m['rings']==5
    print('Excuse Call recorded audio: source and PCM hashes, five variants PASS')
    raise SystemExit

def decode(name):
    raw=subprocess.check_output(['ffmpeg','-v','error','-i',str(ASSETS/'source'/name),'-ac','1','-ar',str(RATE),'-af','highpass=f=70','-f','s16le','pipe:1'])
    a=array.array('h');a.frombytes(raw)
    if sys.byteorder!='little':a.byteswap()
    # Remove only leading/trailing near-silence; retain the recorded envelope.
    active=[i for i,v in enumerate(a) if abs(v)>80]
    return list(a[max(0,active[0]-160):min(len(a),active[-1]+480)])
def normalize(a,peak):
    scale=peak/max(abs(v) for v in a)
    return [v*scale for v in a]
def resample(a,ratio):
    n=int((len(a)-1)/ratio)
    return [a[int(i*ratio)]*(1-i*ratio%1)+a[int(i*ratio)+1]*(i*ratio%1) for i in range(n)]
melody=decode('marimba.mp3');motor=normalize(decode('vibration.mp3'),18500)
banks=[]
for ring,ratio in enumerate([1,2**(2/12),2**(-2/12),1,2**(4/12)]):
    music=normalize(resample(melody,ratio),14500 if ring==3 else 18500)
    mixed=[0.0]*SAMPLES
    for i,v in enumerate(music[:SAMPLES]):mixed[i]+=v
    for onset in (0,44800):
        for i,v in enumerate(motor):
            if onset+i<SAMPLES:mixed[onset+i]+=v
    peak=max(abs(v) for v in mixed);scale=min(1,26500/peak)
    pcm=[int(v*scale) for v in mixed]
    # Keep the loop seam silent, preserving natural note/motor decays.
    for i in range(160):pcm[i]=pcm[i]*i//160;pcm[-i-1]=pcm[-i-1]*i//160
    banks.append(pcm)
    p=ASSETS/f'ringtone-{ring+1}.wav'
    with wave.open(str(p),'wb') as f:
        f.setnchannels(1);f.setsampwidth(2);f.setframerate(RATE)
        data=array.array('h',pcm)
        if sys.byteorder!='little':data.byteswap()
        f.writeframes(data.tobytes())
p=ASSETS/'excuse_call_pcm.c'
with p.open('w') as f:
    f.write('/* Generated recorded audio. Credits and licenses: source/SOURCES.md. */\n#include <stdint.h>\nconst int16_t ec_pcm_bank[5][72000] = {\n')
    for bank in banks:
        f.write('{\n')
        for i in range(0,SAMPLES,24):f.write(','.join(str(v) for v in bank[i:i+24])+',\n')
        f.write('},\n')
    f.write('};\n')
files=[ASSETS/'source/marimba.mp3',ASSETS/'source/vibration.mp3',p]+[ASSETS/f'ringtone-{n}.wav' for n in range(1,6)]
manifest.write_text(json.dumps({'rate':RATE,'rings':5,'samples_per_ring':SAMPLES,'files':[{'path':str(p.relative_to(ROOT)),'sha256':sha(p),'bytes':p.stat().st_size} for p in files]},indent=2)+'\n')
print('Recorded audio generated:',5*SAMPLES*2,'PCM bytes')
