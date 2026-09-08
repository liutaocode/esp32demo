#!/usr/bin/env python3
"""Prepare random cat/toy sound banks and an original dance groove as IMA ADPCM."""
from pathlib import Path
from array import array
import argparse,hashlib,json,math,random,re,subprocess,sys,wave
ROOT=Path(__file__).resolve().parents[2];DEST=ROOT/'assets/music/cat'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');args=p.parse_args()
if args.check:
    info=json.loads((DEST/'audio-manifest.json').read_text())
    for name,sha in info['sha256'].items():assert hashlib.sha256((DEST/name).read_bytes()).hexdigest()==sha,name
    for clip in info['clips']:
        with wave.open(str(DEST/(clip['name']+'.wav')),'rb') as w:
            assert w.getframerate()==16000 and w.getnchannels()==1 and w.getsampwidth()==2
            a=array('h',w.readframes(w.getnframes()))
        assert a[0]==0 and a[-1]==0 and max(map(abs,a))<=22000
        rms=math.sqrt(sum(x*x for x in a)/len(a));assert rms>=1500
        assert clip['adpcm_snr_db']>10
    assert len(info['clips'])==10 and len({c['pcm_sha256'] for c in info['clips']})==10
    print('Audio: 3 calls + 3 purrs + 3 toy cues + original dance; unique PCM, amplitude, ADPCM quality and hashes PASS')
    sys.exit(0)
RATE=16000; PI=math.pi;clips=[]
def finish(name,group,a,level=5400):
    mean=sum(a)/len(a);a=[x-mean for x in a];rms=math.sqrt(sum(x*x for x in a)/len(a));gain=level/max(1e-9,rms)
    values=[round(math.tanh(x*gain/20000)*20000*max(0,min(1,i/640,(len(a)-1-i)/1200))) for i,x in enumerate(a)]
    if len(values)%2:values.append(0)
    clips.append((name,group,values))
def recording(source,start,duration,speed=1):
    raw=subprocess.check_output(['ffmpeg','-hide_banner','-loglevel','error','-ss',str(start),'-i',str(DEST/f'source-{source}.mp3'),'-t',str(duration),'-af',f'highpass=f=90,lowpass=f=4500,equalizer=f=600:t=q:w=0.8:g=4,aresample=16000,asetrate={round(16000*speed)},aresample=16000','-ac','1','-ar','16000','-f','s16le','-'])
    a=array('h',raw)
    if sys.byteorder!='little':a.byteswap()
    return list(a)
for i,speed in enumerate((1.0,0.88,1.16)):finish(f'call-{i}',1,recording('meow',0.1,1.9,speed))
for i,start in enumerate((0.4,4.2,6.7)):finish(f'purr-{i}',2,recording('purr',start,3.2))
for kind in range(3):
    a=[];duration=(0.62,0.75,0.85)[kind]
    for i in range(round(RATE*duration)):
        t=i/RATE
        if kind==0:  # rubber spring
            phase=2*PI*(700*t-270*t*t+12*math.sin(2*PI*7*t));v=math.sin(phase)*math.exp(-4*t)
        elif kind==1:  # two soft bubbles
            u=t if t<0.32 else t-0.32;v=math.sin(2*PI*(950*u-820*u*u))*math.exp(-12*u)
        else:  # small toy bell
            v=(math.sin(2*PI*1046*t)+0.38*math.sin(2*PI*1568*t))*math.exp(-5*t)
        a.append(v)
    finish(f'toy-{kind}',3,a,4800)
noise=random.Random(73);a=[];bass=[130.81,130.81,155.56,116.54,130.81,174.61,155.56,116.54]
melody=[523.25,0,622.25,783.99,0,622.25,523.25,466.16,523.25,0,783.99,932.33,783.99,622.25,466.16,523.25]
for i in range(RATE*8):
    t=i/RATE;beat=int(t/0.5);u=t%0.5;step=int(t/0.25);h=t%0.25
    kick=0.8*math.sin(2*PI*(53*u+8*(1-math.exp(-35*u))))*math.exp(-18*u)
    snare=0.24*(noise.random()*2-1)*math.exp(-32*u) if beat%2 else 0
    hat=0.075*(noise.random()*2-1)*math.exp(-90*h)
    f=bass[(beat//2)%8];b=(math.sin(2*PI*f*t)+0.22*math.sin(2*PI*3*f*t))*0.23*math.exp(-3*u)
    note=melody[step%16];lead=0
    if note:lead=0.16*(math.sin(2*PI*note*t)+0.23*math.sin(2*PI*2*note*t))*math.sin(PI*min(1,h/0.25))
    a.append(kick+snare+hat+b+lead)
finish('dance',4,a,5900)
text=(ROOT/'main/minecraft_adpcm.c').read_text();steps=list(map(int,re.findall(r'\d+',re.search(r'steps\[89\] = \{(.*?)\}',text,re.S)[1])));changes=[-1,-1,-1,-1,2,4,6,8]*2
def encode(a):
    pred=0;index=0;codes=[];decoded=[]
    for x in a:
        step=steps[index];diff=x-pred;code=8 if diff<0 else 0;diff=abs(diff);delta=step>>3
        for bit,value in ((4,step),(2,step>>1),(1,step>>2)):
            if diff>=value:code|=bit;diff-=value;delta+=value
        pred=max(-32768,min(32767,pred+(-delta if code&8 else delta)));index=max(0,min(88,index+changes[code]));codes.append(code);decoded.append(pred)
    packed=bytes((codes[i]<<4)|codes[i+1] for i in range(0,len(codes),2))
    snr=10*math.log10(max(1,sum(x*x for x in a))/max(1,sum((x-y)**2 for x,y in zip(a,decoded))))
    return packed,snr
header=['/* Cat recordings: CC0 Joseph SARDIN. Toy cues and dance: original. */','#include "cat_clips.h"']
info=[]
for idx,(name,group,values) in enumerate(clips):
    packed,snr=encode(values);header.append(f'static const uint8_t clip_{idx}[] = {{')
    header.extend('    '+','.join(f'0x{x:02x}' for x in packed[i:i+24])+',' for i in range(0,len(packed),24));header.append('};')
    out=array('h',values)
    if sys.byteorder!='little':out.byteswap()
    pcm=out.tobytes()
    with wave.open(str(DEST/f'{name}.wav'),'wb') as w:w.setparams((1,2,RATE,len(values),'NONE','not compressed'));w.writeframes(pcm)
    info.append({'name':name,'group':group,'samples':len(values),'bytes':len(packed),'adpcm_snr_db':round(snr,2),'pcm_sha256':hashlib.sha256(pcm).hexdigest()})
header.append('const cat_clip_t cat_clips[10] = {')
for idx,(_,group,values) in enumerate(clips):header.append(f'    {{clip_{idx}, {len(values)}, {group}}},')
header.append('};');(DEST/'cat_clips.c').write_text('\n'.join(header)+'\n')
old=DEST/'cat_pcm.c'
if old.exists():old.unlink()
manifest={'recording_license':'CC0-1.0','recording_author':'Joseph SARDIN','source_pages':{'purr':'https://bigsoundbank.com/cat-purr-s0436.html','meow':'https://bigsoundbank.com/miaulement-chat-2-s1890.html'},'original_sounds':'Three toy cues and a 120 BPM dance groove created for this application','format':'16 kHz mono IMA ADPCM; predictor/index zero, high nibble first; fresh decoder per loop','clips':info,'sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(DEST.iterdir()) if p.suffix in ('.mp3','.wav','.c')}}
(DEST/'audio-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Generated',len(clips),'clips,',sum(x['bytes'] for x in info),'compressed bytes; minimum SNR',min(x['adpcm_snr_db'] for x in info),'dB')
