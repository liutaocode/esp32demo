#!/usr/bin/env python3
"""Build Word Sprite's original word catalog, offline TTS and Chinese font."""
import argparse
import csv
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
APP = ROOT / "main/apps/word_sprite"
AUDIO = ROOT / "assets/music/word_sprite"
PROMPTS = [
    ("welcome", "听单词，选中文。一起唤醒单词精灵！"),
    ("correct", "答对了！跟着读一遍吧。"),
    ("retry", "没关系，听一听，再记一次。"),
    ("finish", "挑战完成！让眼睛休息一下吧。"),
    ("review", "一起找回这些单词吧！"),
]

def rows():
    with (APP / "words.csv").open(encoding="utf-8", newline="") as f:
        data = list(csv.DictReader(f))
    assert len(data) == 36 and len({r["id"] for r in data}) == 36
    for i, r in enumerate(data):
        assert int(r["world"]) == i // 12
        assert re.fullmatch(r"[a-z]+", r["id"]) and r["english"] == r["id"]
        assert r["chinese"] and len(r["chinese"]) <= 4
    return data

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--synthesize", action="store_true")
    p.add_argument("--synthesize-chinese", action="store_true",
                   help="generate only the 36 Mandarin word-meaning clips")
    p.add_argument("--font", action="store_true")
    p.add_argument("--check", action="store_true")
    args = p.parse_args()
    data = rows()
    speech = [(r["id"], r["english"], "Samantha", 140) for r in data]
    speech += [(name, text, "Tingting", 175) for name, text in PROMPTS]
    speech += [("meaning_" + r["id"], "这个单词的意思是：" + r["chinese"] + "。",
                "Tingting", 175) for r in data]
    AUDIO.mkdir(parents=True, exist_ok=True)
    if args.synthesize or args.synthesize_chinese:
        with tempfile.TemporaryDirectory(prefix="word-sprite-tts-") as t:
            for name, text, voice, rate in speech:
                if not args.synthesize and not name.startswith("meaning_"):
                    continue
                aiff = Path(t) / (name + ".aiff")
                subprocess.run(["say", "-v", voice, "-r", str(rate), "-o", str(aiff), text], check=True)
                subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(aiff),
                                "-ar", "16000", "-ac", "1", "-c:a", "pcm_s16le",
                                str(AUDIO / (name + ".wav"))], check=True)
    packed = bytearray()
    clips = []
    manifest = []
    for name, text, voice, rate in speech:
        path = AUDIO / (name + ".wav")
        with wave.open(str(path), "rb") as wav:
            assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (1, 2, 16000)
            n = wav.getnframes()
            assert 0 < n <= 16000 * 10
            pcm = wav.readframes(n)
        samples = struct.unpack(f"<{n}h", pcm)
        assert max(abs(v) for v in samples) > 100, f"silent clip: {name}"
        blob, predictor, step = encode(samples)
        clips.append(f"    {{{len(packed)}, {len(blob)}, {n}, {predictor}, {step}}},")
        packed.extend(blob)
        manifest.append(dict(id=name, text=text, voice=voice, rate=rate,
                             samples=n, sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    catalog = '#include "word_sprite_catalog.h"\n\nconst ws_word_t ws_words[WS_WORDS] = {\n'
    for r in data:
        catalog += "    {%s, %s},\n" % (json.dumps(r["english"]), json.dumps(r["chinese"], ensure_ascii=False))
    catalog += "};\n"
    index = '#include "word_sprite_audio.h"\n\nconst ws_clip_t ws_clips[WS_AUDIO_COUNT] = {\n'
    index += "\n".join(clips) + f"\n}};\nconst size_t ws_audio_size = {len(packed)};\n"
    outputs = {
        APP / "word_sprite_catalog.c": catalog.encode(),
        APP / "word_sprite_audio_index.c": index.encode(),
        AUDIO / "word_sprite_adpcm.bin": bytes(packed),
        AUDIO / "manifest.json": (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode(),
    }
    for path, content in outputs.items():
        if args.check:
            assert path.read_bytes() == content, f"stale generated asset: {path}"
        else:
            path.write_bytes(content)
    if args.font:
        source = (APP / "word_sprite.c").read_text() + catalog
        symbols = "".join(sorted({ch for ch in source if ord(ch) > 127}))
        subprocess.run([
            "npx", "--yes", "lv_font_conv@1.5.3", "--size", "16", "--bpp", "2",
            "--format", "lvgl", "--font",
            str(ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf"),
            "--symbols", symbols, "--range", "0x20-0x7e", "--no-compress", "--no-kerning",
            "--lv-include", "lvgl.h", "--lv-font-name", "word_sprite_zh_16",
            "-o", str(ROOT / "assets/fonts/word_sprite_zh_16.c")], check=True)
    print(f"Word Sprite: {len(data)} words, {len(speech)} TTS clips, {len(packed)} ADPCM bytes; assets OK")

if __name__ == "__main__":
    main()
