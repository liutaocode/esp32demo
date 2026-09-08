#!/usr/bin/env python3
"""Generate and pack the twenty Suzhou guide narration clips."""

from __future__ import annotations

import shutil
import struct
import subprocess
import tempfile
import wave
from pathlib import Path

from ima_adpcm_encode import encode


ROOT = Path(__file__).resolve().parents[1]
WAV_DIR = ROOT / "assets/music/suzhou_tts"
OUTPUT = ROOT / "main/suzhou_tts_audio.c"

SCRIPTS = (
    ("01_zhuozhengyuan_intro.wav", "拙政园。水景开阔，移步换景。苏州古典园林的代表，池水、曲桥、亭榭与花木相映，四季皆有不同风致。"),
    ("02_zhuozhengyuan_detail.wav", "拙政园游览看点。远香堂、小飞虹、梧竹幽居。清晨入园，更适合慢看水面与借景。"),
    ("03_liuyuan_intro.wav", "留园。厅堂精雅，奇石入画。以建筑空间和庭院层次见长，长廊串联山水，转折之间常有意外景致。"),
    ("04_liuyuan_detail.wav", "留园游览看点。冠云峰、五峰仙馆与曲廊花窗。可以留心门洞如何框出一幅幅小景。"),
    ("05_huqiu_intro.wav", "虎丘。千年斜塔，吴中胜境。山虽不高，却集古塔、剑池与石刻于一处，是认识苏州历史脉络的经典去处。"),
    ("06_huqiu_detail.wav", "虎丘游览看点。云岩寺塔、千人石、剑池。山路多石阶，建议穿轻便防滑的鞋。"),
    ("07_hanshansi_intro.wav", "寒山寺。枫桥钟声，诗意江南。因枫桥夜泊而广为人知，寺院、古钟与运河古桥共同构成悠远的人文意境。"),
    ("08_hanshansi_detail.wav", "寒山寺游览看点。钟楼、碑廊与邻近枫桥。适合与虎丘安排在同一天缓步游览。"),
    ("09_pingjianglu_intro.wav", "平江路。河街相邻，小桥流水。沿河老街保留水巷格局，白墙黛瓦、石桥、小店与评弹茶馆相互交织。"),
    ("10_pingjianglu_detail.wav", "平江路游览看点。支巷、古桥与临水人家。离开主街走进小巷，更能感受日常苏州。"),
    ("11_shantangjie_intro.wav", "山塘街。七里水巷，灯影人家。古街沿山塘河展开，桥、埠、民居相连，傍晚灯火映水，最有水乡气韵。"),
    ("12_shantangjie_detail.wav", "山塘街游览看点。古石桥、河埠与临水街屋。夜景热闹，想安静拍照可以选择上午。"),
    ("13_shizilin_intro.wav", "狮子林。假山迷宫，石峰如狮。以层叠太湖石假山著称，洞壑、石峰与回廊交错，像穿行山水迷宫。"),
    ("14_shizilin_detail.wav", "狮子林游览看点。假山群、燕誉堂、真趣亭。岔路较多，慢走才能体会峰回路转。"),
    ("15_wangshiyuan_intro.wav", "网师园。小园极致，夜色入戏。面积不大却比例精巧，池水居中，建筑环绕，在有限空间里营造深远层次。"),
    ("16_wangshiyuan_detail.wav", "网师园游览看点。月到风来亭、濯缨水阁。观察窗、廊、池之间彼此借景的关系。"),
    ("17_canglangting_intro.wav", "沧浪亭。古园临水，清幽疏朗。它是苏州现存历史悠久的园林之一，未入园先见水，复廊连接园内外景色。"),
    ("18_canglangting_detail.wav", "沧浪亭游览看点。临水复廊、漏窗、翠玲珑。环境清雅，适合静看竹影与光线变化。"),
    ("19_jinjihu_intro.wav", "金鸡湖。古今相映，湖畔新城。开阔湖面与现代城市天际线相映，展现古典园林之外的另一种苏州风景。"),
    ("20_jinjihu_detail.wav", "金鸡湖游览看点。湖滨步道、日落与夜景。湖面风大，傍晚游览可以多带一件外衣。"),
)


def synthesize() -> None:
    if not shutil.which("say") or not shutil.which("ffmpeg"):
        raise SystemExit("macOS say and FFmpeg are required to regenerate TTS WAV files")
    WAV_DIR.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="suzhou-tts-") as temporary:
        temporary_path = Path(temporary)
        for filename, text in SCRIPTS:
            aiff = temporary_path / f"{Path(filename).stem}.aiff"
            wav = WAV_DIR / filename
            subprocess.run(
                ["say", "-v", "Tingting", "-r", "185", "-o", str(aiff), text],
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
        '#include "suzhou_tts_audio.h"',
        "",
        "const uint8_t suzhou_tts_audio_data[] = {",
    ]
    for offset in range(0, len(packed), 16):
        row = ", ".join(f"0x{byte:02x}" for byte in packed[offset : offset + 16])
        lines.append(f"    {row},")
    lines.extend([
        "};",
        f"const size_t suzhou_tts_audio_data_size = {len(packed)};",
        "",
        "const suzhou_tts_clip_t suzhou_tts_clips[] = {",
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
        f"const size_t suzhou_tts_clip_count = {len(clips)};",
        "",
    ])
    OUTPUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {len(clips)} clips, {len(packed)} ADPCM bytes to {OUTPUT}")


if __name__ == "__main__":
    synthesize()
    pack()
