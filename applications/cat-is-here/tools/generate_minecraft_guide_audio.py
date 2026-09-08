#!/usr/bin/env python3
"""Pack the ordered Minecraft guide WAV clips into one IMA-ADPCM C asset."""

from __future__ import annotations

import struct
import wave
from pathlib import Path

from minecraft_adpcm_encode import encode


ROOT = Path(__file__).resolve().parents[1]
INPUT_DIR = ROOT / "assets" / "music" / "minecraft_guide_entries"
OUTPUT = ROOT / "main" / "minecraft_guide_audio.c"
CLIP_FILES = (
    "01_creeper.wav",
    "02_enderman.wav",
    "03_axolotl.wav",
    "04_bee.wav",
    "05_wolf.wav",
    "06_diamond_ore.wav",
    "07_crafting_table.wav",
    "08_nether_portal.wav",
    "09_enchanting_table.wav",
    "10_ender_dragon.wav",
    "11_zombie.wav",
    "12_skeleton.wav",
    "13_spider.wav",
    "14_slime.wav",
    "15_villager.wav",
    "16_iron_golem.wav",
    "17_piglin.wav",
    "18_warden.wav",
    "19_wither.wav",
    "20_elytra.wav",
)


def read_clip(path: Path) -> tuple[bytes, int, int, int]:
    with wave.open(str(path), "rb") as wav:
        if wav.getnchannels() != 1 or wav.getsampwidth() != 2 or wav.getframerate() != 16000:
            raise SystemExit(f"{path}: expected 16 kHz, 16-bit, mono PCM WAV")
        frames = wav.readframes(wav.getnframes())
    samples = struct.unpack(f"<{len(frames) // 2}h", frames)
    data, predictor, index = encode(samples)
    return data, len(samples), predictor, index


def main() -> None:
    clips = [read_clip(INPUT_DIR / name) for name in CLIP_FILES]
    packed = b"".join(clip[0] for clip in clips)

    lines = [
        '#include "minecraft_guide_audio.h"',
        "",
        "const uint8_t minecraft_guide_audio_data[] = {",
    ]
    for offset in range(0, len(packed), 16):
        row = ", ".join(f"0x{byte:02x}" for byte in packed[offset:offset + 16])
        lines.append(f"    {row},")
    lines.extend([
        "};",
        f"const size_t minecraft_guide_audio_data_size = {len(packed)};",
        "",
        "const minecraft_guide_audio_clip_t minecraft_guide_audio_clips[] = {",
    ])
    offset = 0
    for data, sample_count, predictor, index in clips:
        lines.append(
            "    { .offset = %d, .adpcm_size = %d, .sample_count = %d, "
            ".initial_predictor = %d, .initial_step_index = %d },"
            % (offset, len(data), sample_count, predictor, index)
        )
        offset += len(data)
    lines.extend([
        "};",
        f"const size_t minecraft_guide_audio_clip_count = {len(clips)};",
        "",
    ])
    OUTPUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {len(clips)} clips, {len(packed)} ADPCM bytes to {OUTPUT}")


if __name__ == "__main__":
    main()
