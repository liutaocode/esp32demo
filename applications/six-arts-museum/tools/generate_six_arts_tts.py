#!/usr/bin/env python3
"""Generate and pack the twenty-two Six Arts Museum narration clips."""

from __future__ import annotations

import shutil
import struct
import subprocess
import tempfile
import wave
from pathlib import Path

from ima_adpcm_encode import encode


ROOT = Path(__file__).resolve().parents[1]
WAV_DIR = ROOT / "assets/music/six_arts_tts"
OUTPUT = ROOT / "main/six_arts_tts_audio.c"

SCRIPTS = (
    (
        "01_museum_story.wav",
        "欢迎来到六悦博物馆。这里有四层展厅、六十多个展馆，陈列四万多件民间艺术品。让我们从日常老物件出发，看见无名匠人的心血。",
    ),
    (
        "02_museum_detail.wav",
        "六悦之名，取意眼、耳、鼻、舌、身、心，六种感官皆悦。馆内采用开放式陈列，让传统艺术走近今天的生活。",
    ),
    (
        "03_queti_story.wav",
        "现在看到的是狮子雀替。雀替位于立柱和横梁之间，兼顾承重与装饰。狮子题材象征守护和吉祥，也凝结着木匠的精细技艺。",
    ),
    (
        "04_queti_detail.wav",
        "请仔细观察雀替的层层雕刻。它既加固建筑，又装点屋檐下的空间，是理解中国传统木构建筑细节的一把钥匙。",
    ),
    (
        "05_bed_story.wav",
        "这是一架多进拔步床。它像一间缩小的房屋，床前层层围合，在过去兼顾睡眠、更衣、饮食和私密生活。",
    ),
    (
        "06_bed_detail.wav",
        "据报道，六悦的床榻馆收藏四十六件古床，这架多进明式拔步床被称为镇馆之宝。请留意它复杂的空间层次和木作结构。",
    ),
    (
        "07_doors_story.wav",
        "眼前是门神彩绘门。成对门神以浓烈色彩守护入口。古门不仅分隔内外，也把家族的愿望、身份和礼俗画在门面上。",
    ),
    (
        "08_doors_detail.wav",
        "五楼展线串起门神、药柜、书柜和古床。观看彩绘门时，可以留意人物的服饰、兵器、姿态和神情。",
    ),
    (
        "09_window_story.wav",
        "这里展示明清花窗。几何格栅与木雕纹样，把照进室内的光影也变成装饰，是六悦博物馆最醒目的收藏类别之一。",
    ),
    (
        "10_window_detail.wav",
        "一扇花窗既负责通风采光，也借不同纹样表达祝愿。馆方介绍，这里收藏了规模可观的明清雕花窗。",
    ),
    (
        "11_sedan_story.wav",
        "这件神轿服务于迎神赛会等民间仪式。繁密雕刻和鲜艳彩绘，把无形的信仰变成一座可以移动的节庆舞台。",
    ),
    (
        "12_sedan_detail.wav",
        "三楼还陈列神龛与漆画。把神轿放回当年的仪式场景，才能读懂它与地方社区、共同记忆之间的联系。",
    ),
    (
        "13_plaque_story.wav",
        "这方达尊有二匾，制作于清光绪三年，也就是公元一八七七年，用来祝贺一位八旬长者。一个二字，留下了谦逊的余地。",
    ),
    (
        "14_plaque_detail.wav",
        "达尊原指爵、齿、德三种尊荣。匾文只说有二，不把三种荣誉全部占尽。短短四个字，记录了乡里礼俗与说话的分寸。",
    ),
    (
        "15_buddha_story.wav",
        "来到万佛石窟，古代石雕造像汇成一面佛墙，并随六种色彩变换光影。这是走进六悦博物馆最震撼的场景之一。",
    ),
    (
        "16_buddha_detail.wav",
        "馆方把万佛石窟称为对龙门石窟的艺术化想象。原本散落的石雕，在新的展陈空间里重新汇聚成群像。",
    ),
    (
        "17_founder_story.wav",
        "这是创办人杜维明，英文名米切尔杜德克。他在一九八一年初到中国，从一件玉石狮子开启收藏。二零一八年，六悦博物馆在黎里开放。",
    ),
    (
        "18_founder_detail.wav",
        "四十多年间，杜维明在城市和乡村的变迁中保存民间器物。他希望年轻一代仍能看见传统工匠的心血，以及过去真实的生活。",
    ),
    (
        "19_curator_story.wav",
        "现在介绍馆长陈杰。他负责藏品设计与陈列，用鲜亮色彩衬托斑驳老物，让传统不再显得遥远和沉闷。",
    ),
    (
        "20_curator_detail.wav",
        "修复团队遵循少改变、少修理的原则，尽量保留岁月痕迹。开放式布展也拉近了观众与器物之间的距离。",
    ),
    (
        "21_location_story.wav",
        "六悦博物馆位于苏州市吴江区黎里镇人民东路一号。当前公布的开放时间是每天上午九点到下午六点，出发前请再次确认。",
    ),
    (
        "22_location_detail.wav",
        "地图搜索六悦博物馆，即可导航前往黎里古镇。馆方电话是，零五一二，六三九五五三八八。请提前确认开放、票务和交通安排。",
    ),
)


def synthesize() -> None:
    if not shutil.which("say") or not shutil.which("ffmpeg"):
        raise SystemExit("macOS say and FFmpeg are required to regenerate TTS WAV files")
    WAV_DIR.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="six-arts-tts-") as temporary:
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
        '#include "six_arts_tts_audio.h"',
        "",
        "const uint8_t six_arts_tts_audio_data[] = {",
    ]
    for offset in range(0, len(packed), 16):
        row = ", ".join(f"0x{byte:02x}" for byte in packed[offset : offset + 16])
        lines.append(f"    {row},")
    lines.extend([
        "};",
        f"const size_t six_arts_tts_audio_data_size = {len(packed)};",
        "",
        "const six_arts_tts_clip_t six_arts_tts_clips[] = {",
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
        f"const size_t six_arts_tts_clip_count = {len(clips)};",
        "",
    ])
    OUTPUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {len(clips)} clips, {len(packed)} ADPCM bytes to {OUTPUT}")


if __name__ == "__main__":
    synthesize()
    pack()
