#!/usr/bin/env python3
"""Create labelled-by-filename host renders and audition actual ADPCM firmware audio."""
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import wave
from minecraft_adpcm_encode import STEP_TABLE, INDEX_TABLE
ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'projects/pocket-hype/assets'
def run(args, cwd=ROOT):
    subprocess.run([str(a) for a in args], cwd=cwd, check=True)
with tempfile.TemporaryDirectory(prefix='pocket-hype-ui-') as tmp:
    tmp = Path(tmp)
    run(['cmake', '-S', ROOT/'tests/pocket_hype_ui', '-B', tmp/'build'])
    run(['cmake', '--build', tmp/'build', '-j', '8'])
    run([tmp/'build/preview'], cwd=tmp)
    for ppm in sorted(tmp.glob('*.ppm')):
        run(['ffmpeg', '-v', 'error', '-y', '-i', ppm, DEST/f'host-{ppm.stem}.png'])
blob = (ROOT/'assets/music/pocket_hype/pocket_hype_adpcm.bin').read_bytes()
entries = [tuple(map(int,m)) for m in re.findall(r'\{(\d+), (\d+), (\d+), (-?\d+), (\d+)\}', (ROOT/'main/apps/pocket_hype/pocket_hype_audio_index.c').read_text())]
preview = []
for clip in [2,5,8,11,14,17]:
    offset, size, count, predictor, index = entries[clip]
    assert offset+size <= len(blob)
    pcm = [predictor]
    for i in range(count-1):
        b = blob[offset+i//2]; code = b >> 4 if i & 1 else b & 15
        step = STEP_TABLE[index]
        delta = (step >> 3) + (step if code & 4 else 0) + (step >> 1 if code & 2 else 0) + (step >> 2 if code & 1 else 0)
        predictor = max(-32768,min(32767,predictor+(-delta if code & 8 else delta)))
        index = max(0,min(88,index+INDEX_TABLE[code])); pcm.append(predictor)
    preview.extend(pcm); preview.extend([0]*8000)
with wave.open(str(DEST/'audio-demo.wav'),'wb') as f:
    f.setparams((1,2,16000,0,'NONE','not compressed'))
    f.writeframes(struct.pack(f'<{len(preview)}h',*preview))
print('Pocket Hype host renders and decoded firmware audio preview ready; no device capture performed.')
