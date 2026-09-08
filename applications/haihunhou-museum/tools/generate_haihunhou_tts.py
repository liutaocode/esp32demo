#!/usr/bin/env python3
"""Generate and pack the twenty-two Haihunhou Museum narration clips."""

from __future__ import annotations

import shutil
import struct
import subprocess
import tempfile
import wave
from pathlib import Path

from ima_adpcm_encode import encode


ROOT = Path(__file__).resolve().parents[1]
WAV_DIR = ROOT / "assets/music/haihunhou_tts"
OUTPUT = ROOT / "main/haihunhou_tts_audio.c"

SCRIPTS = (
    ("01_site_story.wav", "欢迎来到南昌汉代海昏侯国遗址博物馆。一座侯国都城、墓园与陵墓，共同保存两千年前的生活。让我们跟随出土器物，走进刘贺的时代。"),
    ("02_site_detail.wav", "遗址从二零一一年开始考古发掘，出土文物一万余件套。博物馆与遗址现场共同讲述海昏侯国。"),
    ("03_liu_he_story.wav", "刘贺的一生几经起落。他先为昌邑王，短暂即位二十七天，后来被封为第一代海昏侯。"),
    ("04_liu_he_detail.wav", "墓中印章、奏牍与器物铭文等证据彼此印证，让考古学家确认墓主身份，并重建他的生活世界。"),
    ("05_hoof_gold_story.wav", "马蹄形与麟趾形金器光泽厚重，名字来自古人想象中的祥瑞动物，也显示汉代贵族的财富。"),
    ("06_hoof_gold_detail.wav", "它们与金饼、金板等一同出土。形制、重量与刻记，为研究西汉黄金制度提供了重要线索。"),
    ("07_gold_cake_story.wav", "一枚枚金饼像缩小的太阳，表面保留铸造、锤击和使用留下的痕迹，是海昏侯墓的醒目发现。"),
    ("08_gold_cake_detail.wav", "考古记录的不只是金光，还包括重量、成色、刻划和出土位置。这些信息帮助理解财富如何被管理。"),
    ("09_goose_lamp_story.wav", "大雁回首衔鱼，鱼身托起灯盘。烟气可沿雁颈进入腹中水面，造型与实用巧妙结合。"),
    ("10_goose_lamp_detail.wav", "这类灯体现古人控制烟尘的构想。观看时可顺着灯罩、鱼身、雁颈和雁腹寻找烟气路径。"),
    ("11_confucius_story.wav", "孔子衣镜的漆木构件上绘有孔子及弟子形象，并写下人物传记。图像、文字和日用器物在这里相遇。"),
    ("12_confucius_detail.wav", "木胎漆器保存不易，出土后需持续保护。它也提醒我们，经典与先贤故事曾进入汉代贵族的日常空间。"),
    ("13_lunyu_story.wav", "写在竹简上的论语保存了今天通行本没有的知道篇，让失落两千年的文字重新进入视野。"),
    ("14_lunyu_detail.wav", "简牍往往残断、字迹漫漶，需要编号、清理、红外成像和缀合释读。每个判断都来自长期协作。"),
    ("15_bells_story.wav", "大小有序的钟组成乐列。它们既能发声，也是身份与礼制的象征，把侯国宴飨和祭祀带回耳边。"),
    ("16_bells_detail.wav", "观察钟体大小、悬挂次序和纹饰，再想象不同音高依次响起。考古发现让西汉礼乐有了具体形状。"),
    ("17_coins_story.wav", "密集的五铢钱曾以绳贯穿、成串存放。锈结的铜钱像时间切片，记录货币流通与财富储藏。"),
    ("18_coins_detail.wav", "考古人员通过数量、重量、版式和位置研究钱币。它们看似相同，细部却藏着铸造与年代信息。"),
    ("19_curator_story.wav", "馆长彭明瀚长期从事江西考古与博物馆工作，推动海昏侯国遗址从考古现场走向公众。"),
    ("20_curator_detail.wav", "他的策展思路把博物馆、遗址本体与周边环境连成整体，让观众在原址附近理解文物从何而来。"),
    ("21_location_story.wav", "博物馆位于南昌市新建区大塘坪乡。地图搜索南昌汉代海昏侯国遗址博物馆，即可导航到国家考古遗址公园。"),
    ("22_location_detail.wav", "常规开放时间为上午九点到下午五点，下午四点停止入馆。预约、票务和临时闭馆信息，请以出发前官方公告为准。"),
)


def synthesize() -> None:
    if not shutil.which("say") or not shutil.which("ffmpeg"):
        raise SystemExit("macOS say and FFmpeg are required to regenerate TTS WAV files")
    WAV_DIR.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="haihunhou-tts-") as temporary:
        temporary_path = Path(temporary)
        for filename, text in SCRIPTS:
            aiff = temporary_path / f"{Path(filename).stem}.aiff"
            wav = WAV_DIR / filename
            subprocess.run(
                ["say", "-v", "Tingting", "-r", "180", "-o", str(aiff), text],
                check=True,
            )
            subprocess.run(
                [
                    "ffmpeg", "-y", "-hide_banner", "-loglevel", "error",
                    "-i", str(aiff), "-ar", "16000", "-ac", "1",
                    "-sample_fmt", "s16", str(wav),
                ],
                check=True,
            )


def read_clip(path: Path) -> tuple[bytes, int, int, int]:
    with wave.open(str(path), "rb") as wav:
        if wav.getnchannels() != 1 or wav.getsampwidth() != 2 or wav.getframerate() != 16000:
            raise SystemExit(f"{path}: expected 16 kHz, 16-bit, mono PCM WAV")
        frames = wav.readframes(wav.getnframes())
    samples = struct.unpack(f"<{len(frames) // 2}h", frames)
    data, predictor, index = encode(samples)
    return data, len(samples), predictor, index


def pack() -> None:
    clips = [read_clip(WAV_DIR / filename) for filename, _ in SCRIPTS]
    packed = b"".join(clip[0] for clip in clips)
    lines = [
        '#include "haihunhou_tts_audio.h"',
        "",
        "const uint8_t haihunhou_tts_audio_data[] = {",
    ]
    for offset in range(0, len(packed), 16):
        row = ", ".join(f"0x{byte:02x}" for byte in packed[offset : offset + 16])
        lines.append(f"    {row},")
    lines.extend([
        "};",
        f"const size_t haihunhou_tts_audio_data_size = {len(packed)};",
        "",
        "const haihunhou_tts_clip_t haihunhou_tts_clips[] = {",
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
        f"const size_t haihunhou_tts_clip_count = {len(clips)};",
        "",
    ])
    OUTPUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {len(clips)} clips, {len(packed)} ADPCM bytes to {OUTPUT}")


if __name__ == "__main__":
    synthesize()
    pack()
