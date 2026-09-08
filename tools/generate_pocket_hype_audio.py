#!/usr/bin/env python3
"""Build original effects and Mandarin TTS; verify existing assets offline."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import random
import re
import struct
import subprocess
import tempfile
import wave
from minecraft_adpcm_encode import encode
ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / 'main/apps/pocket_hype'
DEST = ROOT / 'assets/music/pocket_hype'
RATE = 16000
VOICE = 'Tingting'
text = (APP / 'pocket_hype_state.c').read_text().split('ph_lines[PH_SCENES][3] = {', 1)[1].split('\n};', 1)[0]
PHRASES = re.findall(r'"([^"]+)"', text)
assert len(PHRASES) == 18

def write_wav(path, values):
    with wave.open(str(path), 'wb') as f:
        f.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
        f.writeframes(struct.pack(f'<{len(values)}h', *values))

def read_wav(path):
    with wave.open(str(path), 'rb') as f:
        assert (f.getnchannels(), f.getsampwidth(), f.getframerate()) == (1, 2, RATE)
        n = f.getnframes()
        return struct.unpack(f'<{n}h', f.readframes(n))

def effects(scene, tier):
    rng = random.Random(1300 + scene * 3 + tier)
    out = [0.0] * int(RATE * (1.1 + tier * .28))
    def note(start, dur, hz, gain=.22, end=None):
        for i in range(int(dur * RATE)):
            j = int(start * RATE) + i
            if j >= len(out): break
            t = i / RATE
            env = min(1.0, t / .008) * math.exp(-3.0 * t / dur)
            phase = 2 * math.pi * (hz * t + ((end or hz) - hz) * t * t / (2 * dur))
            out[j] += gain * env * (math.sin(phase) + .22 * math.sin(phase * 2))
    def clap(start, gain):
        for i in range(int(.11 * RATE)):
            j = int(start * RATE) + i
            if j >= len(out): break
            t = i / RATE
            env = math.exp(-t * 50) * min(1, t / .002)
            out[j] += gain * env * rng.uniform(-1, 1)
    if scene == 0:
        for _ in range(14 + tier * 16): clap(rng.uniform(.015, len(out)/RATE - .14), .45)
        note(.05, .16, 880, .1); note(.24, .17, 1174, .13)
    elif scene == 1:
        for i, hz in enumerate([392, 523, 659, 784]): note(.03+i*.19, .30, hz, .3)
        if tier: note(.86, .5, 1046, .26)
    elif scene == 2:
        for i in range(8 + tier*3): clap(.02+i*.055, .25+i*.017)
        note(.05, .7, 196, .14, 392); note(.75, .32, 880, .24)
    elif scene == 3:
        for i in range(3): note(.04+i*.23, .19, 670-i*150, .26, 500-i*150)
        if tier: note(.85, .26, 440, .2, 880)
    elif scene == 4:
        for i, hz in enumerate([523, 659, 784, 1046]): note(.02+i*.13, .30, hz, .28)
        note(.69, .42, 1046, .22); note(.69, .42, 659, .12)
    else:
        for i, hz in enumerate([523, 659, 784]): note(.02+i*.24, .55, hz, .19)
    # Gentle fades; no borrowed music, meme recordings or recognisable melody.
    for i in range(len(out)):
        out[i] *= min(1, i/160, (len(out)-1-i)/400)
    return out

def synthesize():
    DEST.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='pocket-hype-tts-') as tmp:
        for i, phrase in enumerate(PHRASES):
            aiff = Path(tmp) / 'voice.aiff'
            wav = Path(tmp) / 'voice.wav'
            subprocess.run(['say', '-v', VOICE, '-r', '225', '-o', str(aiff), phrase], check=True)
            subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(aiff), '-ar', str(RATE), '-ac', '1', '-c:a', 'pcm_s16le', str(wav)], check=True)
            speech = read_wav(wav)
            audible = [j for j, value in enumerate(speech) if abs(value) > 140]
            assert audible, phrase
            speech = speech[max(0, audible[0]-160):min(len(speech), audible[-1]+1200)]
            write_wav(DEST / f'voice_{i:02d}.wav', speech)

def outputs():
    blob = bytearray(); entries = []; manifest = []; files = {}
    for voice_on in (True, False):
        for i, phrase in enumerate(PHRASES):
            fx = effects(i//3, i%3)
            if voice_on:
                speech = read_wav(DEST / f'voice_{i:02d}.wav')
                offset = int(.22 * RATE)
                mix = [0.0] * max(len(fx), len(speech) + offset + 320)
                for j, value in enumerate(fx): mix[j] += value * (.50 if j >= offset else 1)
                peak = max(abs(v) for v in speech)
                for j, value in enumerate(speech): mix[offset+j] += value / peak * .60
            else: mix = fx
            peak = max(abs(v) for v in mix)
            values = tuple(round(v * (.82 / max(1.0, peak)) * 32767) for v in mix)
            assert 0 < len(values) <= RATE * 6
            packed, predictor, step = encode(values)
            entries.append(f'    {{{len(blob)}, {len(packed)}, {len(values)}, {predictor}, {step}}},')
            blob.extend(packed)
            manifest.append({'id': len(manifest), 'scene': i//3, 'tier': i%3+1,
                'phrase': phrase if voice_on else '', 'tts': voice_on,
                'duration_s': round(len(values)/RATE, 3), 'peak': max(abs(v) for v in values),
                'pcm_sha256': hashlib.sha256(struct.pack(f'<{len(values)}h', *values)).hexdigest()})
            # Review previews are generated from exactly the shipped pre-encoding PCM.
            files[DEST / f'cue_{len(manifest)-1:02d}.pcm'] = struct.pack(f'<{len(values)}h', *values)
    index = '#include "pocket_hype_audio.h"\nconst ph_clip_t ph_clips[PH_AUDIO_COUNT] = {\n'+'\n'.join(entries)+f'\n}};\nconst size_t ph_audio_bytes = {len(blob)};\n'
    files[APP / 'pocket_hype_audio_index.c'] = index.encode()
    files[DEST / 'pocket_hype_adpcm.bin'] = bytes(blob)
    files[DEST / 'manifest.json'] = (json.dumps({'voice': VOICE, 'speech_rate': 225, 'sample_rate': RATE, 'clips': manifest}, ensure_ascii=False, indent=2)+'\n').encode()
    return files

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--synthesize', action='store_true')
parser.add_argument('--check', action='store_true')
args = parser.parse_args()
assert not(args.synthesize and args.check)
if args.synthesize: synthesize()
for path, content in outputs().items():
    if args.check: assert path.read_bytes() == content, f'Stale asset: {path}'
    else: path.write_bytes(content)
print('Pocket Hype: 18 Mandarin cues + 18 effects-only cues; audio assets PASS')
