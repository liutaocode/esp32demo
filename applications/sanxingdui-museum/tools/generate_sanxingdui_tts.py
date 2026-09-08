#!/usr/bin/env python3
"""Generate and pack the twenty-two Sanxingdui Museum narration clips."""

from __future__ import annotations

import shutil
import struct
import subprocess
import tempfile
import wave
from pathlib import Path

from ima_adpcm_encode import encode


ROOT = Path(__file__).resolve().parents[1]
WAV_DIR = ROOT / "assets/music/sanxingdui_tts"
OUTPUT = ROOT / "main/sanxingdui_tts_audio.c"

SCRIPTS = (
    ("01_site_story.wav", "欢迎来到三星堆博物馆。神树、面具、金杖与人像从祭祀坑中醒来。让我们用十一站，走进想象奇绝的古蜀世界。"),
    ("02_site_detail.wav", "新馆以世纪逐梦、巍然王都、天地人神展开叙事，展出一千五百余件套文物。"),
    ("03_standing_story.wav", "高冠长袍的青铜大立人站在方座上，双手环握却留下空处。那件消失的物品，仍等待新的答案。"),
    ("04_standing_detail.wav", "人像本体高约一点七二米，连座通高约二点六二米。夸张双手、层叠礼服与纹饰共同显示庄严身份。"),
    ("05_tree_story.wav", "一号青铜神树枝干分层向上，鸟立花果之间，神龙沿树身蜿蜒。它把古蜀人的宇宙想象铸成立体图景。"),
    ("06_tree_detail.wav", "现存树体由多段残件修复而成。树枝、果实、神鸟与龙彼此呼应，常被联系到通天与太阳崇拜。"),
    ("07_eye_mask_story.wav", "青铜纵目面具巨耳展开，眼睛像望远镜般向前伸出。超越常人的五官，也许在表现能通达天地的神性。"),
    ("08_eye_mask_detail.wav", "面具宽约一点三八米。关于它代表神、祖先还是传说人物，学界仍有讨论，想象不等于定论。"),
    ("09_staff_story.wav", "金杖表面刻有人头像、鱼、鸟与箭。连续图像像一段无声叙事，凝聚着古蜀王权与信仰。"),
    ("10_staff_detail.wav", "出土时内部木芯已朽，仅留卷成杖形的金皮。纹样含义尚有多种解释，细看可辨对称布局。"),
    ("11_gold_mask_story.wav", "宽阔眉眼、挺直鼻梁与大耳被薄金塑出。金面具既延续三星堆面具传统，也把金色带入祭祀世界。"),
    ("12_gold_mask_detail.wav", "面具出土时残缺，考古人员依据折痕与结构展开保护。它如何佩戴、代表谁，仍需更多证据。"),
    ("13_kneeling_story.wav", "青铜人像双手扶住头顶大尊，身体、衣饰与容器连成复杂整体，像把一场仪式凝固在青铜中。"),
    ("14_kneeling_detail.wav", "它由多个部件组合，出土后经过细致拼对修复。人物动作提示承托与奉献，却不能简单还原仪式。"),
    ("15_lattice_story.wav", "龟背形网格状器的椭圆网格像龟背，内部包着整块玉石。这件前所未见的器物，成为新一轮考古的代表发现。"),
    ("16_lattice_detail.wav", "它的名称来自外形，并不等于用途结论。铜、玉如何组合，怎样使用，仍是研究中的开放问题。"),
    ("17_zhang_story.wav", "扁长的祭山图玉璋上刻着人物、山形与礼仪图像。细线虽小，却像一幅浓缩的古蜀仪式长卷。"),
    ("18_zhang_detail.wav", "祭山图是依据图像所作的命名。人物姿态和符号如何解读仍有争议，适合带着问题观看。"),
    ("19_curator_story.wav", "馆长雷雨长期参与三星堆考古与研究，从发掘现场到博物馆叙事，持续回答古蜀文明的新问题。"),
    ("20_curator_detail.wav", "他强调用考古材料理解中华文明多元一体。新发现不断出现，展览也会随着研究更新。"),
    ("21_location_story.wav", "三星堆博物馆位于四川省广汉市向新路一百三十三号。地图搜索三星堆博物馆即可导航。"),
    ("22_location_detail.wav", "常规开放时间为上午八点半到下午六点，下午五点停止入馆。建议通过官方渠道预约，节假日与暑期请以最新公告为准。"),
)


def synthesize() -> None:
    if not shutil.which("say") or not shutil.which("ffmpeg"):
        raise SystemExit("macOS say and FFmpeg are required to regenerate TTS WAV files")
    WAV_DIR.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="sanxingdui-tts-") as temporary:
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
        '#include "sanxingdui_tts_audio.h"',
        "",
        "const uint8_t sanxingdui_tts_audio_data[] = {",
    ]
    for offset in range(0, len(packed), 16):
        row = ", ".join(f"0x{byte:02x}" for byte in packed[offset : offset + 16])
        lines.append(f"    {row},")
    lines.extend([
        "};",
        f"const size_t sanxingdui_tts_audio_data_size = {len(packed)};",
        "",
        "const sanxingdui_tts_clip_t sanxingdui_tts_clips[] = {",
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
        f"const size_t sanxingdui_tts_clip_count = {len(clips)};",
        "",
    ])
    OUTPUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {len(clips)} clips, {len(packed)} ADPCM bytes to {OUTPUT}")


if __name__ == "__main__":
    synthesize()
    pack()
