#!/usr/bin/env python3
"""Generate Grassland Lab's catalog, Mandarin speech pack and subset font."""
import argparse
import csv
import hashlib
import json
import struct
import subprocess
import tempfile
import wave
from pathlib import Path
from minecraft_adpcm_encode import encode
ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / 'main/apps/pvz_almanac'
AUDIO = ROOT / 'assets/music/pvz_almanac'

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--synthesize', action='store_true')
    p.add_argument('--font', action='store_true')
    p.add_argument('--check', action='store_true')
    args = p.parse_args()
    with (APP / 'catalog.csv').open(encoding='utf-8', newline='') as f:
        rows = list(csv.DictReader(f))
    assert len(rows) == 24 and len({r['id'] for r in rows}) == 24
    for i, r in enumerate(rows):
        assert int(r['kind']) == (i >= 16)
        assert len(r['name']) <= 6 and len(r['ability']) <= 24 and len(r['tip']) <= 24
        assert r['name'] not in r['clue']
    speech = [(r['id'], r['name'] + '。' + r['ability'] + r['tip']) for r in rows]
    speech += [(r['id'] + '_clue', r['clue']) for r in rows]
    AUDIO.mkdir(parents=True, exist_ok=True)
    if args.synthesize:
        manifest_path = AUDIO / 'manifest.json'
        previous = {r['id']: r for r in json.loads(manifest_path.read_text())} if manifest_path.exists() else {}
        with tempfile.TemporaryDirectory(prefix='pvz-tts-') as t:
            for name, text in speech:
                old = previous.get(name, {})
                wav_path = AUDIO / (name + '.wav')
                if (old.get('text') == text and old.get('voice') == 'Tingting' and old.get('rate') == 210
                    and wav_path.exists() and hashlib.sha256(wav_path.read_bytes()).hexdigest() == old.get('sha256')):
                    continue
                aiff = Path(t) / (name + '.aiff')
                subprocess.run(['say', '-v', 'Tingting', '-r', '210', '-o', str(aiff), text], check=True)
                subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(aiff),
                    '-ar', '16000', '-ac', '1', '-c:a', 'pcm_s16le', str(AUDIO / (name + '.wav'))], check=True)
    packed, clips, manifest = bytearray(), [], []
    for name, text in speech:
        path = AUDIO / (name + '.wav')
        with wave.open(str(path), 'rb') as w:
            assert (w.getnchannels(), w.getsampwidth(), w.getframerate()) == (1, 2, 16000)
            n = w.getnframes()
            assert 0 < n < 16000 * 16
            pcm = w.readframes(n)
        samples = struct.unpack(f'<{n}h', pcm)
        assert max(abs(v) for v in samples) > 100, f'silent clip: {name}'
        blob, predictor, step = encode(samples)
        clips.append(f'    {{{len(packed)}, {len(blob)}, {n}, {predictor}, {step}}},')
        packed.extend(blob)
        manifest.append(dict(id=name, text=text, voice='Tingting', rate=210,
            samples=n, sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    catalog = '#include "pvz_catalog.h"\nconst pvz_entry_t pvz_catalog[PVZ_COUNT] = {\n'
    for r in rows:
        strings = [json.dumps(r[k], ensure_ascii=False) for k in ('id','name','role','ability','tip','clue')]
        catalog += '    {' + ', '.join(strings) + f', {r["cost"]}, {r["kind"]}, 0x{r["color"]}' + '},\n'
    catalog += '};\n'
    index = '#include "pvz_audio.h"\nconst pvz_clip_t pvz_clips[PVZ_AUDIO_COUNT] = {\n'
    index += '\n'.join(clips) + f'\n}};\nconst size_t pvz_audio_size = {len(packed)};\n'
    outputs = {APP / 'pvz_catalog.c': catalog.encode(), APP / 'pvz_audio_index.c': index.encode(),
        AUDIO / 'pvz_adpcm.bin': bytes(packed),
        AUDIO / 'manifest.json': (json.dumps(manifest, ensure_ascii=False, indent=2) + '\n').encode()}
    for path, data in outputs.items():
        if args.check: assert path.read_bytes() == data, f'stale asset: {path}'
        else: path.write_bytes(data)
    if args.font:
        source = catalog + ''.join(f.read_text() for f in APP.glob('*.c') if 'index' not in f.name)
        symbols = ''.join(sorted({c for c in source if ord(c) > 127}))
        font = ROOT / 'managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf'
        for size in (12, 16, 20):
            output = ROOT / f'assets/fonts/pvz_zh_{size}.c'
            subprocess.run(['npx', '--yes', 'lv_font_conv@1.5.3', '--size', str(size), '--bpp', '4',
                '--format', 'lvgl', '--font', str(font), '--symbols', symbols, '--range', '0x20-0x7e',
                '--no-compress', '--no-kerning', '--lv-include', 'lvgl.h', '--lv-font-name', f'pvz_zh_{size}',
                '-o', str(output)], check=True)
            output.write_text(output.read_text().replace(str(ROOT) + '/', ''))
    print(f'Grassland Lab: {len(rows)} cards, {len(speech)} speech clips, {len(packed)} ADPCM bytes; assets OK')
if __name__ == '__main__': main()
