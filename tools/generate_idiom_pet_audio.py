#!/usr/bin/env python3
"""Generate and verify offline Mandarin explanations from Idiom Pet's catalog."""
import argparse
import hashlib
import json
import re
import struct
import subprocess
import tempfile
import wave
from pathlib import Path
from minecraft_adpcm_encode import encode

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / 'main/apps/idiom_pet'
AUDIO = ROOT / 'assets/music/idiom_pet'


def explanations():
    source = (APP / 'idiom_pet_catalog.c').read_text().split('ip_questions[IP_QUESTIONS] = {', 1)[1]
    strings = [json.loads(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"', source)]
    assert len(strings) == 24 * 6, 'Update the catalog reader when the question schema changes'
    return [(strings[i], '没关系。正确成语是，' + strings[i] + '。意思是，' +
             strings[i + 2].replace('\n', '')) for i in range(0, len(strings), 6)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--synthesize', action='store_true')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    assert not (args.synthesize and args.check)
    speech = explanations()
    AUDIO.mkdir(parents=True, exist_ok=True)
    if not args.synthesize:
        recorded = json.loads((AUDIO / 'manifest.json').read_text())
        assert len(recorded) == len(speech)
        for i, (idiom, text) in enumerate(speech):
            old = recorded[i]
            assert (old['id'], old['idiom'], old['text'], old['voice'], old['rate']) == (
                i, idiom, text, 'Tingting', 190), 'Narration changed: rerun with --synthesize'
    if args.synthesize:
        with tempfile.TemporaryDirectory(prefix='idiom-pet-tts-') as temp:
            for i, (_, text) in enumerate(speech):
                aiff = Path(temp) / f'{i:02d}.aiff'
                subprocess.run(['say', '-v', 'Tingting', '-r', '190', '-o', str(aiff), text], check=True)
                subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(aiff),
                                '-ar', '16000', '-ac', '1', '-c:a', 'pcm_s16le',
                                str(AUDIO / f'explain_{i:02d}.wav')], check=True)
    packed, entries, manifest = bytearray(), [], []
    for i, (idiom, text) in enumerate(speech):
        path = AUDIO / f'explain_{i:02d}.wav'
        with wave.open(str(path), 'rb') as wav:
            assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (1, 2, 16000)
            n = wav.getnframes()
            assert 0 < n <= 16000 * 30
            pcm = wav.readframes(n)
        samples = struct.unpack(f'<{n}h', pcm)
        assert max(abs(v) for v in samples) > 100, f'Silent explanation: {idiom}'
        blob, predictor, step = encode(samples)
        entries.append(f'    {{{len(packed)}, {len(blob)}, {n}, {predictor}, {step}}},')
        packed.extend(blob)
        manifest.append(dict(id=i, idiom=idiom, text=text, voice='Tingting', rate=190,
                             samples=n, sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    index = '#include "idiom_pet_audio.h"\n\nconst ip_audio_clip_t ip_audio_clips[IP_QUESTIONS] = {\n'
    index += '\n'.join(entries) + f'\n}};\nconst size_t ip_audio_size = {len(packed)};\n'
    outputs = {
        APP / 'idiom_pet_audio_index.c': index.encode(),
        AUDIO / 'idiom_pet_adpcm.bin': bytes(packed),
        AUDIO / 'manifest.json': (json.dumps(manifest, ensure_ascii=False, indent=2) + '\n').encode(),
    }
    for path, content in outputs.items():
        if args.check:
            assert path.read_bytes() == content, f'Stale audio asset: {path}'
        else:
            path.write_bytes(content)
    print(f'Idiom Pet: {len(speech)} Mandarin explanations; {len(packed)} ADPCM bytes; assets PASS')


if __name__ == '__main__':
    main()
