#!/usr/bin/env python3
"""Generate and verify Vibe Check's offline Mandarin narration pack."""

import argparse
import hashlib
import json
import struct
import subprocess
import tempfile
import wave
from pathlib import Path

from minecraft_adpcm_encode import encode

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "main/apps/vibe_check"
AUDIO = ROOT / "assets/music/vibe_check"
VOICE = "Tingting"
RATE = 190

SPEECH = [
    ("welcome", "欢迎来到气场测试。五次选择，解锁你的隐藏人格。按确定键开始。"),
    ("question_1", "第一题，你现在的电量？按上键选择窝着充电，按下键选择能量满格。"),
    ("question_2", "第二题，突然多出一小时？按上键选择出门探索，按下键选择动手创造。"),
    ("question_3", "第三题，剧情突然反转？按上键选择静观其变，按下键选择冲上去玩。"),
    ("question_4", "第四题，群聊里你通常？按上键选择安静潜水，按下键选择带头开聊。"),
    ("question_5", "第五题，选一个增益？按上键选择好运加成，按下键选择专注加成。"),
    ("result_1", "你的今日人格是，安静预言家。总能看见别人忽略的细节。"),
    ("result_2", "你的今日人格是，幸运探路者。总能在墙上找到一扇门。"),
    ("result_3", "你的今日人格是，夜行建造师。把脑洞一步步变成现实。"),
    ("result_4", "你的今日人格是，梦境黑客。把古怪想法变成现实。"),
    ("result_5", "你的今日人格是，治愈魔法师。柔软也是一种超能力。"),
    ("result_6", "你的今日人格是，天选主角。今天自带主角光环。"),
    ("result_7", "你的今日人格是，混沌精灵。规则只是温柔的建议。"),
    ("result_8", "你的今日人格是，涡轮英雄。今天一路全速前进。"),
]


def synthesize() -> None:
    AUDIO.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="vibe-check-tts-") as temp:
        for name, text in SPEECH:
            aiff = Path(temp) / f"{name}.aiff"
            subprocess.run(
                ["say", "-v", VOICE, "-r", str(RATE), "-o", str(aiff), text],
                check=True,
            )
            subprocess.run(
                ["ffmpeg", "-v", "error", "-y", "-i", str(aiff),
                 "-ar", "16000", "-ac", "1", "-c:a", "pcm_s16le",
                 str(AUDIO / f"{name}.wav")],
                check=True,
            )


def build_outputs() -> dict[Path, bytes]:
    packed = bytearray()
    clips = []
    manifest = []
    for name, text in SPEECH:
        path = AUDIO / f"{name}.wav"
        with wave.open(str(path), "rb") as wav_file:
            assert (wav_file.getnchannels(), wav_file.getsampwidth(),
                    wav_file.getframerate()) == (1, 2, 16000)
            samples_count = wav_file.getnframes()
            assert 0 < samples_count <= 16000 * 15
            pcm = wav_file.readframes(samples_count)
        samples = struct.unpack(f"<{samples_count}h", pcm)
        assert max(abs(value) for value in samples) > 100, f"silent clip: {name}"
        blob, predictor, step = encode(samples)
        clips.append(
            f"    {{{len(packed)}, {len(blob)}, {samples_count}, "
            f"{predictor}, {step}}},"
        )
        packed.extend(blob)
        manifest.append({
            "id": name,
            "text": text,
            "voice": VOICE,
            "rate": RATE,
            "samples": samples_count,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        })

    index = (
        '#include "vibe_check_audio.h"\n\n'
        "const vibe_check_audio_clip_t "
        "vibe_check_audio_clips[VC_AUDIO_COUNT] = {\n"
        + "\n".join(clips)
        + f"\n}};\nconst size_t vibe_check_audio_size = {len(packed)};\n"
    )
    return {
        APP / "vibe_check_audio_index.c": index.encode(),
        AUDIO / "vibe_check_adpcm.bin": bytes(packed),
        AUDIO / "manifest.json":
            (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode(),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--synthesize", action="store_true")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    assert not (args.synthesize and args.check)
    if args.synthesize:
        synthesize()
    outputs = build_outputs()
    for path, content in outputs.items():
        if args.check:
            assert path.read_bytes() == content, f"stale generated asset: {path}"
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
    print(
        f"Vibe Check: {len(SPEECH)} Mandarin TTS clips, "
        f"{len(outputs[AUDIO / 'vibe_check_adpcm.bin'])} ADPCM bytes; assets PASS"
    )


if __name__ == "__main__":
    main()
